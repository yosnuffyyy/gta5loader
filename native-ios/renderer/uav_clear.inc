static unsigned uav_pixel(NativeGpu *g,WGPUTextureFormat fmt,bool floating,const uint32_t values[4],uint8_t pixel[16]){
 for(unsigned f=0;f<sizeof(storage_formats)/sizeof(storage_formats[0]);f++)if(storage_formats[f].format==fmt){unsigned channels=storage_formats[f].channels,bits=storage_formats[f].bits,type=storage_formats[f].type;memset(pixel,0,16);
  for(unsigned k=0;k<channels;k++){unsigned src=fmt==WGPUTextureFormat_BGRA8Unorm&&k<3?2-k:k;uint32_t word=values[src];if(floating){float value;memcpy(&value,&word,4);if(!check(g,isfinite(value),"Non-finite UAV float clear is unsupported"))return 0;
   if(type==0&&bits==16){_Float16 half=(_Float16)value;memcpy(&word,&half,2);}else if(type==1)word=(uint32_t)lroundf(fmaxf(0,fminf(1,value))*uav_component_mask(bits));else if(type==2)word=(uint32_t)(int32_t)lroundf(fmaxf(-1,fminf(1,value))*((1u<<(bits-1))-1));else if(type>=3){fail(g,"Float clear requires a floating-point or normalized UAV format");return 0;}
  }else word&=uav_component_mask(bits);
  memcpy(pixel+k*(bits/8),&word,bits/8);
 }return channels*bits/8;
 }fail(g,"UAV clear format is unsupported");return 0;
}
static bool uav_clear(NativeGpu *g,const uint8_t *p){Object *v=uav_object(g,gpu_word(p,0));if(!v)return false;Object *o=object(g,v->meta[2]);bool floating=gpu_word(p,1)!=0;uint32_t values[4];for(unsigned i=0;i<4;i++)values[i]=gpu_word(p,2+i);bool zero=!(values[0]|values[1]|values[2]|values[3]);
 if(v->meta[1]){uint64_t offset,bytes;if(!storage_range(g,v,o,&offset,&bytes))return false;
  if(zero){draw_end_pass(g);if(!g->draw.encoder)g->draw.encoder=encoder(g);if(!g->draw.encoder)return false;wgpuCommandEncoderClearBuffer(g->draw.encoder,o->buffer,offset,bytes);}
  else {if(!draw_flush(g))return false;uint8_t pattern[16];unsigned step=4;if(v->meta[8]&1||o->stride){if(!check(g,!floating,"Float clear on raw/structured buffer is invalid"))return false;memcpy(pattern,values,4);}else {WGPUTextureFormat fmt=format(v->meta[3],0);step=uav_pixel(g,fmt,floating,values,pattern);if(!step)return false;}
   uint32_t chunk=256*1024;if(bytes<chunk)chunk=(uint32_t)bytes;chunk-=chunk%step;uint8_t *data=malloc(chunk);if(!check(g,data!=NULL&&chunk,"Bounded UAV buffer clear allocation")){free(data);return false;}for(unsigned i=0;i<chunk;i+=step)memcpy(data+i,pattern,step);for(uint64_t done=0;done<bytes;){uint32_t n=bytes-done<chunk?(uint32_t)(bytes-done):chunk;wgpuQueueWriteBuffer(g->queue,o->buffer,offset+done,data,n);done+=n;}free(data);
  }
 }else {uint32_t mip=v->meta[5],width=mip_extent(o->td.size.width,mip),height=mip_extent(o->td.size.height,mip),first=v->meta[4]==5||v->meta[4]==8?v->meta[6]:0,layers=v->meta[4]==5||v->meta[4]==8?v->meta[7]:1,total=o->td.dimension==WGPUTextureDimension_3D?mip_extent(o->td.size.depthOrArrayLayers,mip):o->td.size.depthOrArrayLayers;
  if(!layers||layers==UINT32_MAX)layers=total-first;if(!check(g,mip<o->td.mipLevelCount&&first<total&&layers<=total-first&&o->td.sampleCount==1,"UAV clear mip/layer range invalid"))return false;
  uint8_t pixel[16];unsigned bpp=uav_pixel(g,o->td.format,floating,values,pixel);if(!bpp)return false;
  if(zero){if(!g->zero_source){WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=1024*1024;d.usage=WGPUBufferUsage_CopySrc;g->zero_source=wgpuDeviceCreateBuffer(g->device,&d);if(!g->zero_source)return false;}draw_end_pass(g);if(!g->draw.encoder)g->draw.encoder=encoder(g);if(!g->draw.encoder)return false;uint32_t row=(width*bpp+255)&~255u,rows=(1024*1024)/row;if(!check(g,rows>0,"UAV clear row exceeds staging bound"))return false;
   for(unsigned layer=first;layer<first+layers;layer++)for(unsigned y=0;y<height;y+=rows){unsigned n=height-y<rows?height-y:rows;WGPUTexelCopyBufferInfo src=WGPU_TEXEL_COPY_BUFFER_INFO_INIT;src.buffer=g->zero_source;src.layout.bytesPerRow=row;src.layout.rowsPerImage=n;WGPUTexelCopyTextureInfo dst=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;dst.texture=o->texture;dst.mipLevel=mip;dst.origin=(WGPUOrigin3D){0,y,layer};WGPUExtent3D extent={width,n,1};wgpuCommandEncoderCopyBufferToTexture(g->draw.encoder,&src,&dst,&extent);}
  }else {if(!draw_flush(g))return false;uint32_t row=width*bpp,rows=(256*1024)/row;if(!rows)rows=1;if(rows>height)rows=height;uint8_t *data=malloc((size_t)rows*row);if(!check(g,data!=NULL,"Bounded UAV texture clear allocation"))return false;for(unsigned i=0;i<rows*row;i+=bpp)memcpy(data+i,pixel,bpp);
   for(unsigned layer=first;layer<first+layers;layer++)for(unsigned y=0;y<height;y+=rows){unsigned n=height-y<rows?height-y:rows;WGPUTexelCopyTextureInfo dst=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;dst.texture=o->texture;dst.mipLevel=mip;dst.origin=(WGPUOrigin3D){0,y,layer};WGPUTexelCopyBufferLayout layout=WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;layout.bytesPerRow=row;layout.rowsPerImage=n;WGPUExtent3D extent={width,n,1};wgpuQueueWriteTexture(g->queue,&dst,data,(size_t)n*row,&layout,&extent);}free(data);
  }proxy_dirty(o);
 }g->draw.uav_clears++;return !atomic_load(&g->failed);
}
