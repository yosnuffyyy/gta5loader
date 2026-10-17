#include "uav_layout.h"
static Object *uav_object(NativeGpu *g,uint32_t id){Object *v=object(g,id);if(!check(g,v&&v->kind==37&&v->words==9&&v->meta[1]<=1,"UAV descriptor missing or invalid"))return NULL;Object *o=srv_source(g,v);if(!o)return NULL;if(v->meta[1]&&!check(g,!(v->meta[8]&~7u)&&((v->meta[8]&6)!=6)&&!((v->meta[8]&1)&&(v->meta[8]&6)),"Invalid combined raw/append/counter UAV flags"))return NULL;return v;}
static WGPUBuffer uav_counter(NativeGpu *g,Object *v){
 if(!check(g,uav_hidden_counter(v->meta[1],v->meta[8]),"Counter shader binding requires an append/counter UAV"))return NULL;
 if(!v->counter){WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=16;d.usage=WGPUBufferUsage_Storage|WGPUBufferUsage_CopySrc|WGPUBufferUsage_CopyDst;v->counter=wgpuDeviceCreateBuffer(g->device,&d);}return v->counter;
}
static bool uav_set(NativeGpu *g,const uint8_t *p,uint32_t n){if(!check(g,n>=3,"Truncated UAV state payload"))return false;uint32_t stage=gpu_word(p,0),first=gpu_word(p,1),count=gpu_word(p,2);
 if(!check(g,stage<3&&first<=8&&count<=8-first&&n==3+2*count,"Invalid UAV binding/count range"))return false;
 for(unsigned i=0;i<count;i++){uint32_t id=gpu_word(p,3+i),value=gpu_word(p,3+count+i);Object *v=id?uav_object(g,id):NULL;if(id&&!v)return false;g->uav[stage][first+i]=id;
  if(g->host->progress&&g->uav_logs<12){char line[240];snprintf(line,sizeof(line),"Native UAV: stage %u slot %u view %u resource %u kind %u flags 0x%x initial %u%s",stage,first+i,id,v?v->meta[2]:0,v?v->meta[1]:0,v?v->meta[8]:0,value,value==UINT32_MAX?" (preserve)":v&&uav_hidden_counter(v->meta[1],v->meta[8])?" (counter reset)":" (not applicable by D3D API)");g->host->progress(g->host->progress_context,line);g->uav_logs++;}
  if(value==UINT32_MAX)continue;if(!v||!uav_hidden_counter(v->meta[1],v->meta[8])){g->draw.counter_ignored++;continue;}
  WGPUBuffer counter=uav_counter(g,v);if(!counter)return false;if(!g->counter_source){WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=4096;d.usage=WGPUBufferUsage_CopySrc|WGPUBufferUsage_CopyDst;g->counter_source=wgpuDeviceCreateBuffer(g->device,&d);if(!g->counter_source)return false;}
  /* Unique staging words until submit: a later reset cannot overwrite the
     value of a preceding pending dispatch. Queue order permits reuse only
     after that encoder has been submitted, without a GPU completion wait. */
  if(g->counter_slot==1024&&!draw_flush(g))return false;draw_end_pass(g);if(!g->draw.encoder)g->draw.encoder=encoder(g);if(!g->draw.encoder)return false;
  unsigned at=g->counter_slot++*4;wgpuQueueWriteBuffer(g->queue,g->counter_source,at,&value,4);wgpuCommandEncoderCopyBufferToBuffer(g->draw.encoder,g->counter_source,at,counter,0,4);g->draw.counter_resets++;
 }return !atomic_load(&g->failed);
}
static bool uav_copy_counter(NativeGpu *g,const uint8_t *p){Object *dst=object(g,gpu_word(p,0)),*v=uav_object(g,gpu_word(p,2));uint32_t at=gpu_word(p,1);if(!v)return false;
 if(!check(g,dst&&dst->buffer&&at%4==0&&at<=dst->size&&dst->size-at>=4,"Counter copy destination offset/buffer invalid"))return false;WGPUBuffer src=uav_counter(g,v);if(!src)return false;draw_end_pass(g);if(!g->draw.encoder)g->draw.encoder=encoder(g);if(!g->draw.encoder)return false;draw_buffer_used(g,dst);wgpuCommandEncoderCopyBufferToBuffer(g->draw.encoder,src,0,dst->buffer,at,4);g->draw.counter_copies++;return !atomic_load(&g->failed);
}
static bool storage_range(NativeGpu *g,Object *v,Object *o,uint64_t *offset,uint64_t *bytes){unsigned stride=o->stride?o->stride:4;
 if(!check(g,(v->kind==17||v->kind==37)&&v->words==9&&v->meta[1]==1&&o->buffer,"Storage buffer view descriptor invalid"))return false;
 if(v->meta[3]){WGPUTextureFormat fmt=format(v->meta[3],0);for(unsigned i=0;i<sizeof(storage_formats)/sizeof(storage_formats[0]);i++)if(fmt==storage_formats[i].format)stride=storage_formats[i].channels*storage_formats[i].bits/8;}
 if(v->kind==37&&(v->meta[8]&1))stride=4;
 return check(g,uav_buffer_range(o->size,stride,v->meta[5],v->meta[6],offset,bytes)&&*offset%4==0&&*bytes%4==0,"Storage view range invalid or not DWORD aligned");
}
static WGPUBuffer storage_buffer(NativeGpu *g,DrawBinding *b,uint32_t id,uint64_t *offset,uint64_t *bytes){Object *v=object(g,id);*offset=0;*bytes=65536;
 if(!v){if(!check(g,!id,"Nonzero buffer SRV/UAV handle is missing"))return NULL;if(!b->dummy_buffer){WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=65536;d.usage=WGPUBufferUsage_Storage|WGPUBufferUsage_CopyDst;b->dummy_buffer=wgpuDeviceCreateBuffer(g->device,&d);}return b->dummy_buffer;}
 Object *o=srv_source(g,v);if(!o||!check(g,o->buffer!=NULL,"Storage view has no GPU buffer"))return NULL;
 if(b->binding>=176){*bytes=16;return uav_counter(g,v);}
 if(!storage_range(g,v,o,offset,bytes))return NULL;if(!g->storage_alignment||*offset%g->storage_alignment==0)return o->buffer;
 if(!v->storage_proxy){if(!check(g,*bytes<=64*1024*1024&&g->draw.storage_proxy_bytes<=128*1024*1024-*bytes,"Unaligned storage view exceeds its 64 MiB / total 128 MiB proxy bound"))return NULL;WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=*bytes;d.usage=WGPUBufferUsage_Storage|WGPUBufferUsage_CopySrc|WGPUBufferUsage_CopyDst;v->storage_proxy=wgpuDeviceCreateBuffer(g->device,&d);if(!v->storage_proxy)return NULL;v->storage_owner=g;v->storage_proxy_size=*bytes;g->draw.storage_proxy_bytes+=*bytes;if(g->draw.storage_proxy_bytes>g->draw.storage_proxy_peak)g->draw.storage_proxy_peak=g->draw.storage_proxy_bytes;}
 *offset=0;return v->storage_proxy;
}
static WGPUTextureView storage_texture(NativeGpu *g,DrawBinding *b,uint32_t id){if(b->view&&b->view_id==id&&b->view_epoch==g->draw.resource_epoch)return b->view;if(b->view){wgpuTextureViewRelease(b->view);b->view=NULL;}
 Object *v=uav_object(g,id);if(!v||!check(g,!v->meta[1],"Storage texture binding has a buffer UAV"))return NULL;Object *o=object(g,v->meta[2]);if(!check(g,o->td.format==b->format&&o->td.sampleCount==1&&v->meta[5]<o->td.mipLevelCount,"Storage texture format/sample/mip mismatch"))return NULL;
 WGPUTextureViewDescriptor d=WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;d.dimension=b->dim;d.baseMipLevel=v->meta[5];d.mipLevelCount=1;
 if(b->dim!=WGPUTextureViewDimension_3D){d.baseArrayLayer=v->meta[4]==5?v->meta[6]:0;d.arrayLayerCount=b->dim==WGPUTextureViewDimension_2DArray?(v->meta[4]==5?v->meta[7]:o->td.size.depthOrArrayLayers):1;if(!check(g,d.arrayLayerCount&&d.baseArrayLayer<o->td.size.depthOrArrayLayers&&d.arrayLayerCount<=o->td.size.depthOrArrayLayers-d.baseArrayLayer,"Storage texture layer range invalid"))return NULL;}
 else if(!check(g,v->meta[6]==0&&(!v->meta[7]||v->meta[7]==UINT32_MAX||v->meta[7]==mip_extent(o->td.size.depthOrArrayLayers,d.baseMipLevel)),"Partial 3D UAV slices are not yet implemented"))return NULL;
 b->view=wgpuTextureCreateView(o->texture,&d);b->view_id=id;b->view_epoch=g->draw.resource_epoch;return b->view;
}
static uint32_t storage_binding_id(NativeGpu *g,DrawProgram *p,DrawBinding *b){return b->binding>=160?g->uav[p->stage][b->slot]:g->srv[p->stage][b->slot];}
static bool storage_prepare(NativeGpu *g,DrawProgram *p){for(unsigned i=0;i<p->count;i++){DrawBinding *b=p->bindings+i;if(b->kind!=5&&b->kind!=6)continue;uint32_t id=storage_binding_id(g,p,b);uint64_t at,bytes;WGPUBuffer buffer=storage_buffer(g,b,id,&at,&bytes);if(!buffer)return false;Object *v=object(g,id);
 if(!v&&b->kind==6){draw_end_pass(g);if(!g->draw.encoder)g->draw.encoder=encoder(g);if(!g->draw.encoder)return false;wgpuCommandEncoderClearBuffer(g->draw.encoder,buffer,0,bytes);}
 else if(v&&v->storage_proxy&&b->binding<176){Object *o=object(g,v->meta[2]);if(!storage_range(g,v,o,&at,&bytes))return false;draw_end_pass(g);if(!g->draw.encoder)g->draw.encoder=encoder(g);if(!g->draw.encoder)return false;wgpuCommandEncoderCopyBufferToBuffer(g->draw.encoder,o->buffer,at,buffer,0,bytes);g->draw.storage_proxy_copies++;}
 }return !atomic_load(&g->failed);}
static bool storage_written(NativeGpu *g,DrawProgram *p){for(unsigned i=0;i<p->count;i++){DrawBinding *b=p->bindings+i;if(b->kind!=6&&b->kind!=7)continue;Object *v=object(g,storage_binding_id(g,p,b));if(!v)continue;Object *o=object(g,v->meta[2]);if(b->kind==7&&b->access!=WGPUStorageTextureAccess_ReadOnly)proxy_dirty(o);
 if(b->kind==6&&b->binding<176&&v->storage_proxy){uint64_t at,bytes;if(!storage_range(g,v,o,&at,&bytes))return false;draw_end_pass(g);wgpuCommandEncoderCopyBufferToBuffer(g->draw.encoder,v->storage_proxy,0,o->buffer,at,bytes);g->draw.storage_proxy_copies++;}
 }return !atomic_load(&g->failed);}
