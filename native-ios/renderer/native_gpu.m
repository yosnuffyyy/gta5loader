#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#include "native_gpu.h"
#include "native_launch.h"
#include "gpu_ring.h"
#include "native_wait.h"
#include "wasm-rt-impl.h"
#include "wasm-rt-exceptions.h"
#include <webgpu/wgpu.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <mach/mach.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "draw_types.inc"

typedef struct {
 uint32_t id,kind,size,bind,misc,stride,words;bool a8;
 uint32_t *meta;uint8_t *shadow;
 WGPUBuffer buffer;WGPUTexture texture;
 WGPUSampler sampler; DrawProgram *program;
 WGPUTextureDescriptor td;WGPUTextureFormat views[2];
} Object;
struct NativeGpu {
 struct w2c_env *host;GpuRing ring;pthread_t thread;bool started,ring_valid;
 _Atomic bool stop,failed,ready,closing,callback_done,map_active;
 pthread_mutex_t error_lock;char error[1400];char adapter_name[192];
 void (*cancel)(void*);void *context;void *layer;
 WGPUInstance instance;WGPUAdapter adapter;WGPUDevice device;WGPUQueue queue;
 WGPUSurface surface;WGPURenderPipeline blit;WGPUShaderModule blit_shader;
 CFMutableDictionaryRef objects;uint64_t presents,surface_frames,uploads,readbacks;
 uint64_t buffer_bytes,texture_bytes;_Atomic uint32_t last_op;
 _Atomic uint32_t live_phase;
 _Atomic uint64_t live_commands,live_frames,live_draws,live_shader;
 _Atomic uint64_t drawable_timeouts;
 uint64_t last_sample,last_report;double highest_footprint;
 bool bc_enabled,format_caps_enabled,test_bc;unsigned bc_formats_tested,snorm_cases_tested;
 DrawState draw;
 uint32_t shader[3],layout,topology,vb[16][3],ib[3],cb[3][16][3],srv[3][128],smp[3][16],uav[3][8];
 uint32_t targets[37],blend[6],depth[2],raster,viewports[97],scissors[65];
};
static uint64_t now_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
static void sample_memory(NativeGpu *g){uint64_t now=now_ns();if(now-g->last_sample<1000000000ull)return;g->last_sample=now;
 task_vm_info_data_t info={0};mach_msg_type_number_t count=TASK_VM_INFO_COUNT;if(task_info(mach_task_self(),TASK_VM_INFO,(task_info_t)&info,&count)!=KERN_SUCCESS)return;
 double footprint=(double)info.phys_footprint/1048576.0;if(footprint>g->highest_footprint)g->highest_footprint=footprint;
 if(g->host->progress&&now-g->last_report>=5000000000ull){g->last_report=now;char line[320];snprintf(line,sizeof(line),"Native startup: process %.1f MiB (highest 1-second sample %.1f MiB); heap committed %.1f MiB; GPU commands %llu, surface frames %llu, last %s",footprint,g->highest_footprint,(double)atomic_load(&g->host->memory.size)/1048576.0,(unsigned long long)g->ring.executed,(unsigned long long)g->surface_frames,gpu_opcode_name(g->last_op));g->host->progress(g->host->progress_context,line);}
}
static _Atomic uint32_t *heap_word(NativeGpu *g,uint64_t address){return (_Atomic uint32_t*)(g->host->memory.data+address);}
static void wake(void *context,uint64_t address){NativeGpu *g=context;native_notify(&g->host->memory,address,UINT32_MAX);}
static void fail(NativeGpu *g,const char *message){
 pthread_mutex_lock(&g->error_lock);bool first=!atomic_load_explicit(&g->failed,memory_order_relaxed);
 if(first){snprintf(g->error,sizeof(g->error),"%s [%s, opcode %u]",message,gpu_opcode_name(g->last_op),g->last_op);atomic_store_explicit(&g->failed,true,memory_order_release);}
 pthread_mutex_unlock(&g->error_lock);
 if(first){if(g->ring_valid)gpu_ring_failed(&g->ring);if(g->cancel)g->cancel(g->context);}
}
static bool check(NativeGpu *g,bool value,const char *why){if(!value)fail(g,why);return value;}
static void device_error(WGPUDevice const *d,WGPUErrorType type,WGPUStringView s,void *ctx,void *unused){(void)d;(void)type;(void)unused;NativeGpu *g=ctx;char text[1100];size_t n=s.data?(s.length==WGPU_STRLEN?strlen(s.data):s.length):0;if(n>1000)n=1000;snprintf(text,sizeof(text),"Native WebGPU: %.*s",(int)n,s.data?s.data:"");atomic_fetch_add(heap_word(g,g->ring.base+16),1);fail(g,text);}
static void lost(WGPUDevice const *d,WGPUDeviceLostReason reason,WGPUStringView s,void *ctx,void *unused){(void)d;(void)reason;NativeGpu *g=ctx;if(!atomic_load(&g->closing))device_error(d,WGPUErrorType_Unknown,s,ctx,unused);}
static void got_adapter(WGPURequestAdapterStatus s,WGPUAdapter a,WGPUStringView m,void *ctx,void *unused){(void)unused;NativeGpu *g=ctx;g->adapter=a;if(s!=WGPURequestAdapterStatus_Success)fail(g,"Native Metal adapter request failed");(void)m;atomic_store(&g->callback_done,true);}
static void got_device(WGPURequestDeviceStatus s,WGPUDevice d,WGPUStringView m,void *ctx,void *unused){(void)unused;NativeGpu *g=ctx;g->device=d;if(s!=WGPURequestDeviceStatus_Success)fail(g,"Native Metal device request failed");(void)m;atomic_store(&g->callback_done,true);}
static void work_done(WGPUQueueWorkDoneStatus s,WGPUStringView m,void *ctx,void *unused){(void)m;(void)unused;NativeGpu *g=ctx;if(s!=WGPUQueueWorkDoneStatus_Success&&!atomic_load(&g->stop)&&!atomic_load(&g->closing))fail(g,"Native GPU submission failed");atomic_store(&g->callback_done,true);}
static void mapped(WGPUMapAsyncStatus s,WGPUStringView m,void *ctx,void *unused){(void)m;(void)unused;NativeGpu *g=ctx;if(s!=WGPUMapAsyncStatus_Success){atomic_store(&g->map_active,false);if(!atomic_load(&g->stop)&&!atomic_load(&g->closing))fail(g,"Native GPU readback map failed");}atomic_store(&g->callback_done,true);}
static bool wait_callback(NativeGpu *g){uint64_t deadline=now_ns()+15000000000ull;while(!atomic_load(&g->callback_done)){
 if(g->device)wgpuDevicePoll(g->device,false,NULL);else wgpuInstanceProcessEvents(g->instance);
 if(atomic_load(&g->stop)||atomic_load(&g->failed))return false;
 if(now_ns()>deadline){fail(g,"Native GPU callback timeout (15 seconds)");return false;}
 struct timespec d={0,1000000};nanosleep(&d,NULL);
 }return !atomic_load(&g->failed);}
static bool completed(NativeGpu *g){uint32_t previous=atomic_exchange(&g->live_phase,6);atomic_store(&g->callback_done,false);WGPUQueueWorkDoneCallbackInfo c=WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;c.mode=WGPUCallbackMode_AllowProcessEvents;c.callback=work_done;c.userdata1=g;wgpuQueueOnSubmittedWorkDone(g->queue,c);bool ok=wait_callback(g);atomic_store(&g->live_phase,previous);return ok;}
static Object *object(NativeGpu *g,uint32_t id){return id?(Object*)CFDictionaryGetValue(g->objects,(const void*)(uintptr_t)id):NULL;}
static Object *create(NativeGpu *g,uint32_t id,uint32_t kind){if(!check(g,id&&!object(g,id),"Zero or duplicate GPU object ID"))return NULL;Object *o=calloc(1,sizeof(*o));if(!check(g,o!=NULL,"GPU object metadata allocation"))return NULL;o->id=id;o->kind=kind;CFDictionarySetValue(g->objects,(const void*)(uintptr_t)id,o);return o;}
static void draw_program_free(DrawProgram *);
static void destroy(Object *o){if(!o)return;if(o->sampler)wgpuSamplerRelease(o->sampler);draw_program_free(o->program);if(o->buffer){wgpuBufferDestroy(o->buffer);wgpuBufferRelease(o->buffer);}if(o->texture){wgpuTextureDestroy(o->texture);wgpuTextureRelease(o->texture);}free(o->shadow);free(o->meta);free(o);}
static void destroy_entry(const void *k,const void *v,void *c){(void)k;(void)c;destroy((Object*)v);}
static WGPUTextureFormat format(uint32_t dxgi,uint32_t bind){switch(dxgi){
 case 2:return WGPUTextureFormat_RGBA32Float;case 9:case 10:case 11:return WGPUTextureFormat_RGBA16Float;
 case 16:return WGPUTextureFormat_RG32Float;case 23:case 24:return WGPUTextureFormat_RGB10A2Unorm;case 26:return WGPUTextureFormat_RG11B10Ufloat;
 case 27:case 28:return WGPUTextureFormat_RGBA8Unorm;case 29:return WGPUTextureFormat_RGBA8UnormSrgb;case 31:return WGPUTextureFormat_RGBA8Snorm;
 case 33:case 34:case 35:return WGPUTextureFormat_RG16Float;case 39:return bind&0x40?WGPUTextureFormat_Depth32Float:WGPUTextureFormat_R32Float;
 case 40:return WGPUTextureFormat_Depth32Float;case 41:return WGPUTextureFormat_R32Float;
 case 19:case 20:return WGPUTextureFormat_Depth32FloatStencil8;
 case 21:case 22:case 44:case 45:case 46:case 47:return WGPUTextureFormat_Depth24PlusStencil8;
 case 48:case 49:return WGPUTextureFormat_RG8Unorm;case 51:return WGPUTextureFormat_RG8Snorm;
 case 53:return bind&0x40?WGPUTextureFormat_Depth16Unorm:WGPUTextureFormat_R16Float;case 54:return WGPUTextureFormat_R16Float;case 55:case 56:return WGPUTextureFormat_Depth16Unorm;
 case 65:return WGPUTextureFormat_RGBA8Unorm;
 case 60:case 61:return WGPUTextureFormat_R8Unorm;case 63:return WGPUTextureFormat_R8Snorm;case 67:return WGPUTextureFormat_RGB9E5Ufloat;
 case 70:case 71:return WGPUTextureFormat_BC1RGBAUnorm;case 72:return WGPUTextureFormat_BC1RGBAUnormSrgb;
 case 73:case 74:return WGPUTextureFormat_BC2RGBAUnorm;case 75:return WGPUTextureFormat_BC2RGBAUnormSrgb;
 case 76:case 77:return WGPUTextureFormat_BC3RGBAUnorm;case 78:return WGPUTextureFormat_BC3RGBAUnormSrgb;
 case 79:case 80:return WGPUTextureFormat_BC4RUnorm;case 82:case 83:return WGPUTextureFormat_BC5RGUnorm;
 case 87:case 88:case 90:return WGPUTextureFormat_BGRA8Unorm;case 91:return WGPUTextureFormat_BGRA8UnormSrgb;
 case 94:case 95:return WGPUTextureFormat_BC6HRGBUfloat;case 97:case 98:return WGPUTextureFormat_BC7RGBAUnorm;case 99:return WGPUTextureFormat_BC7RGBAUnormSrgb;
 default:return WGPUTextureFormat_Undefined;
}}
static bool hardware(NativeGpu *g){
 WGPUInstanceExtras extra={0};extra.chain.sType=(WGPUSType)WGPUSType_InstanceExtras;extra.backends=WGPUInstanceBackend_Metal;
 WGPUInstanceDescriptor id=WGPU_INSTANCE_DESCRIPTOR_INIT;id.nextInChain=&extra.chain;g->instance=wgpuCreateInstance(&id);if(!check(g,g->instance!=NULL,"Native GPU instance creation"))return false;
 if(g->layer){WGPUSurfaceSourceMetalLayer metal=WGPU_SURFACE_SOURCE_METAL_LAYER_INIT;metal.layer=g->layer;WGPUSurfaceDescriptor sd=WGPU_SURFACE_DESCRIPTOR_INIT;sd.nextInChain=&metal.chain;g->surface=wgpuInstanceCreateSurface(g->instance,&sd);if(!check(g,g->surface!=NULL,"Native Metal surface creation"))return false;}
 WGPURequestAdapterOptions ao=WGPU_REQUEST_ADAPTER_OPTIONS_INIT;ao.backendType=WGPUBackendType_Metal;ao.compatibleSurface=g->surface;
 WGPURequestAdapterCallbackInfo ac=WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;ac.mode=WGPUCallbackMode_AllowProcessEvents;ac.callback=got_adapter;ac.userdata1=g;atomic_store(&g->callback_done,false);wgpuInstanceRequestAdapter(g->instance,&ao,ac);if(!wait_callback(g)||!g->adapter)return false;
 WGPUAdapterInfo ai=WGPU_ADAPTER_INFO_INIT;if(!check(g,wgpuAdapterGetInfo(g->adapter,&ai)==WGPUStatus_Success,"Native adapter information"))return false;size_t n=ai.device.length==WGPU_STRLEN?(ai.device.data?strlen(ai.device.data):0):ai.device.length;if(n>190)n=190;snprintf(g->adapter_name,sizeof(g->adapter_name),"%.*s",(int)n,ai.device.data?ai.device.data:"");wgpuAdapterInfoFreeMembers(ai);
 // The browser requests texture-formats-tier1 for SNORM render targets.
 // Pinned wgpu-native 29 does not map that web feature; its adapter-specific
 // format feature exposes Metal's checked render/blend capabilities instead.
 // Keep signed 8-bit storage: UNORM substitution loses negative values and
 // float expansion would increase the game's render-target memory.
 WGPUFeatureName wanted[]={WGPUFeatureName_TextureCompressionBC,WGPUFeatureName_TextureCompressionBCSliced3D,WGPUFeatureName_Depth32FloatStencil8,WGPUFeatureName_Float32Filterable,WGPUFeatureName_IndirectFirstInstance,WGPUFeatureName_RG11B10UfloatRenderable,WGPUFeatureName_BGRA8UnormStorage,WGPUFeatureName_DepthClipControl,(WGPUFeatureName)WGPUNativeFeature_TextureAdapterSpecificFormatFeatures};WGPUFeatureName features[sizeof(wanted)/sizeof(wanted[0])];size_t count=0;
 for(unsigned i=0;i<sizeof(wanted)/sizeof(wanted[0]);i++)if(wgpuAdapterHasFeature(g->adapter,wanted[i]))features[count++]=wanted[i];
 WGPULimits limits=WGPU_LIMITS_INIT;if(!check(g,wgpuAdapterGetLimits(g->adapter,&limits)==WGPUStatus_Success,"Native adapter limits query"))return false;limits.maxImmediateSize=WGPU_LIMIT_U32_UNDEFINED; /* This adapter does not use the optional immediates feature. */
 WGPUDeviceDescriptor dd=WGPU_DEVICE_DESCRIPTOR_INIT;dd.requiredLimits=&limits;dd.requiredFeatures=features;dd.requiredFeatureCount=count;dd.uncapturedErrorCallbackInfo.callback=device_error;dd.uncapturedErrorCallbackInfo.userdata1=g;dd.deviceLostCallbackInfo.mode=WGPUCallbackMode_AllowProcessEvents;dd.deviceLostCallbackInfo.callback=lost;dd.deviceLostCallbackInfo.userdata1=g;
 WGPURequestDeviceCallbackInfo dc=WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;dc.mode=WGPUCallbackMode_AllowProcessEvents;dc.callback=got_device;dc.userdata1=g;atomic_store(&g->callback_done,false);wgpuAdapterRequestDevice(g->adapter,&dd,dc);if(!wait_callback(g)||!g->device)return false;
 g->queue=wgpuDeviceGetQueue(g->device);if(!check(g,g->queue!=NULL,"Native GPU queue creation"))return false;
 g->bc_enabled=wgpuDeviceHasFeature(g->device,WGPUFeatureName_TextureCompressionBC);
 g->format_caps_enabled=wgpuDeviceHasFeature(g->device,(WGPUFeatureName)WGPUNativeFeature_TextureAdapterSpecificFormatFeatures);
 if(g->surface){WGPUSurfaceConfiguration cfg=WGPU_SURFACE_CONFIGURATION_INIT;cfg.device=g->device;cfg.format=WGPUTextureFormat_BGRA8Unorm;cfg.usage=WGPUTextureUsage_RenderAttachment;cfg.width=640;cfg.height=360;cfg.presentMode=WGPUPresentMode_Fifo;cfg.alphaMode=WGPUCompositeAlphaMode_Opaque;wgpuSurfaceConfigure(g->surface,&cfg);
  // wgpu-hal 29.0.3 configure disables this timeout. Our diagnostic view can
  // scroll offscreen; never let nextDrawable prevent Stop/join indefinitely.
  CAMetalLayer *layer=(__bridge CAMetalLayer*)g->layer;layer.allowsNextDrawableTimeout=YES;
  if(!check(g,layer.allowsNextDrawableTimeout,"Metal drawable timeout was not enabled"))return false;
 }
 return !atomic_load(&g->failed);
}
static WGPUCommandEncoder encoder(NativeGpu *g){WGPUCommandEncoderDescriptor d=WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;return wgpuDeviceCreateCommandEncoder(g->device,&d);}
static bool submit(NativeGpu *g,WGPUCommandEncoder e){if(!e)return check(g,false,"GPU encoder creation");WGPUCommandBufferDescriptor d=WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;WGPUCommandBuffer c=wgpuCommandEncoderFinish(e,&d);wgpuCommandEncoderRelease(e);if(!c)return check(g,false,"GPU command buffer creation");wgpuQueueSubmit(g->queue,1,&c);wgpuCommandBufferRelease(c);return !atomic_load(&g->failed);}
#include "bc_fixture.inc"
static WGPUTextureView view(Object *o,uint32_t mip,uint32_t slice){WGPUTextureViewDescriptor d=WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;d.dimension=WGPUTextureViewDimension_2D;d.baseMipLevel=mip;d.mipLevelCount=1;d.baseArrayLayer=slice;d.arrayLayerCount=1;return wgpuTextureCreateView(o->texture,&d);}
static bool create_buffer(NativeGpu *g,const uint8_t *p){Object *o=create(g,gpu_word(p,0),11);if(!o)return false;o->size=gpu_word(p,1);o->bind=gpu_word(p,2);o->misc=gpu_word(p,3);o->stride=gpu_word(p,4);if(!check(g,o->size&&o->size<=1024*1024*1024,"GPU buffer size unsupported"))return false;
 if(o->bind&4){o->shadow=calloc(1,o->size);if(!check(g,o->shadow!=NULL,"Constant-buffer shadow allocation"))return false;}
 if((o->bind&(1|2|8|0x80))||(o->misc&0x10)){WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=((uint64_t)o->size+3)&~3ull;d.usage=WGPUBufferUsage_CopySrc|WGPUBufferUsage_CopyDst;if(o->bind&1)d.usage|=WGPUBufferUsage_Vertex;if(o->bind&2)d.usage|=WGPUBufferUsage_Index;if(o->bind&(8|0x80))d.usage|=WGPUBufferUsage_Storage;if(o->misc&0x10)d.usage|=WGPUBufferUsage_Indirect;o->buffer=wgpuDeviceCreateBuffer(g->device,&d);g->buffer_bytes+=d.size;}
 return !atomic_load(&g->failed);
}
static bool create_texture(NativeGpu *g,const uint8_t *p){Object *o=create(g,gpu_word(p,0),3);if(!o)return false;uint32_t dim=gpu_word(p,1),dxgi=gpu_word(p,7);o->a8=dxgi==65;o->bind=gpu_word(p,8);o->td=(WGPUTextureDescriptor)WGPU_TEXTURE_DESCRIPTOR_INIT;
 o->td.dimension=dim==3?WGPUTextureDimension_3D:dim==1?WGPUTextureDimension_1D:WGPUTextureDimension_2D;o->td.size=(WGPUExtent3D){gpu_word(p,2),dim==1?1:gpu_word(p,3),gpu_word(p,dim==3?4:6)};o->td.mipLevelCount=gpu_word(p,5);o->td.sampleCount=gpu_word(p,9);o->td.format=format(dxgi,o->bind);
 if(!check(g,dim>=1&&dim<=3&&o->td.format!=WGPUTextureFormat_Undefined,"Unsupported texture dimension/DXGI format"))return false;
 if((dxgi==31||dxgi==51||dxgi==63)&&(o->bind&0x20)&&!check(g,g->format_caps_enabled,"SNORM render target requires native adapter format capabilities"))return false;
 o->td.usage=WGPUTextureUsage_CopySrc|WGPUTextureUsage_CopyDst;if(o->bind&8)o->td.usage|=WGPUTextureUsage_TextureBinding;if(o->bind&(0x20|0x40))o->td.usage|=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_TextureBinding;if(o->bind&0x80)o->td.usage|=WGPUTextureUsage_StorageBinding;if(!(o->bind&(8|0x20|0x40|0x80)))o->td.usage|=WGPUTextureUsage_TextureBinding;if(o->td.sampleCount>1)o->td.usage&=~(WGPUTextureUsage_CopySrc|WGPUTextureUsage_CopyDst|WGPUTextureUsage_StorageBinding);
 switch(o->td.format){
 #define PAIR(a,b) case WGPUTextureFormat_##a:o->views[0]=WGPUTextureFormat_##b;break;case WGPUTextureFormat_##b:o->views[0]=WGPUTextureFormat_##a;break;
 PAIR(RGBA8Unorm,RGBA8UnormSrgb) PAIR(BGRA8Unorm,BGRA8UnormSrgb) PAIR(BC1RGBAUnorm,BC1RGBAUnormSrgb) PAIR(BC2RGBAUnorm,BC2RGBAUnormSrgb) PAIR(BC3RGBAUnorm,BC3RGBAUnormSrgb) PAIR(BC7RGBAUnorm,BC7RGBAUnormSrgb)
 #undef PAIR
 default:break;
 }if(o->views[0]!=WGPUTextureFormat_Undefined){o->td.viewFormatCount=1;o->td.viewFormats=o->views;}
 o->texture=wgpuDeviceCreateTexture(g->device,&o->td);return check(g,o->texture!=NULL,"GPU texture allocation")&&!atomic_load(&g->failed);
}
static bool clear(NativeGpu *g,const uint8_t *p,bool depth){Object *o=object(g,gpu_word(p,0));if(!check(g,o&&o->texture,"Clear requires a texture"))return false;WGPUTextureView v=view(o,gpu_word(p,1),gpu_word(p,2));if(!v)return false;WGPUCommandEncoder e=encoder(g);WGPURenderPassDescriptor pd=WGPU_RENDER_PASS_DESCRIPTOR_INIT;
 WGPURenderPassColorAttachment c=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;WGPURenderPassDepthStencilAttachment d=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
 if(!depth){c.view=v;c.loadOp=WGPULoadOp_Clear;c.storeOp=WGPUStoreOp_Store;c.clearValue=(WGPUColor){gpu_float(p,3),gpu_float(p,4),gpu_float(p,5),gpu_float(p,6)};pd.colorAttachmentCount=1;pd.colorAttachments=&c;}
 else {uint32_t flags=gpu_word(p,3);d.view=v;d.depthLoadOp=(flags&1)?WGPULoadOp_Clear:WGPULoadOp_Load;d.depthStoreOp=WGPUStoreOp_Store;d.depthClearValue=gpu_float(p,4);if(o->td.format==WGPUTextureFormat_Depth24PlusStencil8||o->td.format==WGPUTextureFormat_Depth32FloatStencil8){d.stencilLoadOp=(flags&2)?WGPULoadOp_Clear:WGPULoadOp_Load;d.stencilStoreOp=WGPUStoreOp_Store;d.stencilClearValue=gpu_word(p,5);}pd.depthStencilAttachment=&d;}
 WGPURenderPassEncoder pass=wgpuCommandEncoderBeginRenderPass(e,&pd);if(pass){wgpuRenderPassEncoderEnd(pass);wgpuRenderPassEncoderRelease(pass);}wgpuTextureViewRelease(v);return submit(g,e);
}
static bool readback(NativeGpu *g,const uint8_t *p){Object *o=object(g,gpu_word(p,0));uint64_t dst=gpu_word(p,9)|((uint64_t)gpu_word(p,10)<<32),flag=gpu_word(p,15)|((uint64_t)gpu_word(p,16)<<32);uint32_t bytes=gpu_word(p,14),sub=gpu_word(p,1),box=gpu_word(p,2),row=gpu_word(p,11);bool is_buffer=gpu_word(p,13)!=0;
 if(!check(g,o&&(!flag||(flag%4==0&&host_range(g->host,flag,4))),"Invalid readback resource or flag"))return false;
 uint32_t width=0,height=1,pitch=0;uint64_t total=0,offset=box?gpu_word(p,3):0;
 if(is_buffer){if(!check(g,o->buffer&&offset<=o->size,"Invalid buffer readback"))return false;bytes=box?gpu_word(p,6)-gpu_word(p,3):(bytes<o->size?bytes:o->size);if(!check(g,bytes<=o->size-offset,"Buffer readback outside source"))return false;total=((uint64_t)bytes+3)&~3ull;}
 else {if(!check(g,o->texture&&o->td.sampleCount==1&&(o->td.format==WGPUTextureFormat_RGBA8Unorm||o->td.format==WGPUTextureFormat_BGRA8Unorm),"Texture readback format not implemented"))return false;uint32_t mip=sub%o->td.mipLevelCount;width=box?gpu_word(p,6)-gpu_word(p,3):o->td.size.width>>mip;height=box?gpu_word(p,7)-gpu_word(p,4):o->td.size.height>>mip;if(!width)width=1;if(!height)height=1;if(!check(g,width<=UINT32_MAX/4&&height<=16384,"Readback dimensions overflow"))return false;pitch=(width*4+255)&~255u;total=(uint64_t)pitch*height;if(!row)row=width*4;if(!check(g,row>=width*4,"Readback row pitch too small"))return false;bytes=width*4;}
 uint64_t destination=is_buffer?bytes:(uint64_t)(height-1)*row+bytes;if(!check(g,total&&total<=64*1024*1024&&host_range(g->host,dst,destination),"Readback allocation/destination outside limits"))return false;
 WGPUBufferDescriptor bd=WGPU_BUFFER_DESCRIPTOR_INIT;bd.size=total;bd.usage=WGPUBufferUsage_MapRead|WGPUBufferUsage_CopyDst;WGPUBuffer b=wgpuDeviceCreateBuffer(g->device,&bd);if(!b)return false;WGPUCommandEncoder e=encoder(g);
 if(is_buffer)wgpuCommandEncoderCopyBufferToBuffer(e,o->buffer,offset,b,0,total);
 else {WGPUTexelCopyTextureInfo s=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;s.texture=o->texture;s.mipLevel=sub%o->td.mipLevelCount;s.origin=(WGPUOrigin3D){box?gpu_word(p,3):0,box?gpu_word(p,4):0,sub/o->td.mipLevelCount};WGPUTexelCopyBufferInfo d=WGPU_TEXEL_COPY_BUFFER_INFO_INIT;d.buffer=b;d.layout.bytesPerRow=pitch;d.layout.rowsPerImage=height;WGPUExtent3D extent={width,height,1};wgpuCommandEncoderCopyTextureToBuffer(e,&s,&d,&extent);}
 bool ok=submit(g,e);if(ok){atomic_store(&g->callback_done,false);atomic_store(&g->map_active,true);WGPUBufferMapCallbackInfo cb=WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;cb.mode=WGPUCallbackMode_AllowProcessEvents;cb.callback=mapped;cb.userdata1=g;wgpuBufferMapAsync(b,WGPUMapMode_Read,0,total,cb);ok=wait_callback(g);}
 if(ok){const uint8_t *data=wgpuBufferGetConstMappedRange(b,0,total);ok=check(g,data!=NULL,"Readback mapped range unavailable");if(ok){if(is_buffer)host_store(g->host,dst,data,bytes);else for(unsigned y=0;y<height;y++)host_store(g->host,dst+(uint64_t)y*row,data+(uint64_t)y*pitch,bytes);}}
 if(atomic_exchange(&g->map_active,false))wgpuBufferUnmap(b);wgpuBufferDestroy(b);wgpuBufferRelease(b);
 if(ok&&flag){atomic_store_explicit(heap_word(g,flag),0,memory_order_release);wake(g,flag);}if(ok)g->readbacks++;return ok;
}
static bool state(NativeGpu *g,uint32_t op,const uint8_t *p,uint32_t n){
 uint32_t a=gpu_word(p,0),b=n>1?gpu_word(p,1):0,c=n>2?gpu_word(p,2):0;
 if(op==18){if(!check(g,n==2&&a<3,"Invalid shader state"))return false;g->shader[a]=b;}
 else if(op==19){if(!check(g,n==1,"Invalid layout state"))return false;g->layout=a;}
 else if(op==20){if(!check(g,n==1,"Invalid topology state"))return false;g->topology=a;}
 else if(op==21){if(!check(g,n==2+b*3&&a<=16&&b<=16-a,"Invalid vertex buffer range"))return false;for(unsigned i=0;i<b;i++)for(unsigned k=0;k<3;k++)g->vb[a+i][k]=gpu_word(p,2+3*i+k);}
 else if(op==22){if(!check(g,n==3,"Invalid index buffer state"))return false;for(unsigned k=0;k<3;k++)g->ib[k]=gpu_word(p,k);}
 else if(op==23){if(!check(g,a<3&&b<=16&&c<=16-b&&n==3+3*c,"Invalid constant buffer range"))return false;for(unsigned i=0;i<c;i++)for(unsigned k=0;k<3;k++)g->cb[a][b+i][k]=gpu_word(p,3+3*i+k);}
 else if(op==24||op==25||op==38){unsigned limit=op==24?128:op==25?16:8;if(!check(g,a<3&&b<=limit&&c<=limit-b&&n==(op==38?3+2*c:3+c),"Invalid shader binding range"))return false;for(unsigned i=0;i<c;i++){uint32_t id=gpu_word(p,3+i);if(op==24)g->srv[a][b+i]=id;else if(op==25)g->smp[a][b+i]=id;else {if(!check(g,gpu_word(p,3+c+i)==UINT32_MAX,"UAV counter reset not implemented"))return false;g->uav[a][b+i]=id;}}}
 else if(op==26){if(!check(g,n==37,"Invalid render target state"))return false;for(unsigned i=0;i<37;i++)g->targets[i]=gpu_word(p,i);}
 else if(op==27){if(!check(g,n==6,"Invalid blend state"))return false;for(unsigned i=0;i<6;i++)g->blend[i]=gpu_word(p,i);}
 else if(op==28){if(!check(g,n==2,"Invalid depth state"))return false;g->depth[0]=a;g->depth[1]=b;}
 else if(op==29){if(!check(g,n==1,"Invalid raster state"))return false;g->raster=a;}
 else if(op==30||op==31){unsigned step=op==30?6:4;if(!check(g,a<=16&&n==1+a*step,"Invalid viewport/scissor range"))return false;uint32_t *out=op==30?g->viewports:g->scissors;for(unsigned i=0;i<n;i++)out[i]=gpu_word(p,i);}
 else return false;return true;
}
static bool present(NativeGpu *g,uint32_t id){
 Object *o=object(g,id);if(!check(g,o&&o->texture,"Present requires an engine texture"))return false;g->presents++;
 if(!g->surface)return completed(g); /* Headless CI has no display surface. */
 if(!g->blit){
  const char *code="@group(0) @binding(0) var src: texture_2d<f32>; @vertex fn vs(@builtin(vertex_index) i:u32)->@builtin(position) vec4f {var p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));return vec4f(p[i],0,1);} @fragment fn fs(@builtin(position) p:vec4f)->@location(0) vec4f{return textureLoad(src,vec2i(p.xy),0);}";
  WGPUShaderSourceWGSL wgsl=WGPU_SHADER_SOURCE_WGSL_INIT;wgsl.code=(WGPUStringView){code,WGPU_STRLEN};WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&wgsl.chain;g->blit_shader=wgpuDeviceCreateShaderModule(g->device,&md);
  WGPUColorTargetState color=WGPU_COLOR_TARGET_STATE_INIT;color.format=WGPUTextureFormat_BGRA8Unorm;
  WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;fragment.module=g->blit_shader;fragment.entryPoint=(WGPUStringView){"fs",WGPU_STRLEN};fragment.targetCount=1;fragment.targets=&color;
  WGPURenderPipelineDescriptor d=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;d.vertex.module=g->blit_shader;d.vertex.entryPoint=(WGPUStringView){"vs",WGPU_STRLEN};d.fragment=&fragment;g->blit=wgpuDeviceCreateRenderPipeline(g->device,&d);if(!check(g,g->blit!=NULL,"Native presentation pipeline"))return false;
 }
 WGPUSurfaceTexture drawable=WGPU_SURFACE_TEXTURE_INIT;atomic_store(&g->live_phase,5);wgpuSurfaceGetCurrentTexture(g->surface,&drawable);atomic_store(&g->live_phase,1);
 if(!drawable.texture&&drawable.status==WGPUSurfaceGetCurrentTextureStatus_Timeout){atomic_fetch_add(&g->drawable_timeouts,1);return !atomic_load(&g->failed);}
 if(!check(g,drawable.texture&&(drawable.status==WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal||drawable.status==WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal),"Native drawable unavailable"))return false;
 WGPUTextureView out=wgpuTextureCreateView(drawable.texture,NULL),in=view(o,0,0);WGPUBindGroupLayout layout=wgpuRenderPipelineGetBindGroupLayout(g->blit,0);
 WGPUBindGroupEntry entry=WGPU_BIND_GROUP_ENTRY_INIT;entry.binding=0;entry.textureView=in;WGPUBindGroupDescriptor bg=WGPU_BIND_GROUP_DESCRIPTOR_INIT;bg.layout=layout;bg.entryCount=1;bg.entries=&entry;WGPUBindGroup group=wgpuDeviceCreateBindGroup(g->device,&bg);
 WGPUCommandEncoder e=encoder(g);WGPURenderPassColorAttachment c=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;c.view=out;c.loadOp=WGPULoadOp_Clear;c.storeOp=WGPUStoreOp_Store;WGPURenderPassDescriptor pass=WGPU_RENDER_PASS_DESCRIPTOR_INIT;pass.colorAttachmentCount=1;pass.colorAttachments=&c;
 WGPURenderPassEncoder r=wgpuCommandEncoderBeginRenderPass(e,&pass);wgpuRenderPassEncoderSetPipeline(r,g->blit);wgpuRenderPassEncoderSetBindGroup(r,0,group,0,NULL);wgpuRenderPassEncoderDraw(r,3,1,0,0);wgpuRenderPassEncoderEnd(r);wgpuRenderPassEncoderRelease(r);
 bool ok=submit(g,e);if(ok)ok=check(g,wgpuSurfacePresent(g->surface)==WGPUStatus_Success,"Native surface presentation failed");
 wgpuBindGroupRelease(group);wgpuBindGroupLayoutRelease(layout);wgpuTextureViewRelease(in);wgpuTextureViewRelease(out);wgpuTextureRelease(drawable.texture);if(ok)g->surface_frames++;return ok;
}
static uint32_t mip_extent(uint32_t n,uint32_t mip){return mip<32&&n>>mip?n>>mip:1;}
static bool texture_sub(NativeGpu *g,Object *o,uint32_t sub,WGPUTexelCopyTextureInfo *t,WGPUExtent3D *extent){
 if(!check(g,o&&o->texture&&o->td.mipLevelCount,"Texture copy has no resource"))return false;
 uint32_t mip=sub%o->td.mipLevelCount,layer=sub/o->td.mipLevelCount;
 bool three=o->td.dimension==WGPUTextureDimension_3D;
 if(!check(g,three?layer==0:layer<o->td.size.depthOrArrayLayers,"Texture subresource outside array"))return false;
 *t=(WGPUTexelCopyTextureInfo)WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;t->texture=o->texture;t->mipLevel=mip;t->origin.z=three?0:layer;
 *extent=(WGPUExtent3D){mip_extent(o->td.size.width,mip),mip_extent(o->td.size.height,mip),three?mip_extent(o->td.size.depthOrArrayLayers,mip):1};return true;
}
static bool copy_region(NativeGpu *g,const uint8_t *p){
 Object *d=object(g,gpu_word(p,0)),*s=object(g,gpu_word(p,5));bool box=gpu_word(p,7)!=0;
 if(!check(g,d&&s&&d->kind==s->kind,"Copy resources missing or incompatible"))return false;
 if(s->kind==11){uint64_t from=box?gpu_word(p,8):0,to=gpu_word(p,2),bytes=box?(uint64_t)gpu_word(p,11)-from:(s->size<d->size?s->size:d->size);
  if(!check(g,from<=s->size&&to<=d->size&&bytes<=s->size-from&&bytes<=d->size-to,"Buffer copy outside resource"))return false;
  if(s->shadow&&d->shadow)memmove(d->shadow+to,s->shadow+from,bytes);
  if(s->buffer&&d->buffer){if(!check(g,from%4==0&&to%4==0&&bytes%4==0&&s!=d,"Unaligned or self buffer GPU copy"))return false;if(bytes){WGPUCommandEncoder e=encoder(g);wgpuCommandEncoderCopyBufferToBuffer(e,s->buffer,from,d->buffer,to,bytes);return submit(g,e);}}
  else if(!check(g,s->shadow&&d->shadow,"Copy between GPU and constant-buffer storage not yet implemented"))return false;
  return true;
 }
 WGPUTexelCopyTextureInfo src,dst;WGPUExtent3D se,de;
 if(!texture_sub(g,s,gpu_word(p,6),&src,&se)||!texture_sub(g,d,gpu_word(p,1),&dst,&de))return false;
 uint32_t sx=box?gpu_word(p,8):0,sy=box?gpu_word(p,9):0,sz=box?gpu_word(p,10):0,dx=gpu_word(p,2),dy=gpu_word(p,3),dz=gpu_word(p,4);
 uint32_t w=box?gpu_word(p,11)-sx:se.width,h=box?gpu_word(p,12)-sy:se.height,depth=box?gpu_word(p,13)-sz:se.depthOrArrayLayers;
 if(!check(g,sx<=se.width&&sy<=se.height&&sz<=se.depthOrArrayLayers&&w<=se.width-sx&&h<=se.height-sy&&depth<=se.depthOrArrayLayers-sz&&dx<=de.width&&dy<=de.height&&dz<=de.depthOrArrayLayers&&w<=de.width-dx&&h<=de.height-dy&&depth<=de.depthOrArrayLayers-dz,"Texture copy box outside subresource"))return false;
 if(!w||!h||!depth)return true;
 if(!check(g,s->td.format==d->td.format&&s->td.sampleCount==1&&d->td.sampleCount==1,"Cross-format or multisample texture copy not yet implemented"))return false;
 src.origin.x=sx;src.origin.y=sy;if(s->td.dimension==WGPUTextureDimension_3D)src.origin.z=sz;
 dst.origin.x=dx;dst.origin.y=dy;if(d->td.dimension==WGPUTextureDimension_3D)dst.origin.z=dz;
 WGPUExtent3D extent={w,h,depth};WGPUCommandEncoder e=encoder(g);wgpuCommandEncoderCopyTextureToTexture(e,&src,&dst,&extent);return submit(g,e);
}
static bool copy_resource(NativeGpu *g,const uint8_t *p){Object *s=object(g,gpu_word(p,1)),*d=object(g,gpu_word(p,0));
 if(!check(g,s&&d&&s->kind==d->kind,"Whole resource copy has incompatible objects"))return false;
 if(s->kind==11){uint32_t cmd[14]={d->id,0,0,0,0,s->id};return copy_region(g,(uint8_t*)cmd);}
 if(!check(g,s->texture&&d->texture&&s->td.mipLevelCount==d->td.mipLevelCount&&s->td.size.width==d->td.size.width&&s->td.size.height==d->td.size.height&&s->td.size.depthOrArrayLayers==d->td.size.depthOrArrayLayers,"Whole texture copy descriptors differ"))return false;
 uint32_t layers=s->td.dimension==WGPUTextureDimension_3D?1:s->td.size.depthOrArrayLayers;
 for(unsigned layer=0;layer<layers;layer++)for(unsigned mip=0;mip<s->td.mipLevelCount;mip++){uint32_t sub=layer*s->td.mipLevelCount+mip,cmd[14]={d->id,sub,0,0,0,s->id,sub};if(!copy_region(g,(uint8_t*)cmd))return false;}return true;
}
static bool resolve_resource(NativeGpu *g,const uint8_t *p){Object *d=object(g,gpu_word(p,0)),*s=object(g,gpu_word(p,2));WGPUTexelCopyTextureInfo src,dst;WGPUExtent3D se,de;
 if(!texture_sub(g,s,gpu_word(p,3),&src,&se)||!texture_sub(g,d,gpu_word(p,1),&dst,&de))return false;
 if(!check(g,s->td.sampleCount>1&&d->td.sampleCount==1&&s->td.format==d->td.format&&se.width==de.width&&se.height==de.height,"Invalid color resolve descriptors"))return false;
 WGPUTextureView sv=view(s,src.mipLevel,src.origin.z),dv=view(d,dst.mipLevel,dst.origin.z);if(!sv||!dv){if(sv)wgpuTextureViewRelease(sv);if(dv)wgpuTextureViewRelease(dv);return check(g,false,"Resolve views unavailable");}
 WGPURenderPassColorAttachment color=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;color.view=sv;color.resolveTarget=dv;color.loadOp=WGPULoadOp_Load;color.storeOp=WGPUStoreOp_Discard;
 WGPURenderPassDescriptor pass=WGPU_RENDER_PASS_DESCRIPTOR_INIT;pass.colorAttachmentCount=1;pass.colorAttachments=&color;WGPUCommandEncoder e=encoder(g);WGPURenderPassEncoder r=wgpuCommandEncoderBeginRenderPass(e,&pass);if(r){wgpuRenderPassEncoderEnd(r);wgpuRenderPassEncoderRelease(r);}wgpuTextureViewRelease(sv);wgpuTextureViewRelease(dv);return submit(g,e);
}
static bool upload_texture(NativeGpu *g,const uint8_t *p){Object *o=object(g,gpu_word(p,0));if(!check(g,o&&o->texture,"Texture upload has no object"))return false;
 WGPUTexelCopyTextureInfo t=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;t.texture=o->texture;t.mipLevel=gpu_word(p,1);t.origin=(WGPUOrigin3D){gpu_word(p,3),gpu_word(p,4),gpu_word(p,o->td.dimension==WGPUTextureDimension_3D?5:2)};
 WGPUTexelCopyBufferLayout l=WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;l.bytesPerRow=gpu_word(p,9);l.rowsPerImage=gpu_word(p,10);WGPUExtent3D extent={gpu_word(p,6),gpu_word(p,7),gpu_word(p,8)};uint64_t bytes=gpu_word(p,11);
 if(!o->a8)wgpuQueueWriteTexture(g->queue,&t,p+48,bytes,&l,&extent);
 else {uint32_t width=extent.width,height=extent.height,depth=extent.depthOrArrayLayers,rows=l.rowsPerImage?l.rowsPerImage:height,pitch=l.bytesPerRow?l.bytesPerRow:width;
  if(!check(g,width&&height&&depth&&width<=16384&&pitch>=width&&rows>=height&&(uint64_t)(depth-1)*rows*pitch+(uint64_t)(height-1)*pitch+width<=bytes,"A8 upload source bounds invalid"))return false;
  uint32_t chunkRows=(256*1024)/(width*4);if(!chunkRows)chunkRows=1;if(chunkRows>height)chunkRows=height;uint8_t *rgba=malloc((size_t)chunkRows*width*4);if(!check(g,rgba!=NULL,"A8 bounded conversion allocation"))return false;
  for(unsigned z=0;z<depth;z++)for(unsigned y=0;y<height;y+=chunkRows){unsigned count=height-y<chunkRows?height-y:chunkRows;memset(rgba,0,(size_t)count*width*4);for(unsigned r=0;r<count;r++)for(unsigned x=0;x<width;x++)rgba[((size_t)r*width+x)*4+3]=p[48+((uint64_t)z*rows+y+r)*pitch+x];
   WGPUTexelCopyTextureInfo dst=t;dst.origin.y+=y;dst.origin.z+=z;WGPUTexelCopyBufferLayout packed=WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;packed.bytesPerRow=width*4;packed.rowsPerImage=count;WGPUExtent3D part={width,count,1};wgpuQueueWriteTexture(g->queue,&dst,rgba,(size_t)count*width*4,&packed,&part);
  }free(rgba);
 }g->uploads++;return !atomic_load(&g->failed);
}
static bool end_query(NativeGpu *g,const uint8_t *p){Object *q=object(g,gpu_word(p,0));if(!check(g,q&&q->kind==44&&q->words==4,"Query object missing"))return false;
 uint32_t type=q->meta[1];if(!check(g,type==0||type==2||type==3,"Occlusion/statistics query execution not yet implemented"))return false;
 uint64_t address=q->meta[2]|((uint64_t)q->meta[3]<<32);if(!check(g,address%4==0&&host_range(g->host,address,16),"Query result slot outside shared memory"))return false;
 uint64_t value=type==2?now_ns():1;if(!completed(g))return false;atomic_store_explicit(heap_word(g,address+8),(uint32_t)value,memory_order_relaxed);atomic_store_explicit(heap_word(g,address+12),(uint32_t)(value>>32),memory_order_relaxed);atomic_store_explicit(heap_word(g,address),gpu_word(p,1),memory_order_release);wake(g,address);return true;
}

static void reset_state(NativeGpu *g){memset(g->shader,0,sizeof(g->shader));g->layout=0;g->topology=4;memset(g->vb,0,sizeof(g->vb));memset(g->ib,0,sizeof(g->ib));memset(g->cb,0,sizeof(g->cb));memset(g->srv,0,sizeof(g->srv));memset(g->smp,0,sizeof(g->smp));memset(g->uav,0,sizeof(g->uav));memset(g->targets,0,sizeof(g->targets));memset(g->blend,0,sizeof(g->blend));for(unsigned i=1;i<5;i++)g->blend[i]=0x3f800000;g->blend[5]=UINT32_MAX;memset(g->depth,0,sizeof(g->depth));g->raster=0;g->viewports[0]=g->scissors[0]=0;}
#include "snorm_fixture.inc"
#include "native_draw.inc"
static bool execute(void *context,uint32_t op,const uint8_t *p,uint32_t n){NativeGpu *g=context;g->last_op=op;atomic_store(&g->live_phase,1);atomic_store(&g->live_commands,g->ring.executed);atomic_store(&g->live_frames,g->surface_frames);atomic_store(&g->live_draws,g->draw.draws);if(atomic_load(&g->failed)||atomic_load(&g->stop))return false;
 /* Submit pending draws before resource mutations, copies, fences or present.
    State commands can remain in the current pass. Queue writes never overtake
    an earlier draw that reads the same resource. */
 bool shadow_upload=false;if((op==12||op==47)&&n>=3){Object *o=object(g,gpu_word(p,0));shadow_upload=o&&o->shadow&&!o->buffer;}
 if(!shadow_upload&&op!=32&&!(op>=18&&op<=31)&&op!=38&&op!=0&&op!=2){if(!draw_flush(g))return false;}
 switch(op){
 case 0:case 2:return true; /* Reference protocol treats both as no-ops. */
 case 3:return check(g,n==10,"Invalid texture command")&&create_texture(g,p);
 case 4:{if(!check(g,n==1,"Invalid destroy command"))return false;Object *o=object(g,gpu_word(p,0));if(o){g->draw.resource_epoch++;CFDictionaryRemoveValue(g->objects,(const void*)(uintptr_t)o->id);destroy(o);}return !atomic_load(&g->failed);}
 case 5:return check(g,n==7,"Invalid color clear command")&&clear(g,p,false);
 case 6:return check(g,n==6,"Invalid depth clear command")&&clear(g,p,true);
 case 7:return check(g,n==1,"Invalid present command")&&present(g,gpu_word(p,0));
 case 8:{if(!check(g,n==3,"Invalid fence command"))return false;uint64_t ptr=gpu_word(p,0)|((uint64_t)gpu_word(p,1)<<32);if(!check(g,ptr%4==0&&host_range(g->host,ptr,4),"Fence address outside shared heap"))return false;if(!completed(g))return false;atomic_store_explicit(heap_word(g,ptr),gpu_word(p,2),memory_order_release);wake(g,ptr);return true;}
 case 10:{if(!check(g,n>=1&&gpu_word(p,0)<=(n-1)*4,"Invalid GPU log payload"))return false;return true;}
 case 11:return check(g,n==5,"Invalid buffer command")&&create_buffer(g,p);
 case 12:case 47:{if(!check(g,n>=3,"Invalid buffer upload"))return false;Object *o=object(g,gpu_word(p,0));uint32_t offset=gpu_word(p,1),bytes=gpu_word(p,2);if(!check(g,o&&o->kind==11&&offset<=o->size&&bytes<=o->size-offset&&((uint64_t)bytes+3)/4<=n-3,"Buffer upload outside bounds"))return false;if(o->shadow)memcpy(o->shadow+offset,p+12,bytes);if(o->buffer){if(!check(g,offset%4==0,"Unaligned GPU buffer upload"))return false;uint32_t full=bytes&~3u;if(full)wgpuQueueWriteBuffer(g->queue,o->buffer,offset,p+12,full);if(bytes&3){uint32_t tail=0;memcpy(&tail,p+12+full,bytes&3);wgpuQueueWriteBuffer(g->queue,o->buffer,offset+full,&tail,4);}}g->uploads++;return !atomic_load(&g->failed);}
 case 13:return check(g,n>=12&&((uint64_t)gpu_word(p,11)+3)/4<=n-12,"Invalid texture upload size")&&upload_texture(g,p);
 case 32:return check(g,n==6,"Invalid draw command")&&draw_execute(g,p);
 case 14:case 15:case 16:case 17:case 37:case 44:{uint32_t min=op==14?5:op==15?2:op==16?2:op==44?4:9;if(!check(g,n>=min&&n<=4096,"Invalid GPU metadata payload"))return false;if(op==14&&!check(g,gpu_word(p,1)<3&&gpu_word(p,4)<=32&&n==5+4*gpu_word(p,4),"Invalid shader input signature"))return false;if(op==15&&!check(g,gpu_word(p,1)<=32&&n==2+7*gpu_word(p,1),"Invalid vertex layout"))return false;Object *o=create(g,gpu_word(p,0),op);if(!o)return false;o->meta=malloc(n*4);if(!check(g,o->meta!=NULL,"GPU metadata allocation"))return false;memcpy(o->meta,p,n*4);o->words=n;return true;}
 case 18:case 19:case 20:case 21:case 22:case 23:case 24:case 25:case 26:case 27:case 28:case 29:case 30:case 31:case 38:if(!check(g,n>0,"Empty GPU state command"))return false;return state(g,op,p,n);
 case 33:if(!check(g,n==0,"Invalid clear-state command"))return false;reset_state(g);return true;
 case 34:return check(g,n==14,"Invalid copy-region command")&&copy_region(g,p);
 case 35:return check(g,n==2,"Invalid copy-resource command")&&copy_resource(g,p);
 case 36:return check(g,n==4,"Invalid resolve command")&&resolve_resource(g,p);
 case 45:{if(!check(g,n==1,"Invalid begin-query command"))return false;Object *q=object(g,gpu_word(p,0));return check(g,q&&q->kind==44&&q->meta[1]!=1&&q->meta[1]!=5,"Occlusion begin not yet implemented");}
 case 46:return check(g,n==2,"Invalid end-query command")&&end_query(g,p);
 case 42:return check(g,n==17,"Invalid readback command")&&readback(g,p);
 default:{char text[320];Object *vs=object(g,g->shader[0]),*ps=object(g,g->shader[1]);snprintf(text,sizeof(text),"Graphics operation not yet ported: %s; VS %u (%08x%08x) PS %u (%08x%08x), %llu commands executed",gpu_opcode_name(op),g->shader[0],vs&&vs->kind==14?vs->meta[3]:0,vs&&vs->kind==14?vs->meta[2]:0,g->shader[1],ps&&ps->kind==14?ps->meta[3]:0,ps&&ps->kind==14?ps->meta[2]:0,(unsigned long long)g->ring.executed);fail(g,text);return false;}
 }
}
static void release_hardware(NativeGpu *g){atomic_store(&g->closing,true);draw_cleanup(g);CFDictionaryApplyFunction(g->objects,destroy_entry,NULL);CFDictionaryRemoveAllValues(g->objects);if(g->blit)wgpuRenderPipelineRelease(g->blit);if(g->blit_shader)wgpuShaderModuleRelease(g->blit_shader);if(g->surface){wgpuSurfaceUnconfigure(g->surface);wgpuSurfaceRelease(g->surface);}if(g->queue)wgpuQueueRelease(g->queue);if(g->device){wgpuDeviceDestroy(g->device);wgpuDeviceRelease(g->device);}if(g->adapter)wgpuAdapterRelease(g->adapter);if(g->instance)wgpuInstanceRelease(g->instance);}
static void *loop(void *context){NativeGpu *g=context;@autoreleasepool{
 wasm_rt_init_thread();wasm_rt_trap_t trap=wasm_rt_impl_try();
 if(trap)fail(g,"GPU service shared-memory trap");
 else if(hardware(g)&&(!g->test_bc||(snorm_sample_test(g)&&bc_sample_test(g)&&draw_loader_test(g)))){atomic_store(&g->ready,true);atomic_store_explicit(heap_word(g,g->ring.base+8),1,memory_order_release);wake(g,g->ring.base+8);
  while(!atomic_load(&g->stop)&&!atomic_load(&g->failed)){
   if(gpu_ring_drain(&g->ring,256)<0){if(!atomic_load(&g->failed)&&!atomic_load(&g->stop))fail(g,g->ring.error);break;}
   if(!draw_flush(g))break;atomic_store(&g->live_commands,g->ring.executed);atomic_store(&g->live_frames,g->surface_frames);atomic_store(&g->live_draws,g->draw.draws);atomic_store(&g->live_phase,7);sample_memory(g);wgpuDevicePoll(g->device,false,NULL);uint32_t head=atomic_load(heap_word(g,g->ring.base));
   if(head==g->ring.tail){atomic_store(heap_word(g,g->ring.base+20),1);if(atomic_load(heap_word(g,g->ring.base))==head)native_wait32(&g->host->memory,g->ring.base,head,100000000);atomic_store(heap_word(g,g->ring.base+20),0);}
  }
 }
 if(!atomic_load(&g->ready)&&!atomic_load(&g->failed)&&!atomic_load(&g->stop))fail(g,"Native BC/Metal initialization fixture did not complete");atomic_store(&g->live_phase,8);release_hardware(g);wasm_rt_free_thread();}return NULL;}
NativeGpu *native_gpu_create(struct w2c_env *h,void *layer,void(*cancel)(void*),void *context){NativeGpu *g=calloc(1,sizeof(*g));if(!g)return NULL;reset_state(g);g->host=h;g->cancel=cancel;g->context=context;g->objects=CFDictionaryCreateMutable(NULL,0,NULL,NULL);if(!g->objects){free(g);return NULL;}pthread_mutex_init(&g->error_lock,NULL);if(layer)g->layer=(__bridge_retained void*)(__bridge id)layer;return g;}
void native_gpu_start(NativeGpu *g,uint64_t ring,uint64_t size,uint64_t dbg){(void)dbg;if(!g)return;if(!check(g,!g->started&&size<=UINT32_MAX,"Duplicate/invalid GPU service start"))return;if(!gpu_ring_init(&g->ring,(void*)g->host->memory.data,&g->host->memory.size,ring,(uint32_t)size,g,execute,wake)){fail(g,g->ring.error);return;}g->ring_valid=true;if(!check(g,pthread_create(&g->thread,NULL,loop,g)==0,"GPU service pthread creation"))return;g->started=true;}
void native_gpu_stop(NativeGpu *g){if(!g)return;atomic_store(&g->stop,true);if(g->started){wake(g,g->ring.base);pthread_join(g->thread,NULL);g->started=false;}}
void native_gpu_end(NativeGpu *g){if(!g)return;native_gpu_stop(g);if(g->layer){id held=CFBridgingRelease(g->layer);(void)held;}CFRelease(g->objects);pthread_mutex_destroy(&g->error_lock);free(g);}
const char *native_gpu_error(NativeGpu *g){return g&&atomic_load_explicit(&g->failed,memory_order_acquire)?g->error:NULL;}
bool native_gpu_ready(NativeGpu *g){return g&&atomic_load(&g->ready);}
void native_gpu_snapshot(NativeGpu *g,char *text,size_t size){
 if(!g){snprintf(text,size,"GPU not started");return;}
 static const char *phases[]={"initializing","command execution","local shader read","WGSL compile","pipeline compile","drawable acquisition","GPU completion wait","queue idle","GPU cleanup"};
 uint32_t phase=atomic_load(&g->live_phase);
 snprintf(text,size,"GPU %s; commands %llu; frames %llu; draws %llu; drawable timeouts %llu; last %s; shader %016llx",phase<9?phases[phase]:"unknown",(unsigned long long)atomic_load(&g->live_commands),(unsigned long long)atomic_load(&g->live_frames),(unsigned long long)atomic_load(&g->live_draws),(unsigned long long)atomic_load(&g->drawable_timeouts),gpu_opcode_name(atomic_load(&g->last_op)),(unsigned long long)atomic_load(&g->live_shader));
}
void native_gpu_summary(NativeGpu *g,char *text,size_t size){if(!g){snprintf(text,size,"Native GPU service not created");return;}snprintf(text,size,"Native GPU: %s; ready %u; native BC %u; BC sample formats %u; native format caps %u; SNORM cases %u; commands %llu; uploads %llu; readbacks %llu; surface frames %llu; draws %llu; pipelines %llu (hits %llu); last opcode %s",g->adapter_name,(unsigned)atomic_load(&g->ready),(unsigned)g->bc_enabled,g->bc_formats_tested,(unsigned)g->format_caps_enabled,g->snorm_cases_tested,(unsigned long long)g->ring.executed,(unsigned long long)g->uploads,(unsigned long long)g->readbacks,(unsigned long long)g->surface_frames,(unsigned long long)g->draw.draws,(unsigned long long)g->draw.creates,(unsigned long long)g->draw.hits,gpu_opcode_name(g->last_op));}

/* Replay uses the exact shared-memory protocol, not direct GPU test calls.
   This small fixture verifies resource uploads, clear/readback and fence
   publication independently of engine startup or game assets. */
bool native_gpu_replay_test(struct w2c_env *h,char *report,size_t capacity){
 if(atomic_load(&h->memory.size)<131072){snprintf(report,capacity,"FAIL: replay needs a separate shared-memory fixture");return false;}
 uint64_t base=atomic_load(&h->memory.size)-131072,dst=base+65536,flag=base+64000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g){snprintf(report,capacity,"FAIL: replay service allocation");return false;}
 g->test_bc=true;g->draw.fixture=true;
 host_put32(h,base,64);native_gpu_start(g,base,32768,0);uint64_t deadline=now_ns()+15000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec d={0,1000000};nanosleep(&d,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64;uint32_t cmd[128];
 #define EMIT(op,count) do{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}while(0)
 cmd[0]=11;cmd[1]=64;cmd[2]=1;cmd[3]=0;cmd[4]=0;EMIT(11,5);
 cmd[0]=11;cmd[1]=0;cmd[2]=64;for(unsigned i=0;i<16;i++)cmd[3+i]=i*19+7;EMIT(12,19);
 memset(cmd,0,sizeof(cmd));cmd[0]=11;cmd[9]=(uint32_t)dst;cmd[10]=dst>>32;cmd[13]=1;cmd[14]=64;cmd[15]=(uint32_t)flag;cmd[16]=flag>>32;host_put32(h,flag,1);EMIT(42,17);
 cmd[0]=12;cmd[1]=2;cmd[2]=4;cmd[3]=4;cmd[4]=1;cmd[5]=1;cmd[6]=1;cmd[7]=28;cmd[8]=0x28;cmd[9]=1;EMIT(3,10);
 cmd[0]=12;cmd[1]=0;cmd[2]=0;float rgba[]={.25f,.5f,.75f,1};memcpy(cmd+3,rgba,16);EMIT(5,7);
 memset(cmd,0,sizeof(cmd));cmd[0]=12;cmd[9]=(uint32_t)(dst+256);cmd[10]=(dst+256)>>32;cmd[11]=16;cmd[12]=64;cmd[14]=64;EMIT(42,17);
 cmd[0]=13;cmd[1]=2;cmd[2]=4;cmd[3]=4;cmd[4]=1;cmd[5]=1;cmd[6]=1;cmd[7]=28;cmd[8]=0x28;cmd[9]=1;EMIT(3,10);
 cmd[0]=13;cmd[1]=12;EMIT(35,2);
 memset(cmd,0,sizeof(cmd));cmd[0]=13;cmd[9]=(uint32_t)(dst+512);cmd[10]=(dst+512)>>32;cmd[11]=16;cmd[12]=64;cmd[14]=64;EMIT(42,17);
 cmd[0]=14;cmd[1]=64;cmd[2]=1;cmd[3]=0;cmd[4]=0;EMIT(11,5);
 cmd[0]=14;cmd[1]=11;EMIT(35,2);
 memset(cmd,0,sizeof(cmd));cmd[0]=14;cmd[2]=16;cmd[5]=11;cmd[7]=1;cmd[11]=16;EMIT(34,14);
 memset(cmd,0,sizeof(cmd));cmd[0]=14;cmd[9]=(uint32_t)(dst+1024);cmd[10]=(dst+1024)>>32;cmd[13]=1;cmd[14]=64;EMIT(42,17);
 cmd[0]=15;cmd[1]=2;cmd[2]=4;cmd[3]=4;cmd[4]=1;cmd[5]=1;cmd[6]=1;cmd[7]=65;cmd[8]=8;cmd[9]=1;EMIT(3,10);
 memset(cmd,0,sizeof(cmd));cmd[0]=15;cmd[6]=4;cmd[7]=4;cmd[8]=1;cmd[9]=4;cmd[10]=4;cmd[11]=16;for(unsigned i=0;i<16;i++)((uint8_t*)(cmd+12))[i]=(uint8_t)(i*17);EMIT(13,16);
 memset(cmd,0,sizeof(cmd));cmd[0]=15;cmd[9]=(uint32_t)(dst+768);cmd[10]=(dst+768)>>32;cmd[11]=16;cmd[12]=64;cmd[14]=64;EMIT(42,17);
 cmd[0]=16;cmd[1]=0;cmd[2]=(uint32_t)(flag+8);cmd[3]=(flag+8)>>32;EMIT(44,4);
 cmd[0]=16;EMIT(45,1);cmd[1]=7;EMIT(46,2);EMIT(33,0);
 cmd[0]=17;cmd[1]=2;cmd[2]=4;cmd[3]=4;cmd[4]=1;cmd[5]=1;cmd[6]=1;cmd[7]=71;cmd[8]=8;cmd[9]=1;EMIT(3,10);
 memset(cmd,0,sizeof(cmd));cmd[0]=17;cmd[6]=4;cmd[7]=4;cmd[8]=1;cmd[9]=8;cmd[10]=1;cmd[11]=8;bc_color((uint8_t*)(cmd+12));EMIT(13,14);
 cmd[0]=18;cmd[1]=2;cmd[2]=4;cmd[3]=4;cmd[4]=1;cmd[5]=1;cmd[6]=1;cmd[7]=71;cmd[8]=8;cmd[9]=1;EMIT(3,10);
 cmd[0]=18;cmd[1]=17;EMIT(35,2);cmd[0]=17;EMIT(4,1);cmd[0]=18;EMIT(4,1);
 #include "draw_replay.inc"
 cmd[0]=(uint32_t)(flag+4);cmd[1]=(flag+4)>>32;cmd[2]=99;host_put32(h,flag+4,0);EMIT(8,3);
 #undef EMIT
 atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);
 while(ok&&atomic_load(heap_word(g,flag+4))!=99&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec d={0,1000000};nanosleep(&d,NULL);}
 ok=ok&&!native_gpu_error(g)&&atomic_load(heap_word(g,flag+4))==99&&atomic_load(heap_word(g,flag))==0;
 native_gpu_stop(g);
 if(ok){uint32_t data[16];host_load(h,data,dst,64);for(unsigned i=0;i<16;i++)if(data[i]!=i*19+7)ok=false;host_load(h,data,dst+1024,64);for(unsigned i=0;i<16;i++)if(data[i]!=(i>=4&&i<8?i-4:i)*19+7)ok=false;
  uint8_t pixels[64];for(unsigned copy=0;copy<2;copy++){host_load(h,pixels,dst+256+copy*256,64);for(unsigned i=0;i<16;i++)if(abs(pixels[i*4]-64)>1||abs(pixels[i*4+1]-128)>1||abs(pixels[i*4+2]-191)>1||pixels[i*4+3]!=255)ok=false;}
  host_load(h,pixels,dst+768,64);for(unsigned i=0;i<16;i++)if(pixels[i*4]||pixels[i*4+1]||pixels[i*4+2]||pixels[i*4+3]!=i*17)ok=false;if(atomic_load(heap_word(g,flag+8))!=7||host_u64(h,flag+16)!=1)ok=false;
  for(unsigned test=0;test<10&&ok;test++){uint8_t rendered[1024];host_load(h,rendered,dst+4096+test*1024,sizeof(rendered));for(unsigned i=0;i<256&&ok;i++){bool painted=test!=8&&(test!=6||(i%16>=4&&i%16<12&&i/16>=3&&i/16<13));float scale=test==0&&i%16>=8?.5f:(test%3+1)*.25f;int expected[4]={painted?(int)lroundf(200*scale):0,painted?(int)lroundf(100*scale):0,painted?(int)lroundf(50*scale):0,painted?255:0};if(test==7){expected[0]=101;expected[1]=51;expected[2]=57;expected[3]=128;}for(unsigned k=0;k<4;k++)if(abs(rendered[i*4+k]-expected[k])>1){ok=false;snprintf(g->error,sizeof(g->error),"Draw fixture case %u pixel %u channel %u: got %u expected %d",test,i,k,rendered[i*4+k],expected[k]);atomic_store(&g->failed,true);break;}}}
  if(g->draw.draws!=11||g->draw.creates!=4||g->draw.hits<7)ok=false;
 }
 char summary[500];native_gpu_summary(g,summary,sizeof(summary));snprintf(report,capacity,"%s: native GPU shared-ring replay, buffer/texture uploads, copies/readbacks, bounded A8 expansion, event query and GPU-completion fence.\n%s: signed 8-bit R8/RG8/RGBA8 render targets: 9 cases; clear, additive draw, upload, copy, raw signed readback and sampling; all 16 pixels per case verified.\n%s: BC1–BC7 hardware sampling, 11 linear/sRGB format cases, all 16 pixels per case checked against format precision bounds; compressed BC1 ring upload/copy/destruction.\n%s: original startup IM shader pair through DRAW: 10 cases, all 256 pixels per case checked; direct/indexed/instanced/indirect draws, constant changes and offsets, blending, scissor, depth reject/pass; 11 draws, four cached pipelines.\n%s\n%s",ok?"PASS":"FAIL",g->snorm_cases_tested==9?"PASS":"FAIL",g->bc_formats_tested==11?"PASS":"FAIL",ok?"PASS":"FAIL",summary,native_gpu_error(g)?native_gpu_error(g):"Offscreen fixtures; no game scene is claimed.");native_gpu_end(g);return ok;
}
int native_gpu_replay_probe(char *report,size_t capacity){
 wasm_rt_init();struct w2c_env *h=calloc(1,sizeof(*h));if(!h){snprintf(report,capacity,"FAIL: replay host allocation");return 1;}host_init(h);
 volatile bool allocated=false;volatile int result=1;wasm_rt_trap_t trap=wasm_rt_impl_try();
 if(!trap){wasm_rt_allocate_memory_shared(&h->memory,32,65536,false,65536);allocated=true;result=native_gpu_replay_test(h,report,capacity)?0:1;}else snprintf(report,capacity,"FAIL: GPU replay memory trap %d",trap);
 if(allocated){wasm_rt_free_memory_shared(&h->memory);pthread_mutex_destroy(&h->memory.mem_lock);}free(h);wasm_rt_free_thread();return result;
}
