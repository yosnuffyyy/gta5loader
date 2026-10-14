/* CI-only real VS/PS hashes from the 0.17 Prologue failure. Vertex attributes
   and color attachments are synthetic interface fixtures, not captured scene
   geometry or render-target formats. Production shader layouts/cache are used. */
typedef struct {_Atomic bool done;WGPUPopErrorScopeStatus status;WGPUErrorType type;char message[1000];} PairScope;
static void pair_scope_done(WGPUPopErrorScopeStatus status,WGPUErrorType type,WGPUStringView text,void *context,void *unused){(void)unused;PairScope *s=context;s->status=status;s->type=type;size_t n=text.data?(text.length==WGPU_STRLEN?strlen(text.data):text.length):0;if(n>sizeof(s->message)-1)n=sizeof(s->message)-1;memcpy(s->message,text.data?text.data:"",n);s->message[n]=0;atomic_store_explicit(&s->done,true,memory_order_release);}
static bool pair_original_rejects(NativeGpu *g,DrawProgram *vp,DrawProgram *pp){
 if(!check(g,g->inter_stage_limit==15,"Recorded-pair test must reproduce the phone's negotiated 15-variable limit"))return false;
 WGPUBindGroupLayout groups[]={vp->layout,pp->layout};WGPUPipelineLayoutDescriptor ld=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;ld.bindGroupLayoutCount=2;ld.bindGroupLayouts=groups;WGPUPipelineLayout layout=wgpuDeviceCreatePipelineLayout(g->device,&ld);if(!layout)return false;
 const unsigned formats[]={6,2,16,6,2};WGPUVertexAttribute attrs[5];unsigned stride=0;for(unsigned i=0;i<5;i++){unsigned bytes;attrs[i]=(WGPUVertexAttribute)WGPU_VERTEX_ATTRIBUTE_INIT;attrs[i].format=draw_vertex_format(formats[i],&bytes);attrs[i].shaderLocation=i;attrs[i].offset=stride;stride+=bytes;}
 WGPUVertexBufferLayout buffer=WGPU_VERTEX_BUFFER_LAYOUT_INIT;buffer.arrayStride=stride;buffer.attributeCount=5;buffer.attributes=attrs;
 WGPUColorTargetState colors[3];for(unsigned i=0;i<3;i++){colors[i]=(WGPUColorTargetState)WGPU_COLOR_TARGET_STATE_INIT;colors[i].format=i?WGPUTextureFormat_R32Float:WGPUTextureFormat_RGBA8Unorm;}
 WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;fragment.module=pp->module;fragment.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};fragment.targetCount=3;fragment.targets=colors;
 WGPURenderPipelineDescriptor d=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;d.layout=layout;d.vertex.module=vp->module;d.vertex.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};d.vertex.bufferCount=1;d.vertex.buffers=&buffer;d.fragment=&fragment;d.label=(WGPUStringView){"original recorded pair: expected location15 rejection",WGPU_STRLEN};
 wgpuDevicePushErrorScope(g->device,WGPUErrorFilter_Validation);WGPURenderPipeline pipeline=wgpuDeviceCreateRenderPipeline(g->device,&d);PairScope *scope=calloc(1,sizeof(*scope));if(!scope){if(pipeline)wgpuRenderPipelineRelease(pipeline);wgpuPipelineLayoutRelease(layout);return false;}g->test_scope_context=scope;WGPUPopErrorScopeCallbackInfo cb=WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;cb.mode=WGPUCallbackMode_AllowProcessEvents;cb.callback=pair_scope_done;cb.userdata1=scope;wgpuDevicePopErrorScope(g->device,cb);
 uint64_t deadline=now_ns()+15000000000ull;while(!atomic_load_explicit(&scope->done,memory_order_acquire)){wgpuDevicePoll(g->device,false,NULL);if(now_ns()>deadline){/* Keep callback context alive until the device is drained. */wgpuDevicePoll(g->device,true,NULL);break;}struct timespec t={0,1000000};nanosleep(&t,NULL);}
 if(pipeline)wgpuRenderPipelineRelease(pipeline);wgpuPipelineLayoutRelease(layout);
 bool ok=atomic_load(&scope->done)&&scope->status==WGPUPopErrorScopeStatus_Success&&scope->type==WGPUErrorType_Validation&&strstr(scope->message,"Location[15]")!=NULL;
 if(atomic_load(&scope->done)){free(scope);g->test_scope_context=NULL;}if(ok)puts("PASS: original exact Prologue pair rejected with Location[15] under a real requested device limit of 15; expected validation captured only inside this negative CI test.");return check(g,ok,"Recorded original pair did not reproduce the reported location15 failure");
}
static bool audit_recorded_pair(NativeGpu *g){@autoreleasepool{
 const uint64_t hashes[]={UINT64_C(0x7db515e410659eca),UINT64_C(0x394c572f727d25e8)};
 Object *shaders[2]={0};bool ok=false;reset_state(g);
 for(unsigned stage=0;stage<2;stage++){shaders[stage]=create(g,8100+stage,14);if(!shaders[stage])goto cleanup;
  unsigned words=stage?5:25;shaders[stage]->words=words;shaders[stage]->meta=calloc(words,4);if(!shaders[stage]->meta)goto cleanup;
  uint32_t *m=shaders[stage]->meta;m[0]=8100+stage;m[1]=stage;m[2]=(uint32_t)hashes[stage];m[3]=hashes[stage]>>32;m[4]=stage?0:5;
  if(!stage)for(unsigned i=0;i<5;i++){m[5+i*4]=i+1;m[7+i*4]=i;m[8+i*4]=15;}
  g->shader[stage]=8100+stage;
 }
 Object *input=create(g,8102,15);if(!input)goto cleanup;input->words=37;input->meta=calloc(37,4);if(!input->meta)goto cleanup;input->meta[0]=8102;input->meta[1]=5;
 const unsigned formats[]={6,2,16,6,2},sizes[]={12,16,8,12,16};unsigned stride=0;
 for(unsigned i=0;i<5;i++){uint32_t *e=input->meta+2+i*7;e[0]=i+1;e[2]=formats[i];e[4]=stride;stride+=sizes[i];}g->layout=8102;g->vb[0][0]=8103;g->vb[0][1]=stride;
 for(unsigned i=0;i<3;i++){Object *o=create(g,8104+i,3);if(!o)goto cleanup;o->td=(WGPUTextureDescriptor)WGPU_TEXTURE_DESCRIPTOR_INIT;o->td.dimension=WGPUTextureDimension_2D;o->td.size=(WGPUExtent3D){1,1,1};o->td.mipLevelCount=o->td.sampleCount=1;o->td.format=i?WGPUTextureFormat_R32Float:WGPUTextureFormat_RGBA8Unorm;o->td.usage=WGPUTextureUsage_RenderAttachment;o->texture=wgpuDeviceCreateTexture(g->device,&o->td);if(!o->texture||atomic_load(&g->failed))goto cleanup;g->targets[i*4]=o->id;}
 DrawProgram *vp=draw_program(g,shaders[0]),*pp=draw_program(g,shaders[1]);if(!vp||!pp||atomic_load(&g->failed))goto cleanup;
 if(!pair_original_rejects(g,vp,pp)||!draw_pipeline(g,shaders[0],shaders[1],vp,pp))goto cleanup;
 uint64_t hits=g->draw.hits,creates=g->draw.creates;if(!draw_pipeline(g,shaders[0],shaders[1],vp,pp)||g->draw.hits!=hits+1||g->draw.creates!=creates)goto cleanup;
 ok=!atomic_load(&g->failed);if(ok)puts("PASS: recorded Prologue VS 7db515e410659eca + PS 394c572f727d25e8: real modules, packed bindings, special constants, clip outputs and production cached pipeline. Synthetic vertex/attachment fixtures; scene pixels not claimed.");
 cleanup:reset_state(g);for(unsigned i=8100;i<=8106;i++){Object *o=object(g,i);if(o){CFDictionaryRemoveValue(g->objects,(void*)(uintptr_t)i);destroy(o);}}return ok;
}}
