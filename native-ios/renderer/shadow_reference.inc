/* Created lazily on the GPU owner thread, initialized before dependent draws
   in the same encoder, and retained until GPU service teardown. */
static bool shadow_reference_prepare(NativeGpu *g){
 if(g->reference_shadow_cube)return true;
 WGPUTextureDescriptor td=WGPU_TEXTURE_DESCRIPTOR_INIT;
 td.label=(WGPUStringView){"optional reference empty shadow cube",WGPU_STRLEN};
 td.dimension=WGPUTextureDimension_2D;td.size=(WGPUExtent3D){1,1,6};
 td.mipLevelCount=td.sampleCount=1;td.format=WGPUTextureFormat_Depth32Float;
 td.usage=WGPUTextureUsage_TextureBinding|WGPUTextureUsage_RenderAttachment;
 g->reference_shadow_cube=wgpuDeviceCreateTexture(g->device,&td);
 if(!check(g,g->reference_shadow_cube&&!atomic_load(&g->failed),"Reference empty shadow cube allocation"))return false;
 draw_end_pass(g);if(!g->draw.encoder)g->draw.encoder=encoder(g);
 if(!g->draw.encoder)return false;
 for(unsigned face=0;face<6;face++){
  WGPUTextureViewDescriptor vd=WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
  vd.dimension=WGPUTextureViewDimension_2D;vd.aspect=WGPUTextureAspect_DepthOnly;
  vd.baseArrayLayer=face;vd.arrayLayerCount=vd.mipLevelCount=1;
  WGPUTextureView view=wgpuTextureCreateView(g->reference_shadow_cube,&vd);if(!view)return false;
  WGPURenderPassDepthStencilAttachment depth=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
  depth.view=view;depth.depthLoadOp=WGPULoadOp_Clear;depth.depthStoreOp=WGPUStoreOp_Store;depth.depthClearValue=0;
  WGPURenderPassDescriptor pd=WGPU_RENDER_PASS_DESCRIPTOR_INIT;pd.depthStencilAttachment=&depth;
  WGPURenderPassEncoder pass=wgpuCommandEncoderBeginRenderPass(g->draw.encoder,&pd);
  if(pass){wgpuRenderPassEncoderEnd(pass);wgpuRenderPassEncoderRelease(pass);}wgpuTextureViewRelease(view);
  if(!check(g,pass&&!atomic_load(&g->failed),"Reference empty shadow cube initialization"))return false;
 }
 g->reference_shadow_allocations++;return true;
}
static void shadow_reference_set(DrawProgram *p,uint32_t binding,bool enabled){
 for(unsigned i=0;i<p->count;i++)if(p->bindings[i].binding==binding){
  if(p->bindings[i].reference_empty!=enabled)p->proxy_scan_valid=false;
  p->bindings[i].reference_empty=enabled;return;
 }
}
static DrawProgram *shadow_reference_sync(NativeGpu *g,DrawProgram *variant,DrawProgram *source){
 if(!g->reference_shadow_compat)return variant;
 /* Both lists are sorted; a linked variant is a subset of the source.
    Merge by ABI binding, including texture slots above31, without hashing
    uniforms or assuming that a binding number fits a32-bit bit mask. */
 unsigned source_index=0;
 for(unsigned i=0;i<variant->count;i++){DrawBinding *b=variant->bindings+i;
  while(source_index<source->count&&source->bindings[source_index].binding<b->binding)source_index++;
  bool enabled=source_index<source->count&&source->bindings[source_index].binding==b->binding&&source->bindings[source_index].reference_empty;
  if(b->reference_empty!=enabled)variant->proxy_scan_valid=false;b->reference_empty=enabled;
 }return variant;
}
