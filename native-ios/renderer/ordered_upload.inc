/* Immutable batch-local CPU upload bytes. QueueWriteBuffer fills the staging
   buffer before submitting this encoder; ordered copies then execute between
   the exact draws which surround each mutation. This preserves GPU-written
   portions of the destination and never clones a stale CPU resource mirror. */
enum { ORDERED_UPLOAD_BYTES=512*1024, ORDERED_UPLOAD_SLOTS=2 };
static bool ordered_upload_eligible(NativeGpu *g,const uint8_t *p,uint32_t n){
#ifdef NATIVE_PROBE_CLI
 if(g->test_legacy_ordered_upload)return false;
#endif
 if(n<3||!g->draw.encoder)return false;
 Object *o=object(g,gpu_word(p,0));uint32_t at=gpu_word(p,1),bytes=gpu_word(p,2);
 /* Keep 0.35's direct queue-write path for independent destinations. Only a
    buffer already referenced by this pending encoder needs an ordered copy. */
 return o&&o->kind==11&&o->buffer&&g->draw.batches!=UINT64_MAX&&o->draw_buffer_epoch==g->draw.batches+1&&bytes&&bytes<=ORDERED_UPLOAD_BYTES&&!(at&3)&&!(bytes&3)&&at<=o->size&&bytes<=o->size-at&&((uint64_t)bytes+3)/4<=n-3;
}
static bool ordered_upload(NativeGpu *g,const uint8_t *p){
 Object *o=object(g,gpu_word(p,0));unsigned at=gpu_word(p,1),bytes=gpu_word(p,2);
 if(!g->ordered_mirror){g->ordered_mirror=malloc(ORDERED_UPLOAD_BYTES);if(!check(g,g->ordered_mirror!=NULL,"Ordered upload mirror allocation failed"))return false;
  for(unsigned i=0;i<ORDERED_UPLOAD_SLOTS;i++){WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=ORDERED_UPLOAD_BYTES;d.usage=WGPUBufferUsage_CopySrc|WGPUBufferUsage_CopyDst;g->ordered_buffers[i]=wgpuDeviceCreateBuffer(g->device,&d);if(!check(g,g->ordered_buffers[i]!=NULL,"Ordered upload arena allocation failed"))return false;}
 }
 if(bytes>ORDERED_UPLOAD_BYTES-g->ordered_offset){g->ordered_capacity_flushes++;if(!draw_flush(g))return false;}
 draw_end_pass(g);if(!g->draw.encoder)g->draw.encoder=encoder(g);if(!g->draw.encoder)return false;
 unsigned offset=g->ordered_offset;memcpy(g->ordered_mirror+offset,p+12,bytes);g->ordered_offset+=bytes;
 wgpuCommandEncoderCopyBufferToBuffer(g->draw.encoder,g->ordered_buffers[g->ordered_slot],offset,o->buffer,at,bytes);
 /* Prevent a subsequent unaligned/direct queue update from overtaking this
    encoded write even if no intervening draw uses the destination. */
 draw_buffer_used(g,o);
 if(o->shadow)memcpy(o->shadow+at,p+12,bytes);
 g->ordered_uploads++;g->ordered_bytes+=bytes;g->uploads++;return !atomic_load(&g->failed);
}
