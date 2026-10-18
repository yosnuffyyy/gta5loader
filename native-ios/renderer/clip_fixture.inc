/* Native primitive clipping, including perspective-correct distances and
   MSAA. A fragment discard cannot substitute for these semantics. */
static bool clip_sample_test(NativeGpu *g){
 if(!check(g,g->clip_enabled,"Native clip-distance fixture requires negotiated feature"))return false;
 const char *code="enable clip_distances; @group(0) @binding(0) var<uniform> planes:array<vec4f,4>; struct Out{@builtin(position) p:vec4f,@builtin(clip_distances) d:array<f32,4>}; @vertex fn vs(@builtin(vertex_index) i:u32)->Out{var xy=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));var w=array<f32,3>(1,2,4);var o:Out;o.p=vec4f(xy[i]*w[i],0.5*w[i],w[i]);for(var j=0u;j<4u;j++){o.d[j]=dot(o.p,planes[j]);}return o;} @fragment fn fs()->@location(0) vec4f{return vec4f(0.25,0.5,0.75,1);}";
 WGPUShaderSourceWGSL src=WGPU_SHADER_SOURCE_WGSL_INIT;src.code=(WGPUStringView){code,WGPU_STRLEN};WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&src.chain;WGPUShaderModule module=wgpuDeviceCreateShaderModule(g->device,&md);
 WGPUBuffer uniform=NULL,read=NULL;WGPUTexture output=NULL;WGPUTextureView ov=NULL;bool ok=false;
 if(!module||atomic_load(&g->failed))goto cleanup;
 WGPUBufferDescriptor bd=WGPU_BUFFER_DESCRIPTOR_INIT;bd.size=64;bd.usage=WGPUBufferUsage_Uniform|WGPUBufferUsage_CopyDst;uniform=wgpuDeviceCreateBuffer(g->device,&bd);bd.size=4096;bd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;read=wgpuDeviceCreateBuffer(g->device,&bd);
 WGPUTextureDescriptor td=WGPU_TEXTURE_DESCRIPTOR_INIT;td.dimension=WGPUTextureDimension_2D;td.size=(WGPUExtent3D){16,16,1};td.mipLevelCount=td.sampleCount=1;td.format=WGPUTextureFormat_RGBA8Unorm;td.usage=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_CopySrc;output=wgpuDeviceCreateTexture(g->device,&td);if(!uniform||!read||!output||atomic_load(&g->failed))goto cleanup;ov=wgpuTextureCreateView(output,NULL);
 for(unsigned samples=1;samples<=4;samples+=3){
  WGPURenderPipeline pipeline=NULL;WGPUBindGroupLayout layout=NULL;WGPUBindGroup group=NULL;WGPUTexture ms=NULL;WGPUTextureView mv=NULL;bool passed=false;
  WGPUColorTargetState color=WGPU_COLOR_TARGET_STATE_INIT;color.format=WGPUTextureFormat_RGBA8Unorm;WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;fragment.module=module;fragment.entryPoint=(WGPUStringView){"fs",WGPU_STRLEN};fragment.targetCount=1;fragment.targets=&color;WGPURenderPipelineDescriptor pd=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;pd.vertex.module=module;pd.vertex.entryPoint=(WGPUStringView){"vs",WGPU_STRLEN};pd.fragment=&fragment;pd.multisample.count=samples;pipeline=wgpuDeviceCreateRenderPipeline(g->device,&pd);if(!pipeline||atomic_load(&g->failed))goto next;
  layout=wgpuRenderPipelineGetBindGroupLayout(pipeline,0);WGPUBindGroupEntry entry=WGPU_BIND_GROUP_ENTRY_INIT;entry.buffer=uniform;entry.size=64;WGPUBindGroupDescriptor bg=WGPU_BIND_GROUP_DESCRIPTOR_INIT;bg.layout=layout;bg.entryCount=1;bg.entries=&entry;group=wgpuDeviceCreateBindGroup(g->device,&bg);if(!group||atomic_load(&g->failed))goto next;
  if(samples==4){td.sampleCount=4;td.usage=WGPUTextureUsage_RenderAttachment;ms=wgpuDeviceCreateTexture(g->device,&td);if(!ms)goto next;mv=wgpuTextureCreateView(ms,NULL);if(!mv)goto next;}
  for(unsigned test=0;test<6;test++){
   float planes[4][4]={{0,0,0,1},{0,0,0,1},{0,0,0,1},{0,0,0,1}};
   if(test==1)planes[0][3]=-1;
   if(test==2||test==4||test==5){planes[0][0]=1;planes[0][3]=0;}
   if(test==3||test==4){planes[1][1]=1;planes[1][3]=0;}
   if(test==5){planes[2][0]=-1;planes[2][3]=0;}
   queue_write_buffer(g,uniform,0,planes,sizeof(planes));WGPUCommandEncoder e=encoder(g);WGPURenderPassColorAttachment ca=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;ca.view=samples==1?ov:mv;ca.resolveTarget=samples==4?ov:NULL;ca.loadOp=WGPULoadOp_Clear;ca.storeOp=WGPUStoreOp_Store;WGPURenderPassDescriptor desc=WGPU_RENDER_PASS_DESCRIPTOR_INIT;desc.colorAttachmentCount=1;desc.colorAttachments=&ca;WGPURenderPassEncoder pass=wgpuCommandEncoderBeginRenderPass(e,&desc);if(!pass){wgpuCommandEncoderRelease(e);goto next;}wgpuRenderPassEncoderSetPipeline(pass,pipeline);wgpuRenderPassEncoderSetBindGroup(pass,0,group,0,NULL);wgpuRenderPassEncoderDraw(pass,3,1,0,0);wgpuRenderPassEncoderEnd(pass);wgpuRenderPassEncoderRelease(pass);
   WGPUTexelCopyTextureInfo ti=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;ti.texture=output;WGPUTexelCopyBufferInfo dst=WGPU_TEXEL_COPY_BUFFER_INFO_INIT;dst.buffer=read;dst.layout.bytesPerRow=256;dst.layout.rowsPerImage=16;WGPUExtent3D extent={16,16,1};wgpuCommandEncoderCopyTextureToBuffer(e,&ti,&dst,&extent);if(!submit(g,e))goto next;
   atomic_store(&g->callback_done,false);atomic_store(&g->map_active,true);WGPUBufferMapCallbackInfo cb=WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;cb.mode=WGPUCallbackMode_AllowProcessEvents;cb.callback=mapped;cb.userdata1=g;wgpuBufferMapAsync(read,WGPUMapMode_Read,0,4096,cb);if(!wait_callback(g))goto next;const uint8_t *pixels=wgpuBufferGetConstMappedRange(read,0,4096);if(!pixels)goto next;
   for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){bool on=test==0||(test==2&&x>=8)||(test==3&&y<8)||(test==4&&x>=8&&y<8);int expected[4]={on?64:0,on?128:0,on?191:0,on?255:0};for(unsigned c=0;c<4;c++)if(abs((int)pixels[y*256+x*4+c]-expected[c])>1){char why[190];snprintf(why,sizeof(why),"Native clip fixture samples %u case %u pixel %u,%u channel %u got %u expected %d",samples,test,x,y,c,pixels[y*256+x*4+c],expected[c]);fail(g,why);goto next;}}
   wgpuBufferUnmap(read);atomic_store(&g->map_active,false);g->clip_cases_tested++;
  }passed=true;
 next:if(atomic_exchange(&g->map_active,false)&&read)wgpuBufferUnmap(read);if(group)wgpuBindGroupRelease(group);if(layout)wgpuBindGroupLayoutRelease(layout);if(pipeline)wgpuRenderPipelineRelease(pipeline);if(mv)wgpuTextureViewRelease(mv);if(ms){wgpuTextureDestroy(ms);wgpuTextureRelease(ms);}if(!passed||atomic_load(&g->failed))goto cleanup;
 }
 ok=g->clip_cases_tested==12;
 cleanup:if(atomic_exchange(&g->map_active,false)&&read)wgpuBufferUnmap(read);if(ov)wgpuTextureViewRelease(ov);if(output){wgpuTextureDestroy(output);wgpuTextureRelease(output);}if(uniform){wgpuBufferDestroy(uniform);wgpuBufferRelease(uniform);}if(read){wgpuBufferDestroy(read);wgpuBufferRelease(read);}if(module)wgpuShaderModuleRelease(module);return ok;
}
