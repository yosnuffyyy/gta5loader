/* Regression for the actual 0.11 CREATE_TEXTURE failure. Production resource
   handlers keep R8/RG8/RGBA8 SNORM storage. These diagnostic-only 4x4 targets
   test negative values, blending, uploads, copies and GPU sampling. */
static float snorm_value(int8_t value){return fmaxf(-1.f,(float)value/127.f);}
static bool snorm_sample_test(NativeGpu *g){
 if(!check(g,g->format_caps_enabled,"SNORM fixture requires adapter-specific format capabilities"))return false;
 const char *code=
  "@group(0) @binding(0) var src:texture_2d<f32>;"
  "@vertex fn vs(@builtin(vertex_index) i:u32)->@builtin(position) vec4f {"
  "var p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));return vec4f(p[i],0,1);}"
  "@fragment fn signed_color()->@location(0) vec4f{return vec4f(-0.75,0.5,-1,1);}"
  "@fragment fn sample_color(@builtin(position) p:vec4f)->@location(0) vec4f {"
  "return textureLoad(src,vec2i(p.xy),0)*0.5+vec4f(0.5);}";
 WGPUShaderSourceWGSL wgsl=WGPU_SHADER_SOURCE_WGSL_INIT;wgsl.code=(WGPUStringView){code,WGPU_STRLEN};
 WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&wgsl.chain;
 WGPUShaderModule module=wgpuDeviceCreateShaderModule(g->device,&md);
 WGPURenderPipeline sample=NULL;WGPUBindGroupLayout layout=NULL;WGPUBuffer read=NULL;
 WGPUTexture output=NULL;WGPUTextureView out_view=NULL;bool ok=false;
 if(!module||atomic_load(&g->failed))goto cleanup;
 WGPUColorTargetState color=WGPU_COLOR_TARGET_STATE_INIT;color.format=WGPUTextureFormat_RGBA8Unorm;
 WGPUFragmentState fs=WGPU_FRAGMENT_STATE_INIT;fs.module=module;fs.entryPoint=(WGPUStringView){"sample_color",WGPU_STRLEN};fs.targetCount=1;fs.targets=&color;
 WGPURenderPipelineDescriptor pd=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;pd.vertex.module=module;pd.vertex.entryPoint=(WGPUStringView){"vs",WGPU_STRLEN};pd.fragment=&fs;
 sample=wgpuDeviceCreateRenderPipeline(g->device,&pd);if(!sample||atomic_load(&g->failed))goto cleanup;
 layout=wgpuRenderPipelineGetBindGroupLayout(sample,0);
 WGPUBufferDescriptor rb=WGPU_BUFFER_DESCRIPTOR_INIT;rb.size=2048;rb.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;read=wgpuDeviceCreateBuffer(g->device,&rb);
 WGPUTextureDescriptor td=WGPU_TEXTURE_DESCRIPTOR_INIT;td.dimension=WGPUTextureDimension_2D;td.mipLevelCount=1;td.sampleCount=1;td.size=(WGPUExtent3D){4,4,1};td.format=WGPUTextureFormat_RGBA8Unorm;td.usage=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_CopySrc;
 output=wgpuDeviceCreateTexture(g->device,&td);if(!layout||!read||!output||atomic_load(&g->failed))goto cleanup;
 out_view=wgpuTextureCreateView(output,NULL);if(!out_view)goto cleanup;
 static const struct {uint32_t dxgi,channels;WGPUTextureFormat format;} formats[]={
  {63,1,WGPUTextureFormat_R8Snorm},{51,2,WGPUTextureFormat_RG8Snorm},{31,4,WGPUTextureFormat_RGBA8Snorm}
 };
 static const float initial[4]={-.5f,.25f,.75f,.5f},blended[4]={-1.f,.75f,-.25f,1.f};
 static const int8_t values[8]={-128,-127,-64,-1,0,1,64,127};
 for(unsigned f=0;f<3;f++){
  WGPURenderPipeline draw=NULL;WGPUTextureView in_view=NULL,copy_view=NULL;WGPUBindGroup group=NULL;bool passed=false;
  uint32_t cmd[28]={501,2,4,4,1,1,1,formats[f].dxgi,0x28,1};
  if(!create_texture(g,(uint8_t*)cmd))goto next;
  cmd[0]=502;if(!create_texture(g,(uint8_t*)cmd))goto next;
  Object *input=object(g,501),*copy=object(g,502);
  if(!check(g,input&&copy&&input->td.format==formats[f].format&&copy->td.format==formats[f].format,"SNORM format/storage was changed"))goto next;
  in_view=view(input,0,0);copy_view=view(copy,0,0);if(!in_view||!copy_view)goto next;
  WGPUBindGroupEntry entry=WGPU_BIND_GROUP_ENTRY_INIT;entry.textureView=copy_view;
  WGPUBindGroupDescriptor bg=WGPU_BIND_GROUP_DESCRIPTOR_INIT;bg.layout=layout;bg.entryCount=1;bg.entries=&entry;
  group=wgpuDeviceCreateBindGroup(g->device,&bg);if(!group||atomic_load(&g->failed))goto next;
  WGPUBlendState blend=WGPU_BLEND_STATE_INIT;blend.color.srcFactor=WGPUBlendFactor_One;blend.color.dstFactor=WGPUBlendFactor_One;blend.alpha=blend.color;
  color.format=formats[f].format;color.blend=&blend;fs.entryPoint=(WGPUStringView){"signed_color",WGPU_STRLEN};
  draw=wgpuDeviceCreateRenderPipeline(g->device,&pd);if(!draw||atomic_load(&g->failed))goto next;
  for(unsigned test=0;test<3;test++){
   cmd[0]=501;cmd[1]=0;cmd[2]=0;memcpy(cmd+3,initial,sizeof(initial));if(!clear(g,(uint8_t*)cmd,false))goto next;
   if(test==1){
    WGPUCommandEncoder e=encoder(g);WGPURenderPassColorAttachment ca=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;ca.view=in_view;ca.loadOp=WGPULoadOp_Load;ca.storeOp=WGPUStoreOp_Store;
    WGPURenderPassDescriptor pass=WGPU_RENDER_PASS_DESCRIPTOR_INIT;pass.colorAttachmentCount=1;pass.colorAttachments=&ca;
    WGPURenderPassEncoder r=wgpuCommandEncoderBeginRenderPass(e,&pass);if(!r){wgpuCommandEncoderRelease(e);goto next;}
    wgpuRenderPassEncoderSetPipeline(r,draw);wgpuRenderPassEncoderDraw(r,3,1,0,0);wgpuRenderPassEncoderEnd(r);wgpuRenderPassEncoderRelease(r);if(!submit(g,e))goto next;
   }else if(test==2){
    memset(cmd,0,sizeof(cmd));cmd[0]=501;cmd[6]=4;cmd[7]=4;cmd[8]=1;cmd[9]=4*formats[f].channels;cmd[10]=4;cmd[11]=16*formats[f].channels;
    for(unsigned i=0;i<cmd[11];i++)((int8_t*)(cmd+12))[i]=values[i%8];
    if(!upload_texture(g,(uint8_t*)cmd))goto next;
   }
   cmd[0]=502;cmd[1]=501;if(!copy_resource(g,(uint8_t*)cmd))goto next;
   WGPUCommandEncoder e=encoder(g);WGPURenderPassColorAttachment ca=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;ca.view=out_view;ca.loadOp=WGPULoadOp_Clear;ca.storeOp=WGPUStoreOp_Store;
   WGPURenderPassDescriptor pass=WGPU_RENDER_PASS_DESCRIPTOR_INIT;pass.colorAttachmentCount=1;pass.colorAttachments=&ca;
   WGPURenderPassEncoder r=wgpuCommandEncoderBeginRenderPass(e,&pass);if(!r){wgpuCommandEncoderRelease(e);goto next;}
   wgpuRenderPassEncoderSetPipeline(r,sample);wgpuRenderPassEncoderSetBindGroup(r,0,group,0,NULL);wgpuRenderPassEncoderDraw(r,3,1,0,0);wgpuRenderPassEncoderEnd(r);wgpuRenderPassEncoderRelease(r);
   WGPUTexelCopyTextureInfo ti=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;ti.texture=output;WGPUTexelCopyBufferInfo dst=WGPU_TEXEL_COPY_BUFFER_INFO_INIT;dst.buffer=read;dst.layout.bytesPerRow=256;dst.layout.rowsPerImage=4;WGPUExtent3D extent={4,4,1};
   wgpuCommandEncoderCopyTextureToBuffer(e,&ti,&dst,&extent);ti.texture=copy->texture;dst.layout.offset=1024;wgpuCommandEncoderCopyTextureToBuffer(e,&ti,&dst,&extent);if(!submit(g,e))goto next;
   atomic_store(&g->callback_done,false);atomic_store(&g->map_active,true);WGPUBufferMapCallbackInfo cb=WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;cb.mode=WGPUCallbackMode_AllowProcessEvents;cb.callback=mapped;cb.userdata1=g;
   wgpuBufferMapAsync(read,WGPUMapMode_Read,0,2048,cb);if(!wait_callback(g))goto next;
   const uint8_t *data=wgpuBufferGetConstMappedRange(read,0,2048);if(!check(g,data!=NULL,"SNORM fixture mapped pixels unavailable"))goto next;
   for(unsigned i=0;i<16;i++)for(unsigned k=0;k<4;k++){
    bool stored=k<formats[f].channels;float expected=stored?(test==2?snorm_value(values[(i*formats[f].channels+k)%8]):test==1?blended[k]:initial[k]):k==3?1.f:0.f;
    int sampled=data[(i/4)*256+(i%4)*4+k],wanted=(int)lroundf((expected*.5f+.5f)*255.f);
    bool valid=abs(sampled-wanted)<=2;
    if(stored){int8_t raw=(int8_t)data[1024+(i/4)*256+(i%4)*formats[f].channels+k];valid=valid&&fabsf(snorm_value(raw)-expected)<=1.01f/127.f;if(test==2)valid=valid&&raw==values[(i*formats[f].channels+k)%8];}
    if(!valid){char why[220];snprintf(why,sizeof(why),"SNORM DXGI %u case %u pixel %u channel %u: sample %d expected %d; signed storage/readback mismatch",formats[f].dxgi,test,i,k,sampled,wanted);fail(g,why);goto next;}
   }
   wgpuBufferUnmap(read);atomic_store(&g->map_active,false);g->snorm_cases_tested++;
  }
  passed=true;
 next:
  if(atomic_exchange(&g->map_active,false))wgpuBufferUnmap(read);
  if(group)wgpuBindGroupRelease(group);if(in_view)wgpuTextureViewRelease(in_view);if(copy_view)wgpuTextureViewRelease(copy_view);if(draw)wgpuRenderPipelineRelease(draw);
  for(uint32_t id=501;id<=502;id++){Object *o=object(g,id);if(o){CFDictionaryRemoveValue(g->objects,(const void*)(uintptr_t)id);destroy(o);}}
  if(!passed||atomic_load(&g->failed))goto cleanup;
 }
 ok=g->snorm_cases_tested==9;
 cleanup:
 if(atomic_exchange(&g->map_active,false)&&read)wgpuBufferUnmap(read);
 if(out_view)wgpuTextureViewRelease(out_view);if(output){wgpuTextureDestroy(output);wgpuTextureRelease(output);}if(read){wgpuBufferDestroy(read);wgpuBufferRelease(read);}
 if(layout)wgpuBindGroupLayoutRelease(layout);if(sample)wgpuRenderPipelineRelease(sample);if(module)wgpuShaderModuleRelease(module);return ok;
}
