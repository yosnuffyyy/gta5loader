#include "native_renderer.h"
#include "shader_fixture.h"
#include <webgpu/wgpu.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

enum { SIDE=64, ROW=SIDE*4, PIXELS=ROW*SIDE, VS_FLOATS=84*4, PS_FLOATS=(18+86)*4 };
typedef struct {
 WGPUInstance instance; WGPUAdapter adapter; WGPUDevice device; WGPUQueue queue;
 WGPUShaderModule vs,ps; WGPURenderPipeline pipeline;
 WGPUBindGroupLayout layouts[2]; WGPUBindGroup groups[2];
 WGPUBuffer vertices,vs_uniforms,ps_uniforms,readback;
 WGPUTexture input,target; WGPUTextureView input_view,target_view; WGPUSampler sampler;
 atomic_bool adapter_done,device_done,map_done,closing;
 atomic_uint errors; WGPUMapAsyncStatus map_status;
 pthread_mutex_t lock; char first_error[1200]; char adapter_name[192];
} Probe;
static double seconds(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}
static void error(Probe *p,const char *phase,WGPUStringView message){
 pthread_mutex_lock(&p->lock);
 if(atomic_fetch_add(&p->errors,1)==0){size_t n=message.data?(message.length==WGPU_STRLEN?strlen(message.data):message.length):0;if(n>900)n=900;snprintf(p->first_error,sizeof(p->first_error),"%s: %.*s",phase,(int)n,message.data?message.data:"");}
 pthread_mutex_unlock(&p->lock);
}
static void adapter_cb(WGPURequestAdapterStatus s,WGPUAdapter a,WGPUStringView m,void *u,void *v){
 (void)v;Probe *p=u;p->adapter=a;if(s!=WGPURequestAdapterStatus_Success)error(p,"Metal adapter request",m);atomic_store_explicit(&p->adapter_done,true,memory_order_release);
}
static void device_cb(WGPURequestDeviceStatus s,WGPUDevice d,WGPUStringView m,void *u,void *v){
 (void)v;Probe *p=u;p->device=d;if(s!=WGPURequestDeviceStatus_Success)error(p,"device request",m);atomic_store_explicit(&p->device_done,true,memory_order_release);
}
static void error_cb(WGPUDevice const *d,WGPUErrorType t,WGPUStringView m,void *u,void *v){(void)d;(void)t;(void)v;error(u,"WebGPU validation/runtime",m);}
static void lost_cb(WGPUDevice const *d,WGPUDeviceLostReason r,WGPUStringView m,void *u,void *v){
 (void)d;(void)r;(void)v;Probe *p=u;if(!atomic_load(&p->closing))error(p,"device lost",m);
}
static void map_cb(WGPUMapAsyncStatus s,WGPUStringView m,void *u,void *v){
 (void)v;Probe *p=u;p->map_status=s;if(s!=WGPUMapAsyncStatus_Success)error(p,"readback mapping",m);atomic_store_explicit(&p->map_done,true,memory_order_release);
}
static bool wait_for(Probe *p,atomic_bool *done){
 double deadline=seconds()+15;
 while(!atomic_load_explicit(done,memory_order_acquire)){
  if(p->device)wgpuDevicePoll(p->device,false,NULL);
  wgpuInstanceProcessEvents(p->instance);
  if(seconds()>deadline){error(p,"timeout",(WGPUStringView){"native GPU callback exceeded 15 seconds",WGPU_STRLEN});return false;}
  struct timespec delay={0,1000000};nanosleep(&delay,NULL);
 }
 return true;
}
static WGPUBuffer buffer(Probe *p,uint64_t size,WGPUBufferUsage usage){WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=size;d.usage=usage;return wgpuDeviceCreateBuffer(p->device,&d);}
static WGPUTexture texture(Probe *p,WGPUTextureUsage usage){WGPUTextureDescriptor d=WGPU_TEXTURE_DESCRIPTOR_INIT;d.dimension=WGPUTextureDimension_2D;d.size=(WGPUExtent3D){SIDE,SIDE,1};d.format=WGPUTextureFormat_RGBA8Unorm;d.mipLevelCount=1;d.sampleCount=1;d.usage=usage;return wgpuDeviceCreateTexture(p->device,&d);}
static WGPUShaderModule shader(Probe *p,const char *text,const char *label){
 WGPUShaderSourceWGSL src=WGPU_SHADER_SOURCE_WGSL_INIT;src.code=(WGPUStringView){text,WGPU_STRLEN};
 WGPUShaderModuleDescriptor d=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;d.nextInChain=&src.chain;d.label=(WGPUStringView){label,WGPU_STRLEN};return wgpuDeviceCreateShaderModule(p->device,&d);
}
static void source_pixel(unsigned x,unsigned y,uint8_t out[4]){
 static const uint8_t colors[4][4]={{240,80,32,200},{32,160,240,128},{80,240,160,240},{160,32,80,64}};
 memcpy(out,colors[(y>=SIDE/2)*2+(x>=SIDE/2)],4);
}
static bool draw_and_check(Probe *p,unsigned pass,uint8_t *output){
 const float rgb[3]={.75f,.5f,1.f},alpha[3]={.5f,1.f,.25f};
 float ps[PS_FLOATS]={0};ps[17*4+3]=rgb[pass];ps[(18+85)*4]=alpha[pass];
 wgpuQueueWriteBuffer(p->queue,p->ps_uniforms,0,ps,sizeof(ps));
 WGPUCommandEncoderDescriptor ed=WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;WGPUCommandEncoder e=wgpuDeviceCreateCommandEncoder(p->device,&ed);if(!e)return false;
 WGPURenderPassColorAttachment color=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;color.view=p->target_view;color.loadOp=WGPULoadOp_Clear;color.storeOp=WGPUStoreOp_Store;color.clearValue=(WGPUColor){0,0,0,0};
 WGPURenderPassDescriptor rd=WGPU_RENDER_PASS_DESCRIPTOR_INIT;rd.colorAttachmentCount=1;rd.colorAttachments=&color;
 WGPURenderPassEncoder r=wgpuCommandEncoderBeginRenderPass(e,&rd);if(!r){wgpuCommandEncoderRelease(e);return false;}
 wgpuRenderPassEncoderSetPipeline(r,p->pipeline);
 wgpuRenderPassEncoderSetVertexBuffer(r,0,p->vertices,0,48);
 for(unsigned i=0;i<2;i++)wgpuRenderPassEncoderSetBindGroup(r,i,p->groups[i],0,NULL);
 wgpuRenderPassEncoderDraw(r,6,1,0,0);wgpuRenderPassEncoderEnd(r);wgpuRenderPassEncoderRelease(r);
 WGPUTexelCopyTextureInfo from=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;from.texture=p->target;
 WGPUTexelCopyBufferInfo to=WGPU_TEXEL_COPY_BUFFER_INFO_INIT;to.buffer=p->readback;to.layout.bytesPerRow=ROW;to.layout.rowsPerImage=SIDE;
 WGPUExtent3D extent={SIDE,SIDE,1};wgpuCommandEncoderCopyTextureToBuffer(e,&from,&to,&extent);
 WGPUCommandBufferDescriptor cd=WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;WGPUCommandBuffer c=wgpuCommandEncoderFinish(e,&cd);wgpuCommandEncoderRelease(e);if(!c)return false;
 wgpuQueueSubmit(p->queue,1,&c);wgpuCommandBufferRelease(c);
 atomic_store_explicit(&p->map_done,false,memory_order_release);
 WGPUBufferMapCallbackInfo cb=WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;cb.mode=WGPUCallbackMode_AllowProcessEvents;cb.callback=map_cb;cb.userdata1=p;
 wgpuBufferMapAsync(p->readback,WGPUMapMode_Read,0,PIXELS,cb);
 if(!wait_for(p,&p->map_done))return false;
 if(p->map_status!=WGPUMapAsyncStatus_Success)return false;
 const uint8_t *bytes=wgpuBufferGetConstMappedRange(p->readback,0,PIXELS);
 if(!bytes){wgpuBufferUnmap(p->readback);return false;}
 bool matches=true;
 for(unsigned y=0;y<SIDE&&matches;y++)for(unsigned x=0;x<SIDE&&matches;x++){
  uint8_t src[4];source_pixel(x,y,src);
  for(unsigned k=0;k<4;k++){int expected=(int)(src[k]*(k==3?alpha[pass]:rgb[pass])+.5f);int actual=bytes[(y*SIDE+x)*4+k];if(abs(actual-expected)>1){char m[192];snprintf(m,sizeof(m),"draw %u pixel %u,%u channel %u: got %d expected %d",pass,x,y,k,actual,expected);error(p,"game shader output differs",(WGPUStringView){m,WGPU_STRLEN});matches=false;break;}}
 }
 if(output&&matches)memcpy(output,bytes,PIXELS);
 wgpuBufferUnmap(p->readback);return matches&&atomic_load(&p->errors)==0;
}
static void cleanup(Probe *p){
 atomic_store(&p->closing,true);
 if(p->readback)wgpuBufferUnmap(p->readback);
 for(unsigned i=0;i<2;i++){if(p->groups[i])wgpuBindGroupRelease(p->groups[i]);if(p->layouts[i])wgpuBindGroupLayoutRelease(p->layouts[i]);}
 if(p->pipeline)wgpuRenderPipelineRelease(p->pipeline);
 if(p->sampler)wgpuSamplerRelease(p->sampler);
 if(p->input_view)wgpuTextureViewRelease(p->input_view);if(p->target_view)wgpuTextureViewRelease(p->target_view);
 if(p->input){wgpuTextureDestroy(p->input);wgpuTextureRelease(p->input);}if(p->target){wgpuTextureDestroy(p->target);wgpuTextureRelease(p->target);}
 WGPUBuffer buffers[]={p->vertices,p->vs_uniforms,p->ps_uniforms,p->readback};for(unsigned i=0;i<4;i++)if(buffers[i]){wgpuBufferDestroy(buffers[i]);wgpuBufferRelease(buffers[i]);}
 if(p->vs)wgpuShaderModuleRelease(p->vs);if(p->ps)wgpuShaderModuleRelease(p->ps);
 if(p->queue)wgpuQueueRelease(p->queue);
 if(p->device){wgpuDeviceDestroy(p->device);wgpuDeviceRelease(p->device);}
 if(p->adapter)wgpuAdapterRelease(p->adapter);
 if(p->instance)wgpuInstanceRelease(p->instance);
}
int native_renderer_probe(char *report,size_t capacity,uint8_t *rgba,size_t rgba_capacity){
 if(!report||!capacity||(rgba&&rgba_capacity<PIXELS))return 1;
 Probe p={0};pthread_mutex_init(&p.lock,NULL);const char *phase="Metal backend initialization";int result=1;
 WGPUInstanceExtras extra={0};extra.chain.sType=(WGPUSType)WGPUSType_InstanceExtras;extra.backends=WGPUInstanceBackend_Metal;
 WGPUInstanceDescriptor id=WGPU_INSTANCE_DESCRIPTOR_INIT;id.nextInChain=&extra.chain;p.instance=wgpuCreateInstance(&id);if(!p.instance)goto done;
 WGPURequestAdapterOptions ao=WGPU_REQUEST_ADAPTER_OPTIONS_INIT;ao.backendType=WGPUBackendType_Metal;ao.powerPreference=WGPUPowerPreference_LowPower;
 WGPURequestAdapterCallbackInfo ac=WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;ac.mode=WGPUCallbackMode_AllowProcessEvents;ac.callback=adapter_cb;ac.userdata1=&p;wgpuInstanceRequestAdapter(p.instance,&ao,ac);
 if(!wait_for(&p,&p.adapter_done)||!p.adapter)goto done;
 WGPUAdapterInfo ai=WGPU_ADAPTER_INFO_INIT;if(wgpuAdapterGetInfo(p.adapter,&ai)!=WGPUStatus_Success)goto done;
 if(ai.backendType!=WGPUBackendType_Metal){wgpuAdapterInfoFreeMembers(ai);goto done;}
 size_t an=ai.device.length==WGPU_STRLEN?(ai.device.data?strlen(ai.device.data):0):ai.device.length;if(an>sizeof(p.adapter_name)-1)an=sizeof(p.adapter_name)-1;snprintf(p.adapter_name,sizeof(p.adapter_name),"%.*s",(int)an,ai.device.data?ai.device.data:"");wgpuAdapterInfoFreeMembers(ai);
 WGPUDeviceDescriptor dd=WGPU_DEVICE_DESCRIPTOR_INIT;dd.label=(WGPUStringView){"GTA renderer feasibility",WGPU_STRLEN};dd.uncapturedErrorCallbackInfo.callback=error_cb;dd.uncapturedErrorCallbackInfo.userdata1=&p;dd.deviceLostCallbackInfo.mode=WGPUCallbackMode_AllowProcessEvents;dd.deviceLostCallbackInfo.callback=lost_cb;dd.deviceLostCallbackInfo.userdata1=&p;
 WGPURequestDeviceCallbackInfo dc=WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;dc.mode=WGPUCallbackMode_AllowProcessEvents;dc.callback=device_cb;dc.userdata1=&p;wgpuAdapterRequestDevice(p.adapter,&dd,dc);
 if(!wait_for(&p,&p.device_done)||!p.device)goto done;p.queue=wgpuDeviceGetQueue(p.device);if(!p.queue)goto done;
 phase="original postfx WGSL and packed uniform layouts";
 p.vs=shader(&p,fixture_vs,"postfx::VS_Passthrough b8698e91b33c5e70");p.ps=shader(&p,fixture_ps,"postfx::PS_PassThrough 6d214da591105085");if(!p.vs||!p.ps||atomic_load(&p.errors))goto done;
 WGPUVertexAttribute attr=WGPU_VERTEX_ATTRIBUTE_INIT;attr.format=WGPUVertexFormat_Float32x2;attr.shaderLocation=0;
 WGPUVertexBufferLayout vb=WGPU_VERTEX_BUFFER_LAYOUT_INIT;vb.arrayStride=8;vb.stepMode=WGPUVertexStepMode_Vertex;vb.attributeCount=1;vb.attributes=&attr;
 WGPUColorTargetState ct=WGPU_COLOR_TARGET_STATE_INIT;ct.format=WGPUTextureFormat_RGBA8Unorm;ct.writeMask=WGPUColorWriteMask_All;
 WGPUFragmentState frag=WGPU_FRAGMENT_STATE_INIT;frag.module=p.ps;frag.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};frag.targetCount=1;frag.targets=&ct;
 WGPURenderPipelineDescriptor pd=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;pd.vertex.module=p.vs;pd.vertex.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};pd.vertex.bufferCount=1;pd.vertex.buffers=&vb;pd.fragment=&frag;pd.primitive.topology=WGPUPrimitiveTopology_TriangleList;pd.primitive.frontFace=WGPUFrontFace_CCW;pd.primitive.cullMode=WGPUCullMode_None;
 p.pipeline=wgpuDeviceCreateRenderPipeline(p.device,&pd);if(!p.pipeline||atomic_load(&p.errors))goto done;
 p.vertices=buffer(&p,48,WGPUBufferUsage_Vertex|WGPUBufferUsage_CopyDst);p.vs_uniforms=buffer(&p,VS_FLOATS*4,WGPUBufferUsage_Uniform|WGPUBufferUsage_CopyDst);p.ps_uniforms=buffer(&p,PS_FLOATS*4,WGPUBufferUsage_Uniform|WGPUBufferUsage_CopyDst);p.readback=buffer(&p,PIXELS,WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead);
 p.input=texture(&p,WGPUTextureUsage_CopyDst|WGPUTextureUsage_TextureBinding);p.target=texture(&p,WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_CopySrc);
 if(!p.vertices||!p.vs_uniforms||!p.ps_uniforms||!p.readback||!p.input||!p.target)goto done;
 p.input_view=wgpuTextureCreateView(p.input,NULL);p.target_view=wgpuTextureCreateView(p.target,NULL);WGPUSamplerDescriptor sd=WGPU_SAMPLER_DESCRIPTOR_INIT;sd.magFilter=WGPUFilterMode_Nearest;sd.minFilter=WGPUFilterMode_Nearest;sd.mipmapFilter=WGPUMipmapFilterMode_Nearest;p.sampler=wgpuDeviceCreateSampler(p.device,&sd);if(!p.input_view||!p.target_view||!p.sampler)goto done;
 for(unsigned i=0;i<2;i++){p.layouts[i]=wgpuRenderPipelineGetBindGroupLayout(p.pipeline,i);if(!p.layouts[i])goto done;}
 WGPUBindGroupEntry entries[3]={WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT};entries[0].binding=0;entries[0].buffer=p.vs_uniforms;entries[0].size=VS_FLOATS*4;
 WGPUBindGroupDescriptor bg=WGPU_BIND_GROUP_DESCRIPTOR_INIT;bg.layout=p.layouts[0];bg.entryCount=1;bg.entries=entries;p.groups[0]=wgpuDeviceCreateBindGroup(p.device,&bg);
 entries[0].buffer=p.ps_uniforms;entries[0].size=PS_FLOATS*4;entries[1].binding=21;entries[1].sampler=p.sampler;entries[2].binding=37;entries[2].textureView=p.input_view;bg.layout=p.layouts[1];bg.entryCount=3;p.groups[1]=wgpuDeviceCreateBindGroup(p.device,&bg);if(!p.groups[0]||!p.groups[1]||atomic_load(&p.errors))goto done;
 const float verts[12]={0,0,1,0,0,1,0,1,1,0,1,1};float vs[VS_FLOATS]={0};vs[82*4]=2;vs[82*4+1]=-2;vs[82*4+2]=-1;vs[82*4+3]=1;vs[83*4]=1;vs[83*4+1]=1;
 wgpuQueueWriteBuffer(p.queue,p.vertices,0,verts,sizeof(verts));wgpuQueueWriteBuffer(p.queue,p.vs_uniforms,0,vs,sizeof(vs));
 uint8_t input[PIXELS];for(unsigned y=0;y<SIDE;y++)for(unsigned x=0;x<SIDE;x++)source_pixel(x,y,input+(y*SIDE+x)*4);
 WGPUTexelCopyTextureInfo ti=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;ti.texture=p.input;WGPUTexelCopyBufferLayout tl=WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;tl.bytesPerRow=ROW;tl.rowsPerImage=SIDE;WGPUExtent3D extent={SIDE,SIDE,1};wgpuQueueWriteTexture(p.queue,&ti,input,sizeof(input),&tl,&extent);
 phase="three actual shader draws, GPU completion and pixel comparison";
 for(unsigned i=0;i<3;i++)if(!draw_and_check(&p,i,rgba))goto done;
 if(atomic_load(&p.errors))goto done;result=0;
done:
 cleanup(&p);
 if(atomic_load(&p.errors))result=1;
 if(!result)snprintf(report,capacity,"Native renderer feasibility 0.5\nBackend: wgpu-native 29.0.1.1 / Metal (%s)\nPASS: original postfx vertex + fragment WGSL with the browser's packed constant-buffer layouts.\nPASS: texture/sampler binding, vertex input, three draws with changed RGB/alpha uniforms, all 4,096 pixels compared per draw (1-byte tolerance), GPU readback and resource cleanup.\nValidation/runtime errors: 0\nThis is an offscreen shader fixture, not an engine scene. Ring integration, remaining shader/format coverage, audio and gameplay are pending.",p.adapter_name);
 else snprintf(report,capacity,"Native renderer feasibility 0.5\nFAIL during %s\n%s\nValidation/runtime errors: %u\nNo gameplay compatibility is claimed.",phase,p.first_error[0]?p.first_error:"Resource allocation or backend operation failed",atomic_load(&p.errors));
 pthread_mutex_destroy(&p.lock);return result;
}
