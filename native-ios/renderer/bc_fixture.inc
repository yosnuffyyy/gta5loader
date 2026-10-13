/* Known compressed blocks, sampled by the GPU. No game texture expansion. */
static void bc_bits(uint8_t *p,unsigned at,unsigned count,uint32_t value){for(unsigned i=0;i<count;i++)p[(at+i)/8]|=((value>>i)&1u)<<((at+i)%8);}
static void bc_alpha(uint8_t *p,bool reverse){p[0]=reverse?0:255;p[1]=reverse?255:0;for(unsigned i=0;i<16;i++)bc_bits(p,16+3*i,3,i%8);}
static void bc_color(uint8_t *p){p[0]=0;p[1]=248;p[2]=224;p[3]=7;for(unsigned i=0;i<16;i++)bc_bits(p,32+2*i,2,i%4);}
static unsigned bc_block(unsigned kind,uint8_t out[16]){memset(out,0,16);switch(kind){
 case 1:bc_color(out);return 8;
 case 2:for(unsigned i=0;i<16;i++)bc_bits(out,4*i,4,i);bc_color(out+8);return 16;
 case 3:bc_alpha(out,false);bc_color(out+8);return 16;
 case 4:bc_alpha(out,false);return 8;
 case 5:bc_alpha(out,false);bc_alpha(out+8,true);return 16;
 case 6:return 16; /* Valid BC6H mode 0, zero endpoints/indices: black. */
 case 7:out[0]=64;bc_bits(out,7,7,127);bc_bits(out,14,7,127);bc_bits(out,49,7,127);bc_bits(out,56,7,127);return 16;
 default:return 0;
}}
static void bc_expected(unsigned kind,unsigned i,bool srgb,uint8_t out[4]){
 static const uint8_t c[4][3]={{255,0,0},{0,255,0},{170,85,0},{85,170,0}};
 /* Compare the final UNORM render-target quantization, not an intermediate
    integer truncation of the ideal decoded alpha. */
 static const float a[8]={1,0,6.f/7,5.f/7,4.f/7,3.f/7,2.f/7,1.f/7};
 static const uint8_t b[8]={0,255,51,102,153,204,0,255};
 out[0]=out[1]=out[2]=0;out[3]=255;
 if(kind<=3){memcpy(out,c[i%4],3);if(kind==2)out[3]=i*17;else if(kind==3)out[3]=(uint8_t)lroundf(a[i%8]*255.f);}
 else if(kind==4||kind==5){out[0]=(uint8_t)lroundf(a[i%8]*255.f);if(kind==5)out[1]=b[i%8];}
 else if(kind==7){out[0]=254;out[3]=254;}
 if(srgb)for(unsigned k=0;k<3;k++){float x=out[k]/255.f;x=x<=.04045f?x/12.92f:powf((x+.055f)/1.055f,2.4f);out[k]=(uint8_t)lroundf(x*255.f);}
}
static bool bc_sample_test(NativeGpu *g){
 if(!check(g,g->bc_enabled,"Metal backend did not expose native BC compression; source capability patch/device support must be checked"))return false;
 const char *code="@group(0) @binding(0) var src:texture_2d<f32>; @vertex fn vs(@builtin(vertex_index) i:u32)->@builtin(position) vec4f {var p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));return vec4f(p[i],0,1);} @fragment fn fs(@builtin(position) p:vec4f)->@location(0) vec4f{return textureLoad(src,vec2i(p.xy),0);}";
 WGPUShaderSourceWGSL wgsl=WGPU_SHADER_SOURCE_WGSL_INIT;wgsl.code=(WGPUStringView){code,WGPU_STRLEN};WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&wgsl.chain;
 WGPUShaderModule module=wgpuDeviceCreateShaderModule(g->device,&md);WGPURenderPipeline pipeline=NULL;WGPUBindGroupLayout layout=NULL;WGPUBuffer read=NULL;WGPUTexture output=NULL;WGPUTextureView out_view=NULL;bool ok=false;
 if(!module||atomic_load(&g->failed))goto cleanup;
 WGPUColorTargetState color=WGPU_COLOR_TARGET_STATE_INIT;color.format=WGPUTextureFormat_RGBA8Unorm;
 WGPUFragmentState fs=WGPU_FRAGMENT_STATE_INIT;fs.module=module;fs.entryPoint=(WGPUStringView){"fs",WGPU_STRLEN};fs.targetCount=1;fs.targets=&color;
 WGPURenderPipelineDescriptor pd=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;pd.vertex.module=module;pd.vertex.entryPoint=(WGPUStringView){"vs",WGPU_STRLEN};pd.fragment=&fs;pipeline=wgpuDeviceCreateRenderPipeline(g->device,&pd);if(!pipeline||atomic_load(&g->failed))goto cleanup;layout=wgpuRenderPipelineGetBindGroupLayout(pipeline,0);
 WGPUBufferDescriptor rb=WGPU_BUFFER_DESCRIPTOR_INIT;rb.size=1024;rb.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;read=wgpuDeviceCreateBuffer(g->device,&rb);
 WGPUTextureDescriptor td=WGPU_TEXTURE_DESCRIPTOR_INIT;td.dimension=WGPUTextureDimension_2D;td.mipLevelCount=1;td.sampleCount=1;td.size=(WGPUExtent3D){4,4,1};td.format=WGPUTextureFormat_RGBA8Unorm;td.usage=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_CopySrc;output=wgpuDeviceCreateTexture(g->device,&td);if(!read||!output||!layout||atomic_load(&g->failed))goto cleanup;out_view=wgpuTextureCreateView(output,NULL);if(!out_view)goto cleanup;
 static const struct {unsigned kind;bool srgb;WGPUTextureFormat format;} cases[]={
  {1,false,WGPUTextureFormat_BC1RGBAUnorm},{1,true,WGPUTextureFormat_BC1RGBAUnormSrgb},
  {2,false,WGPUTextureFormat_BC2RGBAUnorm},{2,true,WGPUTextureFormat_BC2RGBAUnormSrgb},
  {3,false,WGPUTextureFormat_BC3RGBAUnorm},{3,true,WGPUTextureFormat_BC3RGBAUnormSrgb},
  {4,false,WGPUTextureFormat_BC4RUnorm},{5,false,WGPUTextureFormat_BC5RGUnorm},
  {6,false,WGPUTextureFormat_BC6HRGBUfloat},{7,false,WGPUTextureFormat_BC7RGBAUnorm},{7,true,WGPUTextureFormat_BC7RGBAUnormSrgb}
 };
 for(unsigned test=0;test<sizeof(cases)/sizeof(cases[0]);test++){
  WGPUTexture input=NULL;WGPUTextureView in_view=NULL;WGPUBindGroup group=NULL;bool passed=false;
  td.format=cases[test].format;td.usage=WGPUTextureUsage_TextureBinding|WGPUTextureUsage_CopyDst;input=wgpuDeviceCreateTexture(g->device,&td);if(!input||atomic_load(&g->failed))goto next;
  in_view=wgpuTextureCreateView(input,NULL);if(!in_view)goto next;
  WGPUBindGroupEntry entry=WGPU_BIND_GROUP_ENTRY_INIT;entry.textureView=in_view;WGPUBindGroupDescriptor bg=WGPU_BIND_GROUP_DESCRIPTOR_INIT;bg.layout=layout;bg.entryCount=1;bg.entries=&entry;group=wgpuDeviceCreateBindGroup(g->device,&bg);if(!group||atomic_load(&g->failed))goto next;
  uint8_t block[16];unsigned bytes=bc_block(cases[test].kind,block);WGPUTexelCopyTextureInfo ti=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;ti.texture=input;WGPUTexelCopyBufferLayout tl=WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;tl.bytesPerRow=bytes;tl.rowsPerImage=4;WGPUExtent3D extent={4,4,1};wgpuQueueWriteTexture(g->queue,&ti,block,bytes,&tl,&extent);
  WGPUCommandEncoder e=encoder(g);WGPURenderPassColorAttachment ca=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;ca.view=out_view;ca.loadOp=WGPULoadOp_Clear;ca.storeOp=WGPUStoreOp_Store;WGPURenderPassDescriptor pass=WGPU_RENDER_PASS_DESCRIPTOR_INIT;pass.colorAttachmentCount=1;pass.colorAttachments=&ca;WGPURenderPassEncoder r=wgpuCommandEncoderBeginRenderPass(e,&pass);if(!r){wgpuCommandEncoderRelease(e);goto next;}
  wgpuRenderPassEncoderSetPipeline(r,pipeline);wgpuRenderPassEncoderSetBindGroup(r,0,group,0,NULL);wgpuRenderPassEncoderDraw(r,3,1,0,0);wgpuRenderPassEncoderEnd(r);wgpuRenderPassEncoderRelease(r);
  ti.texture=output;WGPUTexelCopyBufferInfo dst=WGPU_TEXEL_COPY_BUFFER_INFO_INIT;dst.buffer=read;dst.layout.bytesPerRow=256;dst.layout.rowsPerImage=4;wgpuCommandEncoderCopyTextureToBuffer(e,&ti,&dst,&extent);if(!submit(g,e))goto next;
  atomic_store(&g->callback_done,false);atomic_store(&g->map_active,true);WGPUBufferMapCallbackInfo cb=WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;cb.mode=WGPUCallbackMode_AllowProcessEvents;cb.callback=mapped;cb.userdata1=g;wgpuBufferMapAsync(read,WGPUMapMode_Read,0,1024,cb);if(!wait_callback(g))goto next;
  const uint8_t *data=wgpuBufferGetConstMappedRange(read,0,1024);if(!check(g,data!=NULL,"BC fixture mapped pixels unavailable"))goto next;
  passed=true;for(unsigned i=0;i<16&&passed;i++){uint8_t expected[4];bc_expected(cases[test].kind,i,cases[test].srgb,expected);for(unsigned k=0;k<4;k++)if(abs(data[(i/4)*256+(i%4)*4+k]-expected[k])>1){char why[240];snprintf(why,sizeof(why),"BC%u%s sampled pixel %u channel %u: got %u expected %u",cases[test].kind,cases[test].srgb?" sRGB":"",i,k,data[(i/4)*256+(i%4)*4+k],expected[k]);fail(g,why);passed=false;break;}}
 next:
  if(atomic_exchange(&g->map_active,false))wgpuBufferUnmap(read);if(group)wgpuBindGroupRelease(group);if(in_view)wgpuTextureViewRelease(in_view);if(input){wgpuTextureDestroy(input);wgpuTextureRelease(input);}if(!passed||atomic_load(&g->failed))goto cleanup;g->bc_formats_tested++;
 }
 ok=true;
 cleanup:
 if(atomic_exchange(&g->map_active,false)&&read)wgpuBufferUnmap(read);if(out_view)wgpuTextureViewRelease(out_view);if(output){wgpuTextureDestroy(output);wgpuTextureRelease(output);}if(read){wgpuBufferDestroy(read);wgpuBufferRelease(read);}if(layout)wgpuBindGroupLayoutRelease(layout);if(pipeline)wgpuRenderPipelineRelease(pipeline);if(module)wgpuShaderModuleRelease(module);return ok;
}
