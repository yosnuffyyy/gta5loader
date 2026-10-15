/* CI only. Validate every translated shader through the production loader,
   uniform packing and layouts, then compile native Metal pipelines. One
   shader/pipeline is live at a time. No diagnostic pixels claim a game scene. */
#include "recorded_pair_audit.inc"
#include "ped_pair_audit.inc"
#include "ped_shadow_audit.inc"
#include "array_srv_audit.inc"
static bool audit_null_ms(NativeGpu *g){
 const WGPUTextureSampleType types[]={WGPUTextureSampleType_UnfilterableFloat,WGPUTextureSampleType_Uint,WGPUTextureSampleType_Sint};
 for(unsigned i=0;i<3;i++){
  DrawBinding b={.kind=4,.sample=types[i],.ms=true,.dim=WGPUTextureViewDimension_2D};WGPUBindGroupLayout layout=NULL;WGPUBindGroup group=NULL;bool ok=false;
  WGPUTextureView v=draw_srv(g,&b,0);if(!v||atomic_load(&g->failed))goto cleanup;
  WGPUBindGroupLayoutEntry entry=WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;entry.visibility=WGPUShaderStage_Fragment;entry.texture.sampleType=types[i];entry.texture.viewDimension=WGPUTextureViewDimension_2D;entry.texture.multisampled=true;
  WGPUBindGroupLayoutDescriptor ld=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;ld.entryCount=1;ld.entries=&entry;layout=wgpuDeviceCreateBindGroupLayout(g->device,&ld);if(!layout||atomic_load(&g->failed))goto cleanup;
  WGPUBindGroupEntry binding=WGPU_BIND_GROUP_ENTRY_INIT;binding.textureView=v;WGPUBindGroupDescriptor bd=WGPU_BIND_GROUP_DESCRIPTOR_INIT;bd.layout=layout;bd.entryCount=1;bd.entries=&binding;group=wgpuDeviceCreateBindGroup(g->device,&bd);ok=group&&!atomic_load(&g->failed);
 cleanup:if(group)wgpuBindGroupRelease(group);if(layout)wgpuBindGroupLayoutRelease(layout);if(b.view)wgpuTextureViewRelease(b.view);if(b.dummy){wgpuTextureDestroy(b.dummy);wgpuTextureRelease(b.dummy);}if(!ok)return false;
 }
 puts("PASS: production null multisample textures and typed float/uint/sint bind groups validated (3 cases). Pixel output is checked separately by rendering regressions.");return true;
}
static bool audit_pipeline(NativeGpu *g,DrawProgram *p){@autoreleasepool{
 NSString *source=nil,*args=nil,*output=nil,*vertexText=nil,*fragmentText=nil;LinkedModules linked={0};NSArray *entries=nil,*inputs=nil,*results=nil;
 WGPUBindGroupLayout layouts[2]={p->layout,NULL};WGPUBindGroupLayout empty=NULL;
 WGPUPipelineLayoutDescriptor ld=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
 if(p->stage==1){WGPUBindGroupLayoutDescriptor e=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;empty=wgpuDeviceCreateBindGroupLayout(g->device,&e);layouts[0]=empty;layouts[1]=p->layout;ld.bindGroupLayoutCount=2;}else ld.bindGroupLayoutCount=1;
 ld.bindGroupLayouts=layouts;WGPUPipelineLayout layout=wgpuDeviceCreatePipelineLayout(g->device,&ld);
 WGPUShaderModule stub=NULL;WGPURenderPipeline render=NULL;WGPUComputePipeline compute=NULL;bool ok=false;
 if(!layout||atomic_load(&g->failed))goto cleanup;
 if(p->stage==2){WGPUComputePipelineDescriptor d=WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;d.layout=layout;d.compute.module=p->module;d.compute.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};compute=wgpuDeviceCreateComputePipeline(g->device,&d);ok=compute&&!atomic_load(&g->failed);goto cleanup;}
 source=@(p->source);entries=draw_matches(@"@(vertex|fragment)\\s*fn\\s+main\\s*\\(((?:[^()]|\\([^)]*\\))*)\\)",source);
 if(!check(g,entries.count==1,"Shader audit could not reflect entry inputs"))goto cleanup;
 args=draw_capture(source,entries[0],2);
 inputs=draw_matches(@"(@location\\((\\d+)u?\\)(?:\\s*@interpolate\\([^)]*\\))?)\\s+(\\w+)\\s*:\\s*((?:vec[234]<\\s*(?:f32|u32|i32)\\s*>|f32|u32|i32))",args);
 WGPURenderPipelineDescriptor d=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;d.layout=layout;
 WGPUVertexAttribute attributes[32];WGPUVertexBufferLayout buffers[32];unsigned count=0;
 if(p->stage==0){
  for(NSTextCheckingResult *m in inputs){NSString *type=[draw_capture(args,m,4) stringByReplacingOccurrencesOfString:@" " withString:@""];unsigned size=[type hasPrefix:@"vec"]?(unsigned)([type characterAtIndex:3]-'0'):1;bool integer=[type containsString:@"u32"],signed_=[type containsString:@"i32"];
   static const WGPUVertexFormat floats[]={0,WGPUVertexFormat_Float32,WGPUVertexFormat_Float32x2,WGPUVertexFormat_Float32x3,WGPUVertexFormat_Float32x4},uints[]={0,WGPUVertexFormat_Uint32,WGPUVertexFormat_Uint32x2,WGPUVertexFormat_Uint32x3,WGPUVertexFormat_Uint32x4},ints[]={0,WGPUVertexFormat_Sint32,WGPUVertexFormat_Sint32x2,WGPUVertexFormat_Sint32x3,WGPUVertexFormat_Sint32x4};
   if(!check(g,count<32&&size>=1&&size<=4,"Shader audit input limit"))goto cleanup;attributes[count]=(WGPUVertexAttribute)WGPU_VERTEX_ATTRIBUTE_INIT;attributes[count].format=integer?uints[size]:signed_?ints[size]:floats[size];attributes[count].shaderLocation=draw_capture(args,m,2).intValue;buffers[count]=(WGPUVertexBufferLayout)WGPU_VERTEX_BUFFER_LAYOUT_INIT;buffers[count].arrayStride=size*4;buffers[count].attributeCount=1;buffers[count].attributes=attributes+count;count++;
  }
  // Use a single packed vertex buffer to stay inside real Metal slot limits.
  unsigned stride=0;for(unsigned i=0;i<count;i++){attributes[i].offset=stride;stride+=(unsigned)buffers[i].arrayStride;}
  if(count){buffers[0].arrayStride=stride;buffers[0].attributeCount=count;buffers[0].attributes=attributes;d.vertex.bufferCount=1;d.vertex.buffers=buffers;}
  vertexText=source;d.vertex.module=p->module;d.vertex.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};
 }else{
  NSMutableString *text=[NSMutableString stringWithString:@"struct AuditOut { @builtin(position) position:vec4<f32>,\n"];
  for(NSTextCheckingResult *m in inputs)[text appendFormat:@"%@ %@:%@,\n",draw_capture(args,m,1),draw_capture(args,m,3),draw_capture(args,m,4)];
  [text appendString:@"}\n@vertex fn main()->AuditOut {var o:AuditOut;o.position=vec4<f32>(0,0,0,1);return o;}\n"];
  WGPUShaderSourceWGSL s=WGPU_SHADER_SOURCE_WGSL_INIT;vertexText=text;s.code=(WGPUStringView){text.UTF8String,WGPU_STRLEN};WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&s.chain;stub=wgpuDeviceCreateShaderModule(g->device,&md);d.vertex.module=stub;d.vertex.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};
 }
 WGPUDepthStencilState depth=WGPU_DEPTH_STENCIL_STATE_INIT;depth.format=WGPUTextureFormat_Depth32Float;depth.depthWriteEnabled=WGPUOptionalBool_True;depth.depthCompare=WGPUCompareFunction_Always;d.depthStencil=&depth;
 WGPUColorTargetState targets[8];WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;
 if(p->stage==1){
  results=draw_matches(@"@fragment\\s*fn\\s+main\\s*\\((?:[^()]|\\([^)]*\\))*\\)\\s*->\\s*([^{]+)\\{",source);
  if(!check(g,results.count==1,"Shader audit could not reflect fragment result"))goto cleanup;output=[draw_capture(source,results[0],1) stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
  if(![output containsString:@"@location"]){NSArray *st=draw_matches([NSString stringWithFormat:@"struct\\s+%@\\s*\\{([^}]*)\\}",output],source);if(st.count)output=draw_capture(source,st[0],1);}
  unsigned colors=0;for(unsigned i=0;i<8;i++){targets[i]=(WGPUColorTargetState)WGPU_COLOR_TARGET_STATE_INIT;if(p->outputs&(1u<<i)){targets[i].format=WGPUTextureFormat_RGBA8Unorm;colors=i+1;}}
  for(NSTextCheckingResult *m in draw_matches(@"@location\\((\\d+)u?\\)\\s*(?:\\w+\\s*:\\s*)?(vec[234]<\\s*(?:u32|i32|f32)\\s*>|u32|i32|f32)",output)){unsigned i=draw_capture(output,m,1).intValue;NSString *type=draw_capture(output,m,2);if(i<8){bool scalar=![type hasPrefix:@"vec"],two=[type hasPrefix:@"vec2"];if([type containsString:@"u32"])targets[i].format=scalar?WGPUTextureFormat_R8Uint:two?WGPUTextureFormat_RG8Uint:WGPUTextureFormat_RGBA8Uint;else if([type containsString:@"i32"])targets[i].format=scalar?WGPUTextureFormat_R8Sint:two?WGPUTextureFormat_RG8Sint:WGPUTextureFormat_RGBA8Sint;else targets[i].format=scalar?WGPUTextureFormat_R8Unorm:two?WGPUTextureFormat_RG8Unorm:WGPUTextureFormat_RGBA8Unorm;}}
  fragmentText=source;fragment.module=p->module;fragment.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};fragment.targetCount=colors;fragment.targets=targets;d.fragment=&fragment;
 }
 if(!draw_link_modules(g,vertexText,fragmentText,p->stage==1?p->flat_inputs:0,&linked))goto cleanup;if(linked.vertex)d.vertex.module=linked.vertex;if(linked.fragment)fragment.module=linked.fragment;d.label=(WGPUStringView){p->hash,WGPU_STRLEN};render=wgpuDeviceCreateRenderPipeline(g->device,&d);ok=render&&!atomic_load(&g->failed);
 cleanup:linked_modules_free(&linked);if(render)wgpuRenderPipelineRelease(render);if(compute)wgpuComputePipelineRelease(compute);if(stub)wgpuShaderModuleRelease(stub);if(layout)wgpuPipelineLayoutRelease(layout);if(empty)wgpuBindGroupLayoutRelease(empty);return ok;
}}
int native_gpu_shader_audit(const char *directory,unsigned shard,unsigned parts){@autoreleasepool{
 if(parts!=3||shard>=parts)return 2;
 NSData *raw=nil;NSDictionary *index=nil;
 wasm_rt_init();struct w2c_env *h=calloc(1,sizeof(*h));if(!h)return 2;host_init(h);wasm_rt_allocate_memory_shared(&h->memory,64,65536,false,65536);
 h->game_root=open(directory,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);int result=1;
 if(!g||h->game_root<0)goto cleanup;
 g->test_inter_stage_limit=15;if(!hardware(g)||!audit_null_ms(g)||(shard==0&&(!audit_recorded_pair(g)||!audit_ped_pair(g)||!audit_ped_shadow(g)||!audit_active_shadow(h,false)||!audit_active_shadow(h,true)||!audit_array_srv(h))))goto cleanup;
 raw=draw_read(h->game_root,"shaders/index.json",0,0,true);index=draw_json(raw);if(![index isKindOfClass:NSDictionary.class])goto cleanup;
 unsigned selected=0,modules=0,pipelines=0,errors=0,untranslated=0,clip=0,stages[3]={0};
 for(NSString *hash in [[index allKeys] sortedArrayUsingSelector:@selector(compare:)]){@autoreleasepool{
  if(hash.length!=16)continue;uint64_t bits=strtoull(hash.UTF8String,NULL,16);if(bits%parts!=shard)continue;NSDictionary *rec=index[hash];if(![rec isKindOfClass:NSDictionary.class])continue;if(![rec[@"ok"] boolValue]){untranslated++;continue;}selected++;
  NSString *stage=rec[@"stage"];unsigned st=[stage isEqualToString:@"vs"]?0:[stage isEqualToString:@"ps"]?1:[stage isEqualToString:@"cs"]?2:3;
  if(st==3){printf("FAIL: audit shader %s unknown stage\n",hash.UTF8String);errors++;continue;}
  Object o={0};uint32_t meta[]={1,st,(uint32_t)bits,(uint32_t)(bits>>32),0};o.kind=14;o.meta=meta;
  DrawProgram *p=draw_program(g,&o);if(p&&!atomic_load(&g->failed)){modules++;stages[st]++;if(strstr(p->source,"enable clip_distances"))clip++;if(audit_pipeline(g,p))pipelines++;}
  if(atomic_load(&g->failed)){errors++;printf("FAIL: shader %s %s: %s\n",hash.UTF8String,stage.UTF8String,g->error);atomic_store(&g->failed,false);g->error[0]=0;}
  draw_program_free(o.program);wgpuDevicePoll(g->device,false,NULL);
  if((modules+errors)%200==0){printf("AUDIT progress: modules %u pipelines %u failures %u\n",modules,pipelines,errors);fflush(stdout);}
 }}
 result=errors||modules!=selected||pipelines!=selected?1:0;
 printf("%s: original shader audit shard %u/3; modules %u; native Metal pipelines %u; VS/PS/CS %u/%u/%u; clip shaders %u; failures %u; untranslated records %u (explicitly unavailable, not claimed fixed). No game scene or input is verified by compilation.\n",result?"FAIL":"PASS",shard,modules,pipelines,stages[0],stages[1],stages[2],clip,errors,untranslated);
 cleanup:if(g){if(native_gpu_error(g))puts(native_gpu_error(g));release_hardware(g);native_gpu_end(g);}if(h->game_root>=0)close(h->game_root);wasm_rt_free_memory_shared(&h->memory);pthread_mutex_destroy(&h->memory.mem_lock);free(h);wasm_rt_free_thread();return result;
}}
