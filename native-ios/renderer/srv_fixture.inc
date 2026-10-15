/* Production CREATE/UPLOAD/SRV decoding, view caches and proxy preparation.
   The small shaders/geometry are diagnostic; never described as scene pixels. */
static bool srv_pixel_fixture(struct w2c_env *h){@autoreleasepool{
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;bool ok=false;unsigned passed=0;
 WGPUBuffer read=NULL;WGPUTexture output=NULL;WGPUTextureView out=NULL;WGPUSampler sampler=NULL;
 if(!hardware(g))goto done;
 WGPUBufferDescriptor bd=WGPU_BUFFER_DESCRIPTOR_INIT;bd.size=1024;bd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;read=wgpuDeviceCreateBuffer(g->device,&bd);
 WGPUTextureDescriptor td=WGPU_TEXTURE_DESCRIPTOR_INIT;td.dimension=WGPUTextureDimension_2D;td.size=(WGPUExtent3D){4,4,1};td.mipLevelCount=td.sampleCount=1;td.format=WGPUTextureFormat_RGBA8Unorm;td.usage=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_CopySrc;output=wgpuDeviceCreateTexture(g->device,&td);if(output)out=wgpuTextureCreateView(output,NULL);
 WGPUSamplerDescriptor sd=WGPU_SAMPLER_DESCRIPTOR_INIT;sampler=wgpuDeviceCreateSampler(g->device,&sd);if(!read||!out||!sampler)goto done;
 for(unsigned test=0;test<24;test++){
  DrawProgram p={0};p.count=1;DrawBinding *b=p.bindings;b->kind=4;b->dim=WGPUTextureViewDimension_2D;b->sample=WGPUTextureSampleType_Float;
  WGPUShaderModule module=NULL;WGPURenderPipeline pipeline=NULL;WGPUBindGroupLayout layout=NULL;WGPUBindGroup group=NULL;bool valid=false;
  uint32_t cmd[80]={12000,2,4,4,1,2,13,28,8,1},v[9]={12001,0,12000,28,5,1,1,2,1};
  const char *type="texture_2d_array<f32>",*expr="textureLoad(src,vec2i(0),0,0)";unsigned expected=53;bool srgb=false,depth=false,use_sampler=false;
  b->dim=WGPUTextureViewDimension_2DArray;
  if(test<12){v[4]=10;v[7]=1;v[8]=12;b->dim=WGPUTextureViewDimension_CubeArray;type="texture_cube_array<f32>";use_sampler=true;expected=10+20*(1+test)+3;}
  else if(test==13){cmd[1]=3;cmd[4]=4;cmd[6]=1;v[4]=8;v[7]=v[8]=0;b->dim=WGPUTextureViewDimension_3D;type="texture_3d<f32>";expr="textureLoad(src,vec3i(0,0,1),0)";expected=33;}
  else if(test>=14){cmd[6]=1;v[4]=4;v[7]=v[8]=0;b->dim=WGPUTextureViewDimension_2D;type="texture_2d<f32>";expr="textureLoad(src,vec2i(0),0)";expected=13;
   if(test==14){v[3]=29;srgb=true;}
   if(test==15){cmd[5]=1;cmd[8]=0x28;cmd[9]=4;v[4]=6;v[5]=0;b->ms=true;type="texture_multisampled_2d<f32>";expected=128;}
   if(test==16){cmd[7]=v[3]=62;b->sample=WGPUTextureSampleType_Uint;type="texture_2d<u32>";expr="vec4f(f32(textureLoad(src,vec2i(0),0).x)/255.0,0,0,1)";}
   if(test==17){v[5]=0;expected=0;} /* truly unbound; RGBA zero */
   if(test>=18){cmd[5]=1;v[5]=0;expected=128;depth=true;b->sample=WGPUTextureSampleType_Depth;type="texture_depth_2d";expr="vec4f(textureLoad(src,vec2i(0),0),0,0,1)";if(test&1){v[3]=29;srgb=true;}}
  }
  if(!execute_impl(g,3,(uint8_t*)cmd,10))goto next;
  if(test==15){float c=.5f;uint32_t clear_cmd[7]={12000};memcpy(clear_cmd+3,&c,4);float a=1;memcpy(clear_cmd+6,&a,4);if(!clear(g,(uint8_t*)clear_cmd,false))goto next;}
  else for(unsigned mip=0;mip<cmd[5];mip++)for(unsigned layer=0;layer<(cmd[1]==3?(cmd[4]>>mip):cmd[6]);layer++){
   unsigned side=4>>mip,channels=test==16?1:4;uint32_t upload[28]={12000,mip,cmd[1]==3?0:layer,0,0,cmd[1]==3?layer:0,side,side,1,side*channels,side,side*side*channels};
   for(unsigned px=0;px<side*side;px++){uint8_t *data=(uint8_t*)(upload+12)+px*channels;data[0]=test>=18?128:(uint8_t)(10+20*layer+3*mip);if(channels==4)data[3]=255;}
   if(!execute_impl(g,13,(uint8_t*)upload,12+(side*side*channels+3)/4))goto next;
  }
  if(!execute_impl(g,17,(uint8_t*)v,9))goto next;g->srv[0][0]=test==17?0:12001;
  if(!draw_prepare_proxies(g,&p))goto next;
  WGPUTextureView input=draw_srv(g,b,g->srv[0][0]);if(!input)goto next;
  /* The cache must survive content writes, and alternate view encodings must
     refresh the right depth proxy rather than reuse the other encoding. */
  if(draw_srv(g,b,g->srv[0][0])!=input)goto next;
  if(test>=20){uint32_t upload[28]={12000,0,0,0,0,0,4,4,1,16,4,64};for(unsigned px=0;px<16;px++){((uint8_t*)(upload+12))[px*4]=64;((uint8_t*)(upload+12))[px*4+3]=255;}if(!execute_impl(g,13,(uint8_t*)upload,28)||!draw_prepare_proxies(g,&p)||draw_srv(g,b,12001)!=input)goto next;expected=64;}
  char expression[200],code[1000];if(test<12){unsigned f=test%6;const char *directions[]={"1,0,0","-1,0,0","0,1,0","0,-1,0","0,0,1","0,0,-1"};snprintf(expression,sizeof(expression),"textureSampleLevel(src,smp,vec3f(%s),%u,0.0)",directions[f],test/6);expr=expression;}
  if(test==12){expr="textureLoad(src,vec2i(0),i32(pos.x)/2,0)";v[8]=2;uint32_t id=12001;if(!execute_impl(g,4,(uint8_t*)&id,1)||!execute_impl(g,17,(uint8_t*)v,9)||!draw_prepare_proxies(g,&p))goto next;input=draw_srv(g,b,12001);if(!input)goto next;}
  snprintf(code,sizeof(code),"@group(0) @binding(0) var src:%s; %s @vertex fn vs(@builtin(vertex_index) i:u32)->@builtin(position) vec4f{var p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));return vec4f(p[i],0,1);} @fragment fn fs(@builtin(position) pos:vec4f)->@location(0) vec4f{return %s;}",type,use_sampler?"@group(0) @binding(1) var smp:sampler;":"",expr);
  WGPUShaderSourceWGSL wg=WGPU_SHADER_SOURCE_WGSL_INIT;wg.code=(WGPUStringView){code,WGPU_STRLEN};WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&wg.chain;module=wgpuDeviceCreateShaderModule(g->device,&md);
  WGPUColorTargetState color=WGPU_COLOR_TARGET_STATE_INIT;color.format=WGPUTextureFormat_RGBA8Unorm;WGPUFragmentState frag=WGPU_FRAGMENT_STATE_INIT;frag.module=module;frag.entryPoint=(WGPUStringView){"fs",WGPU_STRLEN};frag.targetCount=1;frag.targets=&color;WGPURenderPipelineDescriptor pd=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;pd.vertex.module=module;pd.vertex.entryPoint=(WGPUStringView){"vs",WGPU_STRLEN};pd.fragment=&frag;pipeline=wgpuDeviceCreateRenderPipeline(g->device,&pd);if(!pipeline||atomic_load(&g->failed))goto next;layout=wgpuRenderPipelineGetBindGroupLayout(pipeline,0);
  WGPUBindGroupEntry entries[2]={WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT};entries[0].textureView=input;entries[1].binding=1;entries[1].sampler=sampler;WGPUBindGroupDescriptor bg=WGPU_BIND_GROUP_DESCRIPTOR_INIT;bg.layout=layout;bg.entries=entries;bg.entryCount=use_sampler?2:1;group=wgpuDeviceCreateBindGroup(g->device,&bg);if(!group||!draw_flush(g))goto next;
  WGPUCommandEncoder e=encoder(g);WGPURenderPassColorAttachment ca=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;ca.view=out;ca.loadOp=WGPULoadOp_Clear;ca.storeOp=WGPUStoreOp_Store;WGPURenderPassDescriptor rp=WGPU_RENDER_PASS_DESCRIPTOR_INIT;rp.colorAttachmentCount=1;rp.colorAttachments=&ca;WGPURenderPassEncoder pass=wgpuCommandEncoderBeginRenderPass(e,&rp);wgpuRenderPassEncoderSetPipeline(pass,pipeline);wgpuRenderPassEncoderSetBindGroup(pass,0,group,0,NULL);wgpuRenderPassEncoderDraw(pass,3,1,0,0);wgpuRenderPassEncoderEnd(pass);wgpuRenderPassEncoderRelease(pass);
  WGPUTexelCopyTextureInfo ti=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;ti.texture=output;WGPUTexelCopyBufferInfo dst=WGPU_TEXEL_COPY_BUFFER_INFO_INIT;dst.buffer=read;dst.layout.bytesPerRow=256;dst.layout.rowsPerImage=4;WGPUExtent3D extent={4,4,1};wgpuCommandEncoderCopyTextureToBuffer(e,&ti,&dst,&extent);if(!submit(g,e))goto next;
  atomic_store(&g->callback_done,false);atomic_store(&g->map_active,true);WGPUBufferMapCallbackInfo cb=WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;cb.mode=WGPUCallbackMode_AllowProcessEvents;cb.callback=mapped;cb.userdata1=g;wgpuBufferMapAsync(read,WGPUMapMode_Read,0,1024,cb);if(!wait_callback(g))goto next;const uint8_t *data=wgpuBufferGetConstMappedRange(read,0,1024);if(!data)goto next;
  valid=true;for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){unsigned want=test==12?53+20*(x/2):expected;if(srgb){float f=want/255.f;want=(unsigned)lroundf((f<=.04045f?f/12.92f:powf((f+.055f)/1.055f,2.4f))*255.f);}const uint8_t *pixel=data+y*256+x*4;if(abs((int)pixel[0]-(int)want)>1||pixel[1]||pixel[2]||pixel[3]!=(test==17?0:255)){valid=false;fprintf(stderr,"SRV pixel case %u (%u,%u): %u/%u/%u/%u wanted red %u\n",test,x,y,pixel[0],pixel[1],pixel[2],pixel[3],want);}}
  if(depth&&(!object(g,12000)->proxies[srgb?3:2]||object(g,12000)->proxies[srgb?2:3]))valid=false;
 next:
  if(atomic_exchange(&g->map_active,false))wgpuBufferUnmap(read);if(group)wgpuBindGroupRelease(group);if(layout)wgpuBindGroupLayoutRelease(layout);if(pipeline)wgpuRenderPipelineRelease(pipeline);if(module)wgpuShaderModuleRelease(module);if(b->view)wgpuTextureViewRelease(b->view);if(b->dummy){wgpuTextureDestroy(b->dummy);wgpuTextureRelease(b->dummy);}
  if(!valid||atomic_load(&g->failed)){fprintf(stderr,"SRV pixel fixture failed case %u: %s\n",test,native_gpu_error(g)?native_gpu_error(g):"pixel/cache invariant");goto done;}
  for(unsigned id=12001;id>=12000;id--)if(!execute_impl(g,4,(uint8_t*)&id,1))goto done;passed++;
 }
 ok=passed==24&&!g->draw.proxy_bytes&&!g->draw.proxy_passes;
 done:if(out)wgpuTextureViewRelease(out);if(output){wgpuTextureDestroy(output);wgpuTextureRelease(output);}if(read){if(atomic_exchange(&g->map_active,false))wgpuBufferUnmap(read);wgpuBufferDestroy(read);wgpuBufferRelease(read);}if(sampler)wgpuSamplerRelease(sampler);release_hardware(g);native_gpu_end(g);return ok;
}}

static bool srv_invalid_fixture(struct w2c_env *h,unsigned test){
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;bool ok=false;if(!hardware(g))goto done;
 uint32_t c[]={12000,2,4,4,1,1,1,28,8,1},v[]={12001,0,12000,28,4,0,1,0,0};DrawProgram p={0};p.count=1;DrawBinding *b=p.bindings;b->kind=4;b->dim=WGPUTextureViewDimension_2D;b->sample=WGPUTextureSampleType_Float;
 if(!execute_impl(g,3,(uint8_t*)c,10)||!execute_impl(g,17,(uint8_t*)v,9))goto done;g->srv[0][0]=12001;
 if(test==0){g->srv[0][0]=999999;}
 if(test==1||test==2){uint32_t id=12000;if(!execute_impl(g,4,(uint8_t*)&id,1))goto done;if(test==2&&!execute_impl(g,3,(uint8_t*)c,10))goto done;}
 if(test==3)b->dim=WGPUTextureViewDimension_Cube;
 if(test==4)b->sample=WGPUTextureSampleType_Uint;
 if(test==5)b->ms=true;
 if(test==6)object(g,12001)->meta[3]=42;
 if(test==7){if(!draw_prepare_proxies(g,&p)||!draw_srv(g,b,12001))goto done;uint32_t id=12001;if(!execute_impl(g,4,(uint8_t*)&id,1))goto done;}
 if(test==8){b->kind=5;uint64_t offset,bytes;ok=!storage_buffer(g,b,999999,&offset,&bytes);}
 else ok=!draw_prepare_proxies(g,&p);
 ok=ok&&native_gpu_error(g)&&!strstr(native_gpu_error(g),"Native WebGPU")&&!g->draw.draws&&!g->draw.proxy_bytes;
 if(b->view)wgpuTextureViewRelease(b->view);
 done:release_hardware(g);native_gpu_end(g);return ok;
}
