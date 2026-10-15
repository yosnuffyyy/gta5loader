/* Raw copies preserve float/packed/signed/compressed bits. One reusable,
   synchronously mapped buffer; never more than 1 MiB of staging storage. */
static bool readback_format(WGPUTextureFormat f,uint32_t *bw,uint32_t *bh,uint32_t *bytes){
 *bw=*bh=1;
 switch(f){
 case WGPUTextureFormat_R8Uint:case WGPUTextureFormat_R8Sint:case WGPUTextureFormat_R8Unorm:case WGPUTextureFormat_R8Snorm:*bytes=1;break;
 case WGPUTextureFormat_R16Unorm:case WGPUTextureFormat_R16Snorm:case WGPUTextureFormat_RG8Uint:case WGPUTextureFormat_RG8Sint:case WGPUTextureFormat_R16Uint:case WGPUTextureFormat_R16Sint:case WGPUTextureFormat_RG8Unorm:case WGPUTextureFormat_RG8Snorm:case WGPUTextureFormat_R16Float:*bytes=2;break;
 case WGPUTextureFormat_RG16Unorm:case WGPUTextureFormat_RG16Snorm:case WGPUTextureFormat_RGBA8Uint:case WGPUTextureFormat_RGBA8Sint:case WGPUTextureFormat_RG16Uint:case WGPUTextureFormat_RG16Sint:case WGPUTextureFormat_R32Uint:case WGPUTextureFormat_R32Sint:case WGPUTextureFormat_RGBA8Unorm:case WGPUTextureFormat_RGBA8UnormSrgb:case WGPUTextureFormat_RGBA8Snorm:case WGPUTextureFormat_BGRA8Unorm:case WGPUTextureFormat_BGRA8UnormSrgb:
 case WGPUTextureFormat_RG16Float:case WGPUTextureFormat_R32Float:case WGPUTextureFormat_RGB10A2Unorm:case WGPUTextureFormat_RG11B10Ufloat:case WGPUTextureFormat_RGB9E5Ufloat:*bytes=4;break;
 case WGPUTextureFormat_RGBA16Unorm:case WGPUTextureFormat_RGBA16Snorm:case WGPUTextureFormat_RGBA16Uint:case WGPUTextureFormat_RGBA16Sint:case WGPUTextureFormat_RG32Uint:case WGPUTextureFormat_RG32Sint:case WGPUTextureFormat_RGBA16Float:case WGPUTextureFormat_RG32Float:*bytes=8;break;
 case WGPUTextureFormat_RGBA32Uint:case WGPUTextureFormat_RGBA32Sint:case WGPUTextureFormat_RGBA32Float:*bytes=16;break;
 case WGPUTextureFormat_BC1RGBAUnorm:case WGPUTextureFormat_BC1RGBAUnormSrgb:case WGPUTextureFormat_BC4RUnorm:*bw=*bh=4;*bytes=8;break;
 case WGPUTextureFormat_BC2RGBAUnorm:case WGPUTextureFormat_BC2RGBAUnormSrgb:case WGPUTextureFormat_BC3RGBAUnorm:case WGPUTextureFormat_BC3RGBAUnormSrgb:
 case WGPUTextureFormat_BC5RGUnorm:case WGPUTextureFormat_BC6HRGBUfloat:case WGPUTextureFormat_BC7RGBAUnorm:case WGPUTextureFormat_BC7RGBAUnormSrgb:*bw=*bh=4;*bytes=16;break;
 default:return false;
 }return true;
}
static bool readback_buffer(NativeGpu *g,uint32_t bytes){
 uint32_t size=256;while(size<bytes&&size<READBACK_STAGING_MAX)size*=2;
 if(!check(g,bytes&&bytes<=size&&size<=READBACK_STAGING_MAX,"Readback staging bound exceeded"))return false;
 if(g->readback_buffer&&g->readback_capacity>=size)return true;
 if(g->readback_buffer){wgpuBufferDestroy(g->readback_buffer);wgpuBufferRelease(g->readback_buffer);g->readback_buffer=NULL;g->readback_capacity=0;}
 WGPUBufferDescriptor bd=WGPU_BUFFER_DESCRIPTOR_INIT;bd.size=size;bd.usage=WGPUBufferUsage_MapRead|WGPUBufferUsage_CopyDst;
 g->readback_buffer=wgpuDeviceCreateBuffer(g->device,&bd);if(!check(g,g->readback_buffer!=NULL,"Readback staging allocation"))return false;
 g->readback_capacity=size;g->readback_allocations++;return !atomic_load(&g->failed);
}
static const uint8_t *readback_map(NativeGpu *g,WGPUCommandEncoder e,uint32_t bytes){
 uint64_t start=now_ns();if(!submit(g,e))return NULL;
 atomic_store(&g->callback_done,false);atomic_store(&g->map_active,true);
 WGPUBufferMapCallbackInfo cb=WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;cb.mode=WGPUCallbackMode_AllowProcessEvents;cb.callback=mapped;cb.userdata1=g;
 uint32_t phase=atomic_exchange(&g->live_phase,6);wgpuBufferMapAsync(g->readback_buffer,WGPUMapMode_Read,0,bytes,cb);bool ok=wait_callback(g);uint64_t elapsed=now_ns()-start;timing_add(&g->metrics.windows[METRIC_READBACK],elapsed);atomic_store(&g->live_phase,phase);if(g->host->progress&&g->metrics.readback_logs<16&&(g->metrics.windows[METRIC_READBACK].count<=8||elapsed>50000000ull)){char line[320];snprintf(line,sizeof(line),"Readback map %llu: opcode %s source %u sub %u, staging %u bytes, submit-to-map %.3f ms, cumulative module/pipeline CPU %.3f/%.3f ms, success %u. Earlier ring/engine wait is excluded.",(unsigned long long)g->metrics.windows[METRIC_READBACK].count,gpu_opcode_name(g->last_op),g->last_payload[0],g->last_payload[1],bytes,elapsed/1e6,g->metrics.windows[METRIC_SHADER].total_ns/1e6,g->metrics.windows[METRIC_PIPELINE].total_ns/1e6,ok);g->host->progress(g->host->progress_context,line);g->metrics.readback_logs++;}if(!ok)return NULL;
 const uint8_t *data=wgpuBufferGetConstMappedRange(g->readback_buffer,0,bytes);if(!check(g,data!=NULL,"Readback mapped range unavailable"))return NULL;uniform_retired(g,true);return data;
}
static void readback_unmap(NativeGpu *g){if(atomic_exchange(&g->map_active,false))wgpuBufferUnmap(g->readback_buffer);}
static bool readback(NativeGpu *g,const uint8_t *p){
 Object *o=object(g,gpu_word(p,0));uint64_t dst=gpu_word(p,9)|((uint64_t)gpu_word(p,10)<<32),flag=gpu_word(p,15)|((uint64_t)gpu_word(p,16)<<32);
 bool is_buffer=gpu_word(p,13)!=0,box=gpu_word(p,2)!=0,ok=true;
 if(!check(g,o&&(!flag||(flag%4==0&&host_range(g->host,flag,4))),"Invalid readback resource or flag"))return false;
 if(is_buffer){
  uint32_t offset=box?gpu_word(p,3):0,end=box?gpu_word(p,6):o->size,bytes=box?end-offset:gpu_word(p,14)<o->size?gpu_word(p,14):o->size;
  if(!check(g,o->buffer&&offset<=end&&end<=o->size&&offset%4==0&&bytes<=READBACK_OUTPUT_MAX&&host_range(g->host,dst,bytes),"Buffer readback source/destination outside bounds or unaligned"))return false;
  for(uint32_t pos=0;pos<bytes;){
   uint32_t n=bytes-pos;if(n>READBACK_STAGING_MAX)n=READBACK_STAGING_MAX;uint32_t padded=(n+3)&~3u;
   if(!readback_buffer(g,padded)){ok=false;break;}WGPUCommandEncoder e=encoder(g);
   wgpuCommandEncoderCopyBufferToBuffer(e,o->buffer,(uint64_t)offset+pos,g->readback_buffer,0,padded);
   const uint8_t *data=readback_map(g,e,padded);if(data)host_store(g->host,dst+pos,data,n);else ok=false;
   readback_unmap(g);if(!ok)break;pos+=n;
  }
 }else{
  uint32_t bw=0,bh=0,block=0;
  if(!o->texture||o->td.sampleCount!=1||!readback_format(o->td.format,&bw,&bh,&block)){
   char why[300];snprintf(why,sizeof(why),"Unsupported texture readback: id %u DXGI %u native format %u samples %u sub %u",o->id,o->dxgi,(unsigned)o->td.format,o->td.sampleCount,gpu_word(p,1));fail(g,why);return false;
  }
  ReadbackRequest r={.dimension=o->td.dimension==WGPUTextureDimension_3D?3:o->td.dimension==WGPUTextureDimension_1D?1:2,.width=o->td.size.width,.height=o->td.size.height,.depth=o->td.size.depthOrArrayLayers,.mips=o->td.mipLevelCount,.sub=gpu_word(p,1),.block_width=bw,.block_height=bh,.block_bytes=block,.alpha_only=o->a8,.box=box,
   .left=gpu_word(p,3),.top=gpu_word(p,4),.front=gpu_word(p,5),.right=gpu_word(p,6),.bottom=gpu_word(p,7),.back=gpu_word(p,8),.row_pitch=gpu_word(p,11),.slice_pitch=gpu_word(p,12),.capacity=gpu_word(p,14)};
  ReadbackLayout l;const char *error=readback_layout(&r,&l);
  if(error){fail(g,error);return false;}if(!check(g,host_range(g->host,dst,l.span),"Texture readback destination outside shared heap"))return false;
  if(!g->texture_readbacks&&g->host->progress){char text[320];snprintf(text,sizeof(text),"Native texture readback: DXGI %u, %ux%ux%u, mip %u; GPU row %u bytes, CPU row stride %u, slice stride %u; staging at most 1 MiB",o->dxgi,l.width,l.height,l.depth,l.mip,l.gpu_row,l.row_stride,l.slice_stride);g->host->progress(g->host->progress_context,text);}
  for(uint32_t z=0;z<l.depth&&ok;z++)for(uint32_t y=0;y<l.rows&&ok;){
   uint32_t rows=l.rows-y;if(rows>l.tile_rows)rows=l.tile_rows;uint32_t bytes=rows*l.pitch;
   if(!readback_buffer(g,bytes)){ok=false;break;}WGPUCommandEncoder e=encoder(g);
   WGPUTexelCopyTextureInfo src=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;src.texture=o->texture;src.mipLevel=l.mip;src.origin=(WGPUOrigin3D){l.x,l.y+y*bh,l.z+z};
   WGPUTexelCopyBufferInfo target=WGPU_TEXEL_COPY_BUFFER_INFO_INIT;target.buffer=g->readback_buffer;target.layout.bytesPerRow=l.pitch;target.layout.rowsPerImage=rows;
   WGPUExtent3D extent={((l.width+bw-1)/bw)*bw,rows*bh,1};wgpuCommandEncoderCopyTextureToBuffer(e,&src,&target,&extent);
   const uint8_t *data=readback_map(g,e,bytes);
   if(data){for(uint32_t row=0;row<rows;row++){
    uint64_t address=dst+(uint64_t)z*l.slice_stride+(uint64_t)(y+row)*l.row_stride;const uint8_t *from=data+(uint64_t)row*l.pitch;
    if(!o->a8)host_store(g->host,address,from,l.output_row);
    else { /* Bounded packing directly into validated shared memory. */
     for(uint32_t x=0;x<l.width;x++)g->host->memory.data[address+x]=from[x*4+3];
    }
   }}else ok=false;
   readback_unmap(g);y+=rows;
  }
  if(ok)g->texture_readbacks++;
 }
 if(ok&&flag){atomic_store_explicit(heap_word(g,flag),0,memory_order_release);wake(g,flag);}if(ok)g->readbacks++;return ok;
}
