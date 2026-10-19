#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "native_gpu.h"
#include "native_launch.h"
#include "gpu_ring.h"
#include "drain_policy.h"
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
#include <TargetConditionals.h>
#if TARGET_OS_IOS
#include <os/proc.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "draw_types.inc"
#include "readback_layout.h"
#include "texture_copy.h"
#include "shader_diagnostic.h"
#include "resource_bytes.h"
#include "query_types.inc"
#include "native_metrics.h"
#include "../diagnostics/native_profile.h"
#include "shadow_trace.h"
#include "shadow_reference.h"
#include "completion_serial.h"
#include "core_index.h"
typedef struct {NativeGpu *owner;unsigned batch;} CompletionContext;

enum { EVENT_REPLIES=128 };
typedef struct {NativeGpu *owner;uint32_t id,sequence;uint64_t generation,token,value,started;bool busy;} EventReply;
typedef struct {
 uint32_t id,kind,size,bind,misc,stride,words,dxgi;bool a8;
 uint32_t *meta;uint8_t *shadow;uint64_t generation,source_generation;
 WGPUBuffer buffer;WGPUTexture texture;
 WGPUSampler sampler; DrawProgram *program;
 WGPUTextureDescriptor td;WGPUTextureFormat views[2];
 NativeGpu *owner;uint64_t tracked_bytes;QueryState query;
 uint64_t draw_buffer_epoch,event_token;
 uint64_t version,created_command,last_upload_command,texture_uploads,last_bound_command;uint32_t last_upload[12],last_bind[2],upload_prefix[2]; DepthProxy *proxies[4];
 WGPUBuffer counter,storage_proxy;uint64_t storage_proxy_size;
 NativeGpu *storage_owner;
 MipChain *mips[2];
} Object;
struct NativeGpu {
 NativeMetrics metrics;
 bool core_v2;CoreIndex *core_index;CorePipeline core_pipeline;uint64_t core_pipeline_epoch,core_index_hits,core_index_misses;size_t core_index_peak_bytes;
 unsigned surface_width,surface_height;CompletionSerial *completion;CompletionContext completion_contexts[COMPLETION_CAPACITY];
 uint64_t command_started,service_cpu_ns,service_cpu_errors;bool queue_writes_pending;uint64_t completion_registers,completion_joins,completion_ready,completion_write_submits,query_types[6];TimingWindow completion_latency;
 unsigned transfer_ops;uint64_t transfer_submits_avoided;
 WGPUBuffer copy_scratch;uint64_t copy_reinterpretations,copy_tiles;
 WGPUBuffer ordered_buffers[2];uint8_t *ordered_mirror;unsigned ordered_offset,ordered_slot;uint64_t ordered_uploads,ordered_bytes,ordered_capacity_flushes;
 NativeProfile *profile;bool profile_preflushed;
 EventReply events[EVENT_REPLIES];unsigned event_pending,event_peak;uint64_t event_token,event_requests,event_results,event_stale,event_pressure;
 ShadowTrace shadow_trace;
 bool reference_shadow_compat;
 WGPUTexture reference_shadow_cube;uint64_t reference_shadow_uses,reference_shadow_allocations;
 struct w2c_env *host;GpuRing ring;pthread_t thread;bool started,ring_valid;
 _Atomic bool stop,failed,ready,closing,callback_done,map_active;
 pthread_mutex_t error_lock;char error[1600],srv_guard[320];char adapter_name[192];
 void (*cancel)(void*);void *context;void *layer;
 WGPUInstance instance;WGPUAdapter adapter;WGPUDevice device;WGPUQueue queue;
 WGPUBuffer readback_buffer;uint32_t readback_capacity,readback_allocations,readback_cases_tested;uint64_t texture_readbacks;
 WGPUBuffer counter_source,zero_source;unsigned counter_slot,storage_alignment,compute_limit,uav_logs;
 struct {uint32_t opcode,words,shader[3],payload[6];} breadcrumbs[8];unsigned breadcrumb_next;
 WGPUComputePipeline upload_pipeline;WGPUBindGroup upload_group;WGPUBuffer upload_edges,upload_parameters;
 uint64_t upload_edge_dispatches,upload_edge_words,whole_copy_subresources,whole_copy_submissions,submissions;unsigned resource_cases,inter_stage_cases;
 uint64_t op_counts[48];uint32_t last_words,last_payload[14];
 WGPURenderPipeline pixel_pipeline;WGPUTexture pixel_texture;unsigned pixel_samples,pixel_min,pixel_max,pixel_varying;uint64_t pixel_hash,pixel_first_hash,pixel_last_present;
 WGPUSurface surface;WGPURenderPipeline blit;WGPUShaderModule blit_shader;
 CFMutableDictionaryRef objects;uint64_t object_generation;uint64_t presents,surface_frames,uploads,readbacks;
 uint64_t buffer_bytes,texture_bytes,resource_peak;_Atomic uint64_t live_resource_bytes;_Atomic uint32_t last_op;
 _Atomic uint32_t live_phase;
 _Atomic uint64_t live_commands,live_frames,live_draws,live_shader;
 _Atomic uint64_t drawable_timeouts;
 uint64_t last_sample,last_report;double highest_footprint;
 unsigned inter_stage_limit;
 bool counting_queries,query_pass;QueryPage query_pages[QUERY_PAGES];unsigned query_current,query_active_count,query_pending_pages,query_pages_allocated,query_cases;
 uint32_t query_active[QUERY_SCOPES];uint64_t query_token,query_begins,query_ends,query_segments,query_publications,query_empty,query_stale,query_map_requests,query_maps_completed,query_pressure_waits,query_pressure_ns,query_gpu_bytes,query_gpu_peak;
#ifdef NATIVE_PROBE_CLI
 unsigned test_inter_stage_limit;bool test_legacy_ordered_upload;bool test_legacy_layout_ids,test_legacy_shadow_counts,test_raw_pipeline_keys,test_flush_new_objects,test_uniform_wrap_wait,test_no_map_retirement,test_legacy_upload_flush,test_drain_flush,test_drain_controlled_clock;unsigned test_pipeline_capacity;bool test_sync_events;
 void *test_scope_context;
 bool test_legacy_transfers;
 bool test_no_pass_reuse;bool test_unused_layout_keys,test_no_uniform_reuse,test_measure_draw;TimingWindow test_draw_times;
#endif
 bool clip_enabled;unsigned clip_cases_tested;bool bc_enabled,format_caps_enabled,norm16_enabled,border_enabled,test_bc;unsigned norm16_cases_tested,border_cases_tested;unsigned bc_formats_tested,snorm_cases_tested,instance_cases_tested;
 DrawState draw;
 uint64_t fresh_flushes_avoided,independent_uploads,dependent_uploads,drain_submits_avoided,drain_bound_flushes;
 uint32_t shader[3],layout,topology,vb[16][3],ib[3],cb[3][16][3],srv[3][128],smp[3][16],uav[3][8];
 uint32_t targets[37],blend[6],depth[2],raster,viewports[97],scissors[65];
};
static uint64_t now_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
#include "native_metrics.inc"
static void slow_command_observe(NativeGpu *,uint32_t,const uint8_t *,uint32_t,uint64_t,uint64_t);
static void frame_cost_report(NativeGpu *);
static void completion_report(NativeGpu *);
static void sample_memory(NativeGpu *g){uint64_t now=now_ns();if(now-g->last_sample<1000000000ull)return;g->last_sample=now;
 task_vm_info_data_t info={0};mach_msg_type_number_t count=TASK_VM_INFO_COUNT;if(task_info(mach_task_self(),TASK_VM_INFO,(task_info_t)&info,&count)!=KERN_SUCCESS)return;
 double footprint=(double)info.phys_footprint/1048576.0;
#if TARGET_OS_IOS
 if(g->host->progress&&now-g->last_report>=5000000000ull){char line[190];snprintf(line,sizeof(line),"Native memory headroom: %.1f MiB available to this app right now; changes with OS pressure, not a guaranteed budget",(double)os_proc_available_memory()/1048576.0);g->host->progress(g->host->progress_context,line);}
#endif
 if(footprint>g->highest_footprint)g->highest_footprint=footprint;
 if(g->host->progress&&now-g->last_report>=5000000000ull){g->last_report=now;char line[320];snprintf(line,sizeof(line),"Native startup: process %.1f MiB (highest 1-second sample %.1f MiB); heap committed %.1f MiB; GPU commands %llu, surface frames %llu, last %s",footprint,g->highest_footprint,(double)atomic_load(&g->host->memory.size)/1048576.0,(unsigned long long)g->ring.executed,(unsigned long long)g->surface_frames,gpu_opcode_name(g->last_op));g->host->progress(g->host->progress_context,line);if(g->metrics.last_present)metrics_report(g);completion_report(g);native_profile_report(g->profile,g->host->progress,g->host->progress_context);}
}
static _Atomic uint32_t *heap_word(NativeGpu *g,uint64_t address){return (_Atomic uint32_t*)(g->host->memory.data+address);}
static void wake(void *context,uint64_t address){NativeGpu *g=context;native_notify(&g->host->memory,address,UINT32_MAX);}
static Object *object(NativeGpu *,uint32_t);
static void copy_diagnostic_locked(NativeGpu *,const uint8_t *);
static void fail(NativeGpu *g,const char *message){
 pthread_mutex_lock(&g->error_lock);bool first=!atomic_load_explicit(&g->failed,memory_order_relaxed);
 if(first){Object *vs=object(g,g->shader[0]),*ps=object(g,g->shader[1]),*cs=object(g,g->shader[2]);snprintf(g->error,sizeof(g->error),"%s %s [%s, opcode %u] VS %u %08x%08x; PS %u %08x%08x; CS %u %08x%08x; payload(%u) %u,%u,%u,%u,%u,%u",message,g->srv_guard,gpu_opcode_name(g->last_op),g->last_op,g->shader[0],vs?shader_diagnostic_word(vs->kind,vs->words,vs->meta,3):0,vs?shader_diagnostic_word(vs->kind,vs->words,vs->meta,2):0,g->shader[1],ps?shader_diagnostic_word(ps->kind,ps->words,ps->meta,3):0,ps?shader_diagnostic_word(ps->kind,ps->words,ps->meta,2):0,g->shader[2],cs?shader_diagnostic_word(cs->kind,cs->words,cs->meta,3):0,cs?shader_diagnostic_word(cs->kind,cs->words,cs->meta,2):0,g->last_words,g->last_payload[0],g->last_payload[1],g->last_payload[2],g->last_payload[3],g->last_payload[4],g->last_payload[5]);if(g->last_op==34&&g->last_words==14)copy_diagnostic_locked(g,(const uint8_t*)g->last_payload);atomic_store_explicit(&g->failed,true,memory_order_release);}
 pthread_mutex_unlock(&g->error_lock);
 if(first){if(g->host->progress){char line[400];snprintf(line,sizeof(line),"Native fatal bindings: layout %u; RT0/depth %u/%u; VB0/index %u/%u; VS/PS/CS %u/%u/%u; PS SRV0-3 %u,%u,%u,%u; PS sampler0-3 %u,%u,%u,%u; CS UAV0-3 %u,%u,%u,%u",g->layout,g->targets[0],g->targets[32],g->vb[0][0],g->ib[0],g->shader[0],g->shader[1],g->shader[2],g->srv[1][0],g->srv[1][1],g->srv[1][2],g->srv[1][3],g->smp[1][0],g->smp[1][1],g->smp[1][2],g->smp[1][3],g->uav[2][0],g->uav[2][1],g->uav[2][2],g->uav[2][3]);g->host->progress(g->host->progress_context,line);unsigned count=g->breadcrumb_next<8?g->breadcrumb_next:8;for(unsigned j=0;j<count;j++){unsigned i=(g->breadcrumb_next-count+j)%8;snprintf(line,sizeof(line),"Native GPU breadcrumb %u/%u: %s words %u; VS/PS/CS %u/%u/%u; payload %u,%u,%u,%u,%u,%u",j+1,count,gpu_opcode_name(g->breadcrumbs[i].opcode),g->breadcrumbs[i].words,g->breadcrumbs[i].shader[0],g->breadcrumbs[i].shader[1],g->breadcrumbs[i].shader[2],g->breadcrumbs[i].payload[0],g->breadcrumbs[i].payload[1],g->breadcrumbs[i].payload[2],g->breadcrumbs[i].payload[3],g->breadcrumbs[i].payload[4],g->breadcrumbs[i].payload[5]);g->host->progress(g->host->progress_context,line);}}if(g->ring_valid)gpu_ring_failed(&g->ring);if(g->cancel)g->cancel(g->context);}
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
 if(atomic_load(&g->callback_done)){g->metrics.callback_sleeps_avoided++;break;}
 struct timespec d={0,1000000};nanosleep(&d,NULL);
 }return !atomic_load(&g->failed);}
/* Successful ordered readback maps retire all earlier submitted uniform
   writes, just as a queue-completion callback does. No future/unsubmitted
   draw data is retired. The service thread is the only queue producer. */
static void uniform_retired(NativeGpu *g,bool mapped_readback){
#ifdef NATIVE_PROBE_CLI
 if(mapped_readback&&g->test_no_map_retirement)return;
#endif
 if(mapped_readback)g->draw.uniform_map_retired+=g->draw.queued_uniform_batches;
 g->draw.queued_uniform_batches=0;g->draw.queued_uniform_bytes=0;
}
static bool completed(NativeGpu *g){uint64_t profile_cpu=g->profile?native_profile_cpu():0;uint64_t start=now_ns();uint32_t previous=atomic_exchange(&g->live_phase,6);atomic_store(&g->callback_done,false);WGPUQueueWorkDoneCallbackInfo c=WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;c.mode=WGPUCallbackMode_AllowProcessEvents;c.callback=work_done;c.userdata1=g;wgpuQueueOnSubmittedWorkDone(g->queue,c);bool ok=wait_callback(g);if(ok)uniform_retired(g,false);timing_add(&g->metrics.windows[METRIC_COMPLETION],now_ns()-start);if(g->profile)native_profile_stage(g->profile,NP_COMPLETION,now_ns()-start,profile_cpu);atomic_store(&g->live_phase,previous);return ok;}
/* All NativeGpu QueueWrite calls pass here so a completed fence can never
   certify a later unsent upload. The baseline path keeps its API behavior. */
static bool queue_advance(NativeGpu *g){return !g->completion||check(g,completion_advance(g->completion),"Queue completion serial exhausted");}
static void queue_write_buffer(NativeGpu *g,WGPUBuffer b,uint64_t offset,const void *data,size_t size){
 if(!queue_advance(g))return;g->queue_writes_pending=true;wgpuQueueWriteBuffer(g->queue,b,offset,data,size);
}
static void queue_write_texture(NativeGpu *g,const WGPUTexelCopyTextureInfo *target,const void *data,size_t size,const WGPUTexelCopyBufferLayout *layout,const WGPUExtent3D *extent){
 if(!queue_advance(g))return;g->queue_writes_pending=true;wgpuQueueWriteTexture(g->queue,target,data,size,layout,extent);
}
static Object *object(NativeGpu *g,uint32_t id){
 if(!id)return NULL;
 if(g->core_index){void *value=NULL;if(core_index_get(g->core_index,id,&value)){g->core_index_hits++;return value;}g->core_index_misses++;}
 return (Object*)CFDictionaryGetValue(g->objects,(const void*)(uintptr_t)id);
}
static Object *create(NativeGpu *g,uint32_t id,uint32_t kind){if(!check(g,id&&!object(g,id),"Zero or duplicate GPU object ID"))return NULL;Object *o=calloc(1,sizeof(*o));if(!check(g,o!=NULL,"GPU object metadata allocation"))return NULL;if(!check(g,g->object_generation<UINT64_MAX,"GPU object generation exhausted")){free(o);return NULL;}o->generation=++g->object_generation;o->id=id;o->kind=kind;o->version=1;o->owner=g;o->created_command=g->ring.executed;CFDictionarySetValue(g->objects,(const void*)(uintptr_t)id,o);if(g->core_index){core_index_put(g->core_index,id,o);g->core_index_peak_bytes=sizeof(CoreIndex)+g->core_index->count*CORE_PAGE_SIZE*sizeof(void*);}return o;}
static void draw_program_free(DrawProgram *);
static void proxy_destroy(Object *);
static void mip_destroy(Object *);
static void proxy_dirty(Object *);
static void destroy(Object *o){if(!o)return;if(o->owner){o->owner->draw.resource_epoch++;if(o->owner->core_index)core_index_put(o->owner->core_index,o->id,NULL);}if(o->owner&&o->tracked_bytes){if(o->kind==11)o->owner->buffer_bytes-=o->tracked_bytes;else o->owner->texture_bytes-=o->tracked_bytes;atomic_store(&o->owner->live_resource_bytes,o->owner->buffer_bytes+o->owner->texture_bytes);}if(o->sampler)wgpuSamplerRelease(o->sampler);draw_program_free(o->program);proxy_destroy(o);mip_destroy(o);if(o->counter){wgpuBufferDestroy(o->counter);wgpuBufferRelease(o->counter);}if(o->storage_proxy){if(o->storage_owner)o->storage_owner->draw.storage_proxy_bytes-=o->storage_proxy_size;wgpuBufferDestroy(o->storage_proxy);wgpuBufferRelease(o->storage_proxy);}if(o->buffer){wgpuBufferDestroy(o->buffer);wgpuBufferRelease(o->buffer);}if(o->texture){wgpuTextureDestroy(o->texture);wgpuTextureRelease(o->texture);}free(o->shadow);free(o->meta);free(o);}
static void destroy_entry(const void *k,const void *v,void *c){(void)k;(void)c;destroy((Object*)v);}
static WGPUTextureFormat format(uint32_t dxgi,uint32_t bind){switch(dxgi){
 case 2:return WGPUTextureFormat_RGBA32Float;case 3:return WGPUTextureFormat_RGBA32Uint;case 4:return WGPUTextureFormat_RGBA32Sint;case 9:case 10:return WGPUTextureFormat_RGBA16Float;case 11:return WGPUTextureFormat_RGBA16Unorm;case 13:return WGPUTextureFormat_RGBA16Snorm;
 case 16:return WGPUTextureFormat_RG32Float;case 17:return WGPUTextureFormat_RG32Uint;case 18:return WGPUTextureFormat_RG32Sint;case 23:case 24:return WGPUTextureFormat_RGB10A2Unorm;case 26:return WGPUTextureFormat_RG11B10Ufloat;
 case 27:case 28:return WGPUTextureFormat_RGBA8Unorm;case 29:return WGPUTextureFormat_RGBA8UnormSrgb;case 30:return WGPUTextureFormat_RGBA8Uint;case 31:return WGPUTextureFormat_RGBA8Snorm;case 32:return WGPUTextureFormat_RGBA8Sint;
 case 33:case 34:return WGPUTextureFormat_RG16Float;case 35:return WGPUTextureFormat_RG16Unorm;case 37:return WGPUTextureFormat_RG16Snorm;case 36:return WGPUTextureFormat_RG16Uint;case 38:return WGPUTextureFormat_RG16Sint;case 12:return WGPUTextureFormat_RGBA16Uint;case 14:return WGPUTextureFormat_RGBA16Sint;case 39:return bind&0x40?WGPUTextureFormat_Depth32Float:WGPUTextureFormat_R32Float;
 case 40:return WGPUTextureFormat_Depth32Float;case 41:return WGPUTextureFormat_R32Float;case 42:return WGPUTextureFormat_R32Uint;case 43:return WGPUTextureFormat_R32Sint;
 case 19:case 20:case 21:case 22:return WGPUTextureFormat_Depth32FloatStencil8;
 case 44:case 45:case 46:case 47:return WGPUTextureFormat_Depth24PlusStencil8;
 case 48:case 49:return WGPUTextureFormat_RG8Unorm;case 50:return WGPUTextureFormat_RG8Uint;case 52:return WGPUTextureFormat_RG8Sint;case 51:return WGPUTextureFormat_RG8Snorm;
 case 53:return bind&0x40?WGPUTextureFormat_Depth16Unorm:WGPUTextureFormat_R16Float;case 54:return WGPUTextureFormat_R16Float;case 55:return WGPUTextureFormat_Depth16Unorm;case 56:return WGPUTextureFormat_R16Unorm;case 58:return WGPUTextureFormat_R16Snorm;case 57:return WGPUTextureFormat_R16Uint;case 59:return WGPUTextureFormat_R16Sint;
 case 65:return WGPUTextureFormat_RGBA8Unorm;
 case 60:case 61:return WGPUTextureFormat_R8Unorm;case 62:return WGPUTextureFormat_R8Uint;case 63:return WGPUTextureFormat_R8Snorm;case 64:return WGPUTextureFormat_R8Sint;case 67:return WGPUTextureFormat_RGB9E5Ufloat;
 case 70:case 71:return WGPUTextureFormat_BC1RGBAUnorm;case 72:return WGPUTextureFormat_BC1RGBAUnormSrgb;
 case 73:case 74:return WGPUTextureFormat_BC2RGBAUnorm;case 75:return WGPUTextureFormat_BC2RGBAUnormSrgb;
 case 76:case 77:return WGPUTextureFormat_BC3RGBAUnorm;case 78:return WGPUTextureFormat_BC3RGBAUnormSrgb;
 case 79:case 80:return WGPUTextureFormat_BC4RUnorm;case 81:return WGPUTextureFormat_BC4RSnorm;case 82:case 83:return WGPUTextureFormat_BC5RGUnorm;case 84:return WGPUTextureFormat_BC5RGSnorm;
 case 87:case 88:case 90:return WGPUTextureFormat_BGRA8Unorm;case 91:return WGPUTextureFormat_BGRA8UnormSrgb;
 case 94:case 95:return WGPUTextureFormat_BC6HRGBUfloat;case 96:return WGPUTextureFormat_BC6HRGBFloat;case 97:case 98:return WGPUTextureFormat_BC7RGBAUnorm;case 99:return WGPUTextureFormat_BC7RGBAUnormSrgb;
 default:return WGPUTextureFormat_Undefined;
}}
static bool hardware(NativeGpu *g){
 WGPUInstanceExtras extra={0};extra.chain.sType=(WGPUSType)WGPUSType_InstanceExtras;extra.backends=WGPUInstanceBackend_Metal;
 WGPUInstanceDescriptor instance_descriptor=WGPU_INSTANCE_DESCRIPTOR_INIT;instance_descriptor.nextInChain=&extra.chain;g->instance=wgpuCreateInstance(&instance_descriptor);if(!check(g,g->instance!=NULL,"Native GPU instance creation"))return false;
 if(g->layer){WGPUSurfaceSourceMetalLayer metal=WGPU_SURFACE_SOURCE_METAL_LAYER_INIT;metal.layer=g->layer;WGPUSurfaceDescriptor sd=WGPU_SURFACE_DESCRIPTOR_INIT;sd.nextInChain=&metal.chain;g->surface=wgpuInstanceCreateSurface(g->instance,&sd);if(!check(g,g->surface!=NULL,"Native Metal surface creation"))return false;}
 WGPURequestAdapterOptions ao=WGPU_REQUEST_ADAPTER_OPTIONS_INIT;ao.backendType=WGPUBackendType_Metal;ao.compatibleSurface=g->surface;
 WGPURequestAdapterCallbackInfo ac=WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;ac.mode=WGPUCallbackMode_AllowProcessEvents;ac.callback=got_adapter;ac.userdata1=g;atomic_store(&g->callback_done,false);wgpuInstanceRequestAdapter(g->instance,&ao,ac);if(!wait_callback(g)||!g->adapter)return false;
 WGPUAdapterInfo ai=WGPU_ADAPTER_INFO_INIT;if(!check(g,wgpuAdapterGetInfo(g->adapter,&ai)==WGPUStatus_Success,"Native adapter information"))return false;size_t n=ai.device.length==WGPU_STRLEN?(ai.device.data?strlen(ai.device.data):0):ai.device.length;if(n>190)n=190;snprintf(g->adapter_name,sizeof(g->adapter_name),"%.*s",(int)n,ai.device.data?ai.device.data:"");wgpuAdapterInfoFreeMembers(ai);
 // The browser requests texture-formats-tier1 for SNORM render targets.
 // Pinned wgpu-native 29 does not map that web feature; its adapter-specific
 // format feature exposes Metal's checked render/blend capabilities instead.
 // Keep signed 8-bit storage: UNORM substitution loses negative values and
 // float expansion would increase the game's render-target memory.
 WGPUFeatureName wanted[]={WGPUFeatureName_ClipDistances,WGPUFeatureName_TextureCompressionBC,WGPUFeatureName_TextureCompressionBCSliced3D,WGPUFeatureName_Depth32FloatStencil8,WGPUFeatureName_Float32Filterable,WGPUFeatureName_IndirectFirstInstance,WGPUFeatureName_RG11B10UfloatRenderable,WGPUFeatureName_BGRA8UnormStorage,WGPUFeatureName_DepthClipControl,(WGPUFeatureName)WGPUNativeFeature_TextureAdapterSpecificFormatFeatures,(WGPUFeatureName)WGPUNativeFeature_TextureFormat16bitNorm,(WGPUFeatureName)WGPUNativeFeature_AddressModeClampToBorder};WGPUFeatureName features[sizeof(wanted)/sizeof(wanted[0])];size_t count=0;
 for(unsigned i=0;i<sizeof(wanted)/sizeof(wanted[0]);i++)if(wgpuAdapterHasFeature(g->adapter,wanted[i]))features[count++]=wanted[i];
 WGPULimits limits=WGPU_LIMITS_INIT;if(!check(g,wgpuAdapterGetLimits(g->adapter,&limits)==WGPUStatus_Success,"Native adapter limits query"))return false;limits.maxImmediateSize=WGPU_LIMIT_U32_UNDEFINED; /* This adapter does not use the optional immediates feature. */
 #ifdef NATIVE_PROBE_CLI
 /* Reproduce the reported A18 varying cap on the CI Metal device. Only lower
    the requested test limit; production always uses the negotiated hardware. */
 if(g->test_bc||g->test_inter_stage_limit){unsigned test=g->test_inter_stage_limit?g->test_inter_stage_limit:15;if(test<limits.maxInterStageShaderVariables)limits.maxInterStageShaderVariables=test;}
#endif
 WGPUDeviceDescriptor dd=WGPU_DEVICE_DESCRIPTOR_INIT;dd.requiredLimits=&limits;dd.requiredFeatures=features;dd.requiredFeatureCount=count;dd.uncapturedErrorCallbackInfo.callback=device_error;dd.uncapturedErrorCallbackInfo.userdata1=g;dd.deviceLostCallbackInfo.mode=WGPUCallbackMode_AllowProcessEvents;dd.deviceLostCallbackInfo.callback=lost;dd.deviceLostCallbackInfo.userdata1=g;
 WGPURequestDeviceCallbackInfo dc=WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;dc.mode=WGPUCallbackMode_AllowProcessEvents;dc.callback=got_device;dc.userdata1=g;atomic_store(&g->callback_done,false);wgpuAdapterRequestDevice(g->adapter,&dd,dc);if(!wait_callback(g)||!g->device)return false;
 WGPULimits device_limits=WGPU_LIMITS_INIT;if(!check(g,wgpuDeviceGetLimits(g->device,&device_limits)==WGPUStatus_Success,"Device varying limits query failed"))return false;g->inter_stage_limit=device_limits.maxInterStageShaderVariables;
 id<MTLDevice> metal_device=(__bridge id<MTLDevice>)wgpuDeviceGetNativeMetalDevice(g->device);
 g->counting_queries=metal_device&&([metal_device supportsFamily:MTLGPUFamilyApple3]||[metal_device supportsFamily:MTLGPUFamilyMac2]);
 g->queue=wgpuDeviceGetQueue(g->device);if(!check(g,g->queue!=NULL,"Native GPU queue creation"))return false;
 g->bc_enabled=wgpuDeviceHasFeature(g->device,WGPUFeatureName_TextureCompressionBC);
 g->format_caps_enabled=wgpuDeviceHasFeature(g->device,(WGPUFeatureName)WGPUNativeFeature_TextureAdapterSpecificFormatFeatures);
 g->clip_enabled=wgpuDeviceHasFeature(g->device,WGPUFeatureName_ClipDistances);g->norm16_enabled=wgpuDeviceHasFeature(g->device,(WGPUFeatureName)WGPUNativeFeature_TextureFormat16bitNorm);g->border_enabled=wgpuDeviceHasFeature(g->device,(WGPUFeatureName)WGPUNativeFeature_AddressModeClampToBorder);
 if(g->host->progress){char report[900];snprintf(report,sizeof(report),"Native capabilities: %s; BC %u; BC3D %u; norm16 %u; SNORM rendering %u; sampler border %u; clip distances %u; float32 filter %u; depth32+stencil %u; indirect first instance %u; unclipped depth %u; BGRA storage %u; RG11 render %u; max2D %u; buffer %llu MiB; storage binding %llu MiB; uniform/storage alignment %u/%u; color attachments %u; compute threads %u, shared %u KiB. inter-stage variables %u. Capabilities are negotiated, not proof of gameplay.",g->adapter_name,g->bc_enabled,wgpuDeviceHasFeature(g->device,WGPUFeatureName_TextureCompressionBCSliced3D),g->norm16_enabled,g->format_caps_enabled,g->border_enabled,g->clip_enabled,wgpuDeviceHasFeature(g->device,WGPUFeatureName_Float32Filterable),wgpuDeviceHasFeature(g->device,WGPUFeatureName_Depth32FloatStencil8),wgpuDeviceHasFeature(g->device,WGPUFeatureName_IndirectFirstInstance),wgpuDeviceHasFeature(g->device,WGPUFeatureName_DepthClipControl),wgpuDeviceHasFeature(g->device,WGPUFeatureName_BGRA8UnormStorage),wgpuDeviceHasFeature(g->device,WGPUFeatureName_RG11B10UfloatRenderable),limits.maxTextureDimension2D,(unsigned long long)(limits.maxBufferSize/1048576),(unsigned long long)(limits.maxStorageBufferBindingSize/1048576),limits.minUniformBufferOffsetAlignment,limits.minStorageBufferOffsetAlignment,limits.maxColorAttachments,limits.maxComputeInvocationsPerWorkgroup,limits.maxComputeWorkgroupStorageSize/1024,g->inter_stage_limit);g->host->progress(g->host->progress_context,report);}
 g->storage_alignment=limits.minStorageBufferOffsetAlignment;if(g->test_bc&&g->storage_alignment<256)g->storage_alignment=256;g->compute_limit=limits.maxComputeWorkgroupsPerDimension;
 if(g->host->progress){char qinfo[160];snprintf(qinfo,sizeof(qinfo),"Native query capability: Metal counting %u; exact sample counts and Boolean predicates; bounded asynchronous results",g->counting_queries);g->host->progress(g->host->progress_context,qinfo);}
 if(g->surface){WGPUSurfaceConfiguration cfg=WGPU_SURFACE_CONFIGURATION_INIT;cfg.device=g->device;cfg.format=WGPUTextureFormat_BGRA8Unorm;cfg.usage=WGPUTextureUsage_RenderAttachment;cfg.width=g->surface_width?g->surface_width:640;cfg.height=g->surface_height?g->surface_height:360;cfg.presentMode=WGPUPresentMode_Fifo;cfg.alphaMode=WGPUCompositeAlphaMode_Opaque;wgpuSurfaceConfigure(g->surface,&cfg);
  // wgpu-hal 29.0.3 configure disables this timeout. Our diagnostic view can
  // scroll offscreen; never let nextDrawable prevent Stop/join indefinitely.
  CAMetalLayer *layer=(__bridge CAMetalLayer*)g->layer;layer.allowsNextDrawableTimeout=YES;
  if(!check(g,layer.allowsNextDrawableTimeout,"Metal drawable timeout was not enabled"))return false;
 }
 return !atomic_load(&g->failed);
}
static WGPUCommandEncoder encoder(NativeGpu *g){WGPUCommandEncoderDescriptor d=WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;return wgpuDeviceCreateCommandEncoder(g->device,&d);}
static bool submit(NativeGpu *g,WGPUCommandEncoder e){if(!e)return check(g,false,"GPU encoder creation");WGPUCommandBufferDescriptor d=WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;WGPUCommandBuffer c=wgpuCommandEncoderFinish(e,&d);wgpuCommandEncoderRelease(e);if(!c)return check(g,false,"GPU command buffer creation");uint64_t profile_cpu=g->profile?native_profile_cpu():0,profile_wall=g->profile?now_ns():0;if(!queue_advance(g)){wgpuCommandBufferRelease(c);return false;}g->queue_writes_pending=false;wgpuQueueSubmit(g->queue,1,&c);if(g->profile)native_profile_stage(g->profile,NP_SUBMIT,now_ns()-profile_wall,profile_cpu);g->submissions++;wgpuCommandBufferRelease(c);return !atomic_load(&g->failed);}
#include "bc_fixture.inc"
static WGPUTextureView view(Object *o,uint32_t mip,uint32_t slice){WGPUTextureViewDescriptor d=WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;d.dimension=WGPUTextureViewDimension_2D;d.baseMipLevel=mip;d.mipLevelCount=1;d.baseArrayLayer=slice;d.arrayLayerCount=1;return wgpuTextureCreateView(o->texture,&d);}
static bool readback_format(WGPUTextureFormat,uint32_t*,uint32_t*,uint32_t*);
static bool draw_depth(WGPUTextureFormat);
static void track_resource(NativeGpu *g,Object *o,uint64_t bytes){o->tracked_bytes=bytes;if(o->kind==11)g->buffer_bytes+=bytes;else g->texture_bytes+=bytes;uint64_t total=g->buffer_bytes+g->texture_bytes;if(total>g->resource_peak)g->resource_peak=total;atomic_store(&g->live_resource_bytes,total);}
static bool create_buffer(NativeGpu *g,const uint8_t *p){Object *o=create(g,gpu_word(p,0),11);if(!o)return false;o->size=gpu_word(p,1);o->bind=gpu_word(p,2);o->misc=gpu_word(p,3);o->stride=gpu_word(p,4);if(!check(g,o->size&&o->size<=1024*1024*1024,"GPU buffer size unsupported"))return false;
 if(o->bind&4){o->shadow=calloc(1,o->size);if(!check(g,o->shadow!=NULL,"Constant-buffer shadow allocation"))return false;}
 if((o->bind&(1|2|8|0x80))||(o->misc&0x10)){WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=((uint64_t)o->size+3)&~3ull;d.usage=WGPUBufferUsage_CopySrc|WGPUBufferUsage_CopyDst;if(o->bind&1)d.usage|=WGPUBufferUsage_Vertex;if(o->bind&2)d.usage|=WGPUBufferUsage_Index;if(o->bind&(8|0x80))d.usage|=WGPUBufferUsage_Storage;if(o->misc&0x10)d.usage|=WGPUBufferUsage_Indirect;o->buffer=wgpuDeviceCreateBuffer(g->device,&d);if(o->buffer&&!atomic_load(&g->failed))track_resource(g,o,d.size);}
 return !atomic_load(&g->failed);
}
static bool create_texture(NativeGpu *g,const uint8_t *p){Object *o=create(g,gpu_word(p,0),3);if(!o)return false;o->words=10;o->meta=malloc(40);if(!check(g,o->meta!=NULL,"Texture create descriptor history allocation"))return false;memcpy(o->meta,p,40);uint32_t dim=gpu_word(p,1),dxgi=gpu_word(p,7);o->dxgi=dxgi;o->a8=dxgi==65;o->bind=gpu_word(p,8);o->td=(WGPUTextureDescriptor)WGPU_TEXTURE_DESCRIPTOR_INIT;
 o->td.dimension=dim==3?WGPUTextureDimension_3D:dim==1?WGPUTextureDimension_1D:WGPUTextureDimension_2D;o->td.size=(WGPUExtent3D){gpu_word(p,2),dim==1?1:gpu_word(p,3),gpu_word(p,dim==3?4:6)};o->td.mipLevelCount=gpu_word(p,5);o->td.sampleCount=gpu_word(p,9);o->td.format=format(dxgi,o->bind);
 if(!check(g,dim>=1&&dim<=3&&o->td.format!=WGPUTextureFormat_Undefined,"Unsupported texture dimension/DXGI format"))return false;
 if((dxgi==31||dxgi==51||dxgi==63)&&(o->bind&0x20)&&!check(g,g->format_caps_enabled,"SNORM render target requires native adapter format capabilities"))return false;
 if((dxgi==11||dxgi==13||dxgi==35||dxgi==37||dxgi==56||dxgi==58)&&!check(g,g->norm16_enabled,"16-bit normalized texture requires negotiated native format feature"))return false;
 o->td.usage=WGPUTextureUsage_CopySrc|WGPUTextureUsage_CopyDst;if(o->bind&8)o->td.usage|=WGPUTextureUsage_TextureBinding;if(o->bind&(0x20|0x40))o->td.usage|=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_TextureBinding;if(o->bind&0x80)o->td.usage|=WGPUTextureUsage_StorageBinding;if(!(o->bind&(8|0x20|0x40|0x80)))o->td.usage|=WGPUTextureUsage_TextureBinding;if(o->td.sampleCount>1)o->td.usage&=~(WGPUTextureUsage_CopySrc|WGPUTextureUsage_CopyDst|WGPUTextureUsage_StorageBinding);
 switch(o->td.format){
 #define PAIR(a,b) case WGPUTextureFormat_##a:o->views[0]=WGPUTextureFormat_##b;break;case WGPUTextureFormat_##b:o->views[0]=WGPUTextureFormat_##a;break;
 PAIR(RGBA8Unorm,RGBA8UnormSrgb) PAIR(BGRA8Unorm,BGRA8UnormSrgb) PAIR(BC1RGBAUnorm,BC1RGBAUnormSrgb) PAIR(BC2RGBAUnorm,BC2RGBAUnormSrgb) PAIR(BC3RGBAUnorm,BC3RGBAUnormSrgb) PAIR(BC7RGBAUnorm,BC7RGBAUnormSrgb)
 #undef PAIR
 default:break;
 }if(o->views[0]!=WGPUTextureFormat_Undefined){o->td.viewFormatCount=1;o->td.viewFormats=o->views;}
 o->texture=wgpuDeviceCreateTexture(g->device,&o->td);if(o->texture&&!atomic_load(&g->failed)){uint32_t bw=1,bh=1,block=0;bool known=readback_format(o->td.format,&bw,&bh,&block);if(!known&&draw_depth(o->td.format)){block=o->td.format==WGPUTextureFormat_Depth16Unorm?2:o->td.format==WGPUTextureFormat_Depth32FloatStencil8?8:4;known=true;}uint64_t bytes=0;if(known&&resource_texture_bytes(o->td.size.width,o->td.size.height,o->td.size.depthOrArrayLayers,o->td.mipLevelCount,o->td.sampleCount,o->td.dimension==WGPUTextureDimension_3D,bw,bh,block,&bytes))track_resource(g,o,bytes);}return check(g,o->texture!=NULL,"GPU texture allocation")&&!atomic_load(&g->failed);
}
static void draw_end_pass(NativeGpu *);
static bool draw_flush(NativeGpu *);
static void draw_buffer_used(NativeGpu *,Object *);
static bool transfer_shared(NativeGpu *g){
#ifdef NATIVE_PROBE_CLI
 if(g->test_legacy_transfers)return false;
#endif
 return g->draw.encoder!=NULL;
}
static WGPUCommandEncoder transfer_begin(NativeGpu *g){if(transfer_shared(g)){draw_end_pass(g);return g->draw.encoder;}return encoder(g);}
static bool transfer_finish(NativeGpu *g,WGPUCommandEncoder e){
 if(e!=g->draw.encoder)return submit(g,e);
 /* Count retained operations, not an inferred net reduction in submissions.
    The paired replay measures actual before/after submissions separately. */
 g->transfer_submits_avoided++;if(++g->transfer_ops>=64)return draw_flush(g);return !atomic_load(&g->failed);
}
static bool clear(NativeGpu *g,const uint8_t *p,bool depth){Object *o=object(g,gpu_word(p,0));if(!check(g,o&&o->texture,"Clear requires a texture"))return false;WGPUTextureView v=view(o,gpu_word(p,1),gpu_word(p,2));if(!v)return false;WGPUCommandEncoder e=transfer_begin(g);WGPURenderPassDescriptor pd=WGPU_RENDER_PASS_DESCRIPTOR_INIT;
 WGPURenderPassColorAttachment c=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;WGPURenderPassDepthStencilAttachment d=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
 if(!depth){c.view=v;c.loadOp=WGPULoadOp_Clear;c.storeOp=WGPUStoreOp_Store;c.clearValue=(WGPUColor){gpu_float(p,3),gpu_float(p,4),gpu_float(p,5),gpu_float(p,6)};pd.colorAttachmentCount=1;pd.colorAttachments=&c;}
 else {uint32_t flags=gpu_word(p,3);d.view=v;d.depthLoadOp=(flags&1)?WGPULoadOp_Clear:WGPULoadOp_Load;d.depthStoreOp=WGPUStoreOp_Store;d.depthClearValue=gpu_float(p,4);if(o->td.format==WGPUTextureFormat_Depth24PlusStencil8||o->td.format==WGPUTextureFormat_Depth32FloatStencil8){d.stencilLoadOp=(flags&2)?WGPULoadOp_Clear:WGPULoadOp_Load;d.stencilStoreOp=WGPUStoreOp_Store;d.stencilClearValue=gpu_word(p,5);}pd.depthStencilAttachment=&d;}
 WGPURenderPassEncoder pass=wgpuCommandEncoderBeginRenderPass(e,&pd);if(pass){wgpuRenderPassEncoderEnd(pass);wgpuRenderPassEncoderRelease(pass);}wgpuTextureViewRelease(v);proxy_dirty(o);return transfer_finish(g,e);
}
#include "native_readback.inc"
static bool uav_set(NativeGpu *,const uint8_t *,uint32_t);
static bool state(NativeGpu *g,uint32_t op,const uint8_t *p,uint32_t n){
 uint32_t a=gpu_word(p,0),b=n>1?gpu_word(p,1):0,c=n>2?gpu_word(p,2):0;
 if(op==18){if(!check(g,n==2&&a<3,"Invalid shader state"))return false;g->shader[a]=b;}
 else if(op==19){if(!check(g,n==1,"Invalid layout state"))return false;g->layout=a;}
 else if(op==20){if(!check(g,n==1,"Invalid topology state"))return false;g->topology=a;}
 else if(op==21){if(!check(g,n==2+b*3&&a<=16&&b<=16-a,"Invalid vertex buffer range"))return false;for(unsigned i=0;i<b;i++)for(unsigned k=0;k<3;k++)g->vb[a+i][k]=gpu_word(p,2+3*i+k);}
 else if(op==22){if(!check(g,n==3,"Invalid index buffer state"))return false;for(unsigned k=0;k<3;k++)g->ib[k]=gpu_word(p,k);}
 else if(op==23){if(!check(g,a<3&&b<=16&&c<=16-b&&n==3+3*c,"Invalid constant buffer range"))return false;for(unsigned i=0;i<c;i++)for(unsigned k=0;k<3;k++)g->cb[a][b+i][k]=gpu_word(p,3+3*i+k);}
 else if(op==24||op==25){unsigned limit=op==24?128:16;if(!check(g,a<3&&b<=limit&&c<=limit-b&&n==3+c,"Invalid shader binding range"))return false;for(unsigned i=0;i<c;i++){uint32_t id=gpu_word(p,3+i);if(op==24){g->srv[a][b+i]=id;Object *v=object(g,id);if(v&&v->kind==17&&v->words==9){Object *t=object(g,v->meta[2]);if(t&&t->texture){t->last_bound_command=g->ring.executed;t->last_bind[0]=a;t->last_bind[1]=b+i;}}}else g->smp[a][b+i]=id;}}
 else if(op==38)return uav_set(g,p,n);
 else if(op==26){if(!check(g,n==37,"Invalid render target state"))return false;for(unsigned i=0;i<37;i++)g->targets[i]=gpu_word(p,i);}
 else if(op==27){if(!check(g,n==6,"Invalid blend state"))return false;for(unsigned i=0;i<6;i++)g->blend[i]=gpu_word(p,i);}
 else if(op==28){if(!check(g,n==2,"Invalid depth state"))return false;g->depth[0]=a;g->depth[1]=b;}
 else if(op==29){if(!check(g,n==1,"Invalid raster state"))return false;g->raster=a;}
 else if(op==30||op==31){unsigned step=op==30?6:4;if(!check(g,a<=16&&n==1+a*step,"Invalid viewport/scissor range"))return false;uint32_t *out=op==30?g->viewports:g->scissors;for(unsigned i=0;i<n;i++)out[i]=gpu_word(p,i);}
 else return false;return true;
}
#include "present_pixels.inc"
static bool present(NativeGpu *g,uint32_t id){
 Object *o=object(g,id);if(!check(g,o&&o->texture,"Present requires an engine texture"))return false;g->presents++;if(!presented_pixels(g,o))return false;
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
  if(s->buffer&&d->buffer){if(!check(g,from%4==0&&to%4==0&&bytes%4==0&&s!=d,"Unaligned or self buffer GPU copy"))return false;if(bytes){WGPUCommandEncoder e=transfer_begin(g);wgpuCommandEncoderCopyBufferToBuffer(e,s->buffer,from,d->buffer,to,bytes);if(e==g->draw.encoder){draw_buffer_used(g,s);draw_buffer_used(g,d);}return transfer_finish(g,e);}}
  else if(!check(g,s->shadow&&d->shadow,"Copy between GPU and constant-buffer storage not yet implemented"))return false;
  return true;
 }
 WGPUTexelCopyTextureInfo src,dst;WGPUExtent3D se,de;
 if(!texture_sub(g,s,gpu_word(p,6),&src,&se)||!texture_sub(g,d,gpu_word(p,1),&dst,&de))return false;
 uint32_t so[]={box?gpu_word(p,8):0,box?gpu_word(p,9):0,box?gpu_word(p,10):0},to[]={gpu_word(p,2),gpu_word(p,3),gpu_word(p,4)};
 uint32_t end[]={box?gpu_word(p,11):se.width,box?gpu_word(p,12):se.height,box?gpu_word(p,13):se.depthOrArrayLayers};
 if(!check(g,s->td.sampleCount==1&&d->td.sampleCount==1,"COPY_REGION requires single-sample resources; multisample resolve uses RESOLVE opcode 36"))return false;
 if(!check(g,tc_cast(s->dxgi,d->dxgi)&&s->td.dimension==d->td.dimension&&s->a8==d->a8,"COPY_REGION incompatible D3D format families/dimensions"))return false;
 CopyShape ss={se.width,se.height,se.depthOrArrayLayers,1,1,0},ds={de.width,de.height,de.depthOrArrayLayers,1,1,0};CopyPlan plan;
 bool formats=readback_format(s->td.format,&ss.bw,&ss.bh,&ss.bytes)&&readback_format(d->td.format,&ds.bw,&ds.bh,&ds.bytes);
 bool depth=draw_depth(s->td.format)||draw_depth(d->td.format);
 if(depth){if(!check(g,s->td.format==d->td.format&&!box&&!to[0]&&!to[1]&&!to[2]&&se.width==de.width&&se.height==de.height,"Depth copy requires identical format and complete subresource"))return false;ss.bytes=ds.bytes=4;formats=true;}
 if(!check(g,formats,"COPY_REGION unsupported physical storage layout"))return false;
 const char *error=tc_plan(ss,ds,so,to,end,&plan);if(!check(g,error==NULL,error?error:""))return false;
 if(!plan.blocks||!plan.rows||!plan.depth)return true;
 if(!check(g,s!=d||gpu_word(p,1)!=gpu_word(p,6),"COPY_REGION overlapping source/destination subresource unsupported"))return false;
 src.origin.x=so[0];src.origin.y=so[1];if(s->td.dimension==WGPUTextureDimension_3D)src.origin.z=so[2];
 dst.origin.x=to[0];dst.origin.y=to[1];if(d->td.dimension==WGPUTextureDimension_3D)dst.origin.z=to[2];
 WGPUCommandEncoder e=transfer_begin(g);if(!e)return false;
 if(s->td.format==d->td.format){WGPUExtent3D extent={plan.sw,plan.sh,plan.depth};wgpuCommandEncoderCopyTextureToTexture(e,&src,&dst,&extent);}
 else {
  if(!g->copy_scratch){WGPUBufferDescriptor b=WGPU_BUFFER_DESCRIPTOR_INIT;b.label=(WGPUStringView){"bounded bit-preserving texture copy",WGPU_STRLEN};b.size=1024*1024;b.usage=WGPUBufferUsage_CopySrc|WGPUBufferUsage_CopyDst;g->copy_scratch=wgpuDeviceCreateBuffer(g->device,&b);}
  if(!g->copy_scratch){if(e!=g->draw.encoder)wgpuCommandEncoderRelease(e);return check(g,false,"Texture copy scratch allocation failed");}
  /* Reuse one bounded GPU-only tile in encoder order. No CPU readback,
     mapping, numeric conversion or completion wait. */
  WGPUTexelCopyBufferInfo buffer=WGPU_TEXEL_COPY_BUFFER_INFO_INIT;buffer.buffer=g->copy_scratch;buffer.layout.bytesPerRow=plan.pitch;
  for(unsigned z=0;z<plan.depth;z++)for(unsigned row=0;row<plan.rows;row+=plan.tile_rows){
   unsigned rows=plan.rows-row;if(rows>plan.tile_rows)rows=plan.tile_rows;
   WGPUTexelCopyTextureInfo a=src,b=dst;a.origin.y+=row*ss.bh;b.origin.y+=row*ds.bh;a.origin.z+=z;b.origin.z+=z;buffer.layout.rowsPerImage=rows;
   WGPUExtent3D from={plan.sw,rows*ss.bh,1},into={plan.dw,rows*ds.bh,1};
   wgpuCommandEncoderCopyTextureToBuffer(e,&a,&buffer,&from);wgpuCommandEncoderCopyBufferToTexture(e,&buffer,&b,&into);g->copy_tiles++;
  }g->copy_reinterpretations++;
 }
 proxy_dirty(d);return transfer_finish(g,e);
}
/* Called under error_lock before failed is release-published. The returned
   error string is immutable for all acquire-readers until GPU teardown. */
static void copy_diagnostic_locked(NativeGpu *g,const uint8_t *p){
 char extra[1000];Object *d=object(g,gpu_word(p,0)),*s=object(g,gpu_word(p,5));
 snprintf(extra,sizeof(extra)," Copy descriptors: source %u DXGI %u native %u size %ux%ux%u mips %u samples %u gen %llu; destination %u DXGI %u native %u size %ux%ux%u mips %u samples %u gen %llu; raw[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]",s?s->id:0,s?s->dxgi:0,s?(unsigned)s->td.format:0,s?s->td.size.width:0,s?s->td.size.height:0,s?s->td.size.depthOrArrayLayers:0,s?s->td.mipLevelCount:0,s?s->td.sampleCount:0,s?(unsigned long long)s->generation:0,d?d->id:0,d?d->dxgi:0,d?(unsigned)d->td.format:0,d?d->td.size.width:0,d?d->td.size.height:0,d?d->td.size.depthOrArrayLayers:0,d?d->td.mipLevelCount:0,d?d->td.sampleCount:0,d?(unsigned long long)d->generation:0,gpu_word(p,0),gpu_word(p,1),gpu_word(p,2),gpu_word(p,3),gpu_word(p,4),gpu_word(p,5),gpu_word(p,6),gpu_word(p,7),gpu_word(p,8),gpu_word(p,9),gpu_word(p,10),gpu_word(p,11),gpu_word(p,12),gpu_word(p,13));
 size_t used=strlen(g->error);if(used<sizeof(g->error))snprintf(g->error+used,sizeof(g->error)-used,"%s",extra);
}

static bool copy_resource(NativeGpu *g,const uint8_t *p){Object *s=object(g,gpu_word(p,1)),*d=object(g,gpu_word(p,0));
 if(!check(g,s&&d&&s->kind==d->kind,"Whole resource copy has incompatible objects"))return false;
 if(s->kind==11){uint32_t cmd[14]={d->id,0,0,0,0,s->id};return copy_region(g,(uint8_t*)cmd);}
 if(!check(g,s!=d&&s->texture&&d->texture&&s->td.dimension==d->td.dimension&&s->td.format==d->td.format&&s->td.sampleCount==1&&d->td.sampleCount==1&&s->td.mipLevelCount==d->td.mipLevelCount&&s->td.size.width==d->td.size.width&&s->td.size.height==d->td.size.height&&s->td.size.depthOrArrayLayers==d->td.size.depthOrArrayLayers,"Whole texture copy descriptors differ or unsupported format/sample count"))return false;
 uint32_t layers=s->td.dimension==WGPUTextureDimension_3D?1:s->td.size.depthOrArrayLayers;
 WGPUCommandEncoder e=encoder(g);if(!e)return false;
 for(unsigned layer=0;layer<layers;layer++)for(unsigned mip=0;mip<s->td.mipLevelCount;mip++){
  uint32_t sub=layer*s->td.mipLevelCount+mip;WGPUTexelCopyTextureInfo src,dst;WGPUExtent3D se,de;
  if(!texture_sub(g,s,sub,&src,&se)||!texture_sub(g,d,sub,&dst,&de)){wgpuCommandEncoderRelease(e);return false;}
  uint32_t bw=1,bh=1,block=0;if(readback_format(s->td.format,&bw,&bh,&block)){se.width=((se.width+bw-1)/bw)*bw;se.height=((se.height+bh-1)/bh)*bh;}wgpuCommandEncoderCopyTextureToTexture(e,&src,&dst,&se);g->whole_copy_subresources++;
 }
 proxy_dirty(d);bool ok=submit(g,e);if(ok)g->whole_copy_submissions++;return ok;
}
static bool resolve_resource(NativeGpu *g,const uint8_t *p){Object *d=object(g,gpu_word(p,0)),*s=object(g,gpu_word(p,2));WGPUTexelCopyTextureInfo src,dst;WGPUExtent3D se,de;
 if(!texture_sub(g,s,gpu_word(p,3),&src,&se)||!texture_sub(g,d,gpu_word(p,1),&dst,&de))return false;
 if(!check(g,s->td.sampleCount>1&&d->td.sampleCount==1&&s->td.format==d->td.format&&se.width==de.width&&se.height==de.height,"Invalid color resolve descriptors"))return false;
 WGPUTextureView sv=view(s,src.mipLevel,src.origin.z),dv=view(d,dst.mipLevel,dst.origin.z);if(!sv||!dv){if(sv)wgpuTextureViewRelease(sv);if(dv)wgpuTextureViewRelease(dv);return check(g,false,"Resolve views unavailable");}
 WGPURenderPassColorAttachment color=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;color.view=sv;color.resolveTarget=dv;color.loadOp=WGPULoadOp_Load;color.storeOp=WGPUStoreOp_Store;
 WGPURenderPassDescriptor pass=WGPU_RENDER_PASS_DESCRIPTOR_INIT;pass.colorAttachmentCount=1;pass.colorAttachments=&color;WGPUCommandEncoder e=encoder(g);WGPURenderPassEncoder r=wgpuCommandEncoderBeginRenderPass(e,&pass);if(r){wgpuRenderPassEncoderEnd(r);wgpuRenderPassEncoderRelease(r);}wgpuTextureViewRelease(sv);wgpuTextureViewRelease(dv);proxy_dirty(d);return submit(g,e);
}
static bool upload_texture(NativeGpu *g,const uint8_t *p){Object *o=object(g,gpu_word(p,0));if(!check(g,o&&o->texture,"Texture upload has no object"))return false;
 WGPUTexelCopyTextureInfo t=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;t.texture=o->texture;t.mipLevel=gpu_word(p,1);t.origin=(WGPUOrigin3D){gpu_word(p,3),gpu_word(p,4),gpu_word(p,o->td.dimension==WGPUTextureDimension_3D?5:2)};
 WGPUTexelCopyBufferLayout l=WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;l.bytesPerRow=gpu_word(p,9);l.rowsPerImage=gpu_word(p,10);WGPUExtent3D extent={gpu_word(p,6),gpu_word(p,7),gpu_word(p,8)};uint64_t bytes=gpu_word(p,11);
 if(!o->a8)queue_write_texture(g,&t,p+48,bytes,&l,&extent);
 else {uint32_t width=extent.width,height=extent.height,depth=extent.depthOrArrayLayers,rows=l.rowsPerImage?l.rowsPerImage:height,pitch=l.bytesPerRow?l.bytesPerRow:width;
  if(!check(g,width&&height&&depth&&width<=16384&&pitch>=width&&rows>=height&&(uint64_t)(depth-1)*rows*pitch+(uint64_t)(height-1)*pitch+width<=bytes,"A8 upload source bounds invalid"))return false;
  uint32_t chunkRows=(256*1024)/(width*4);if(!chunkRows)chunkRows=1;if(chunkRows>height)chunkRows=height;uint8_t *rgba=malloc((size_t)chunkRows*width*4);if(!check(g,rgba!=NULL,"A8 bounded conversion allocation"))return false;
  for(unsigned z=0;z<depth;z++)for(unsigned y=0;y<height;y+=chunkRows){unsigned count=height-y<chunkRows?height-y:chunkRows;memset(rgba,0,(size_t)count*width*4);for(unsigned r=0;r<count;r++)for(unsigned x=0;x<width;x++)rgba[((size_t)r*width+x)*4+3]=p[48+((uint64_t)z*rows+y+r)*pitch+x];
   WGPUTexelCopyTextureInfo dst=t;dst.origin.y+=y;dst.origin.z+=z;WGPUTexelCopyBufferLayout packed=WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;packed.bytesPerRow=width*4;packed.rowsPerImage=count;WGPUExtent3D part={width,count,1};queue_write_texture(g,&dst,rgba,(size_t)count*width*4,&packed,&part);
  }free(rgba);
 }memcpy(o->last_upload,p,48);o->upload_prefix[0]=o->upload_prefix[1]=0;memcpy(o->upload_prefix,p+48,(size_t)(bytes<8?bytes:8));o->texture_uploads++;o->last_upload_command=g->ring.executed;proxy_dirty(o);g->uploads++;return !atomic_load(&g->failed);
}
static void draw_end_pass(NativeGpu *);
static bool draw_flush(NativeGpu *);
 #include "native_queries.inc"
#include "event_queries.inc"
#include "completion_service.inc"
 static bool end_query(NativeGpu *g,const uint8_t *p){Object *q=object(g,gpu_word(p,0));if(!check(g,q&&q->kind==44&&q->words==4,"Query object missing"))return false;
 uint32_t type=q->meta[1];if(type<6)g->query_types[type]++;if(type==1||type==5)return end_occlusion(g,q,gpu_word(p,1));if(!check(g,type==0||type==2||type==3,"Occlusion/statistics query execution not yet implemented"))return false;
 uint64_t address=q->meta[2]|((uint64_t)q->meta[3]<<32);if(!check(g,address%4==0&&host_range(g->host,address,16),"Query result slot outside shared memory"))return false;
 uint64_t value=type==2?now_ns():1;
#ifdef NATIVE_PROBE_CLI
 if(!g->test_sync_events)
#endif
 return g->completion?completion_event_end(g,q,gpu_word(p,1),value):event_end(g,q,gpu_word(p,1),value);
#ifdef NATIVE_PROBE_CLI
 if(!draw_flush(g)||!completed(g))return false;atomic_store_explicit(heap_word(g,address+8),(uint32_t)value,memory_order_relaxed);atomic_store_explicit(heap_word(g,address+12),(uint32_t)(value>>32),memory_order_relaxed);atomic_store_explicit(heap_word(g,address),gpu_word(p,1),memory_order_release);wake(g,address);return true;
#endif
}

static void reset_state(NativeGpu *g){memset(g->shader,0,sizeof(g->shader));g->layout=0;g->topology=4;memset(g->vb,0,sizeof(g->vb));memset(g->ib,0,sizeof(g->ib));memset(g->cb,0,sizeof(g->cb));memset(g->srv,0,sizeof(g->srv));memset(g->smp,0,sizeof(g->smp));memset(g->uav,0,sizeof(g->uav));memset(g->targets,0,sizeof(g->targets));memset(g->blend,0,sizeof(g->blend));for(unsigned i=1;i<5;i++)g->blend[i]=0x3f800000;g->blend[5]=UINT32_MAX;memset(g->depth,0,sizeof(g->depth));g->raster=0;g->viewports[0]=g->scissors[0]=0;}
#include "snorm_fixture.inc"
#include "native_draw.inc"
#include "norm16_fixture.inc"
#include "clip_fixture.inc"
#include "border_fixture.inc"
#include "native_upload.inc"
#include "ordered_upload.inc"

static bool execute_impl(void *context,uint32_t op,const uint8_t *p,uint32_t n){NativeGpu *g=context;g->last_op=op;g->op_counts[op]++;g->last_words=n;memset(g->last_payload,0,8*sizeof(*g->last_payload));for(unsigned i=0;i<n&&i<(op==34?14u:8u);i++)g->last_payload[i]=gpu_word(p,i);unsigned crumb=g->breadcrumb_next++%8;g->breadcrumbs[crumb].opcode=op;g->breadcrumbs[crumb].words=n;memcpy(g->breadcrumbs[crumb].shader,g->shader,sizeof(g->shader));memset(g->breadcrumbs[crumb].payload,0,sizeof(g->breadcrumbs[crumb].payload));for(unsigned i=0;i<n&&i<6;i++)g->breadcrumbs[crumb].payload[i]=gpu_word(p,i);atomic_store(&g->live_phase,1);atomic_store(&g->live_commands,g->ring.executed);atomic_store(&g->live_frames,g->surface_frames);atomic_store(&g->live_draws,g->draw.draws);if(atomic_load(&g->failed)||atomic_load(&g->stop))return false;
 if(g->completion&&g->completion->deferred_count&&completion_due(g->completion,g->command_started)&&!completion_progress(g))return false;
 /* Submit pending draws before resource mutations, copies, fences or present.
    State commands can remain in the current pass. Queue writes never overtake
    an earlier draw that reads the same resource. */
 /* Retire reply ownership before a destruction-triggered flush can poll maps.
    The physical segment still closes/resolves using its immutable snapshot. */
 if(op==4&&n==1){Object *q=object(g,gpu_word(p,0));if(q&&q->kind==44)query_destroy_scope(g,q);}
 if((op==12||op==47)&&ordered_upload_eligible(g,p,n))return ordered_upload(g,p);
 bool shadow_upload=false;if((op==12||op==47)&&n>=3){Object *o=object(g,gpu_word(p,0));shadow_upload=o&&o->shadow&&!o->buffer;}
 bool independent_upload=false;if((op==12||op==47)&&n>=3){Object *o=object(g,gpu_word(p,0));independent_upload=o&&o->buffer&&g->draw.batches!=UINT64_MAX&&o->draw_buffer_epoch!=g->draw.batches+1;
#ifdef NATIVE_PROBE_CLI
 if(g->test_legacy_upload_flush)independent_upload=false;
#endif
 if(g->draw.encoder&&o&&o->buffer){if(independent_upload)g->independent_uploads++;else g->dependent_uploads++;}}
 /* Preserve the prior pass usage/barrier scopes. An unrelated upload can
    avoid a queue submission without merging formerly separate render passes. */
 if(independent_upload)draw_end_pass(g);
 bool fresh_object=(op==3||op==11||op==14||op==15||op==16||op==17||op==37||op==44)&&n&&!object(g,gpu_word(p,0));
#ifdef NATIVE_PROBE_CLI
 if(g->test_flush_new_objects)fresh_object=false;
#endif
 if(fresh_object&&g->draw.encoder)g->fresh_flushes_avoided++;
 bool ordered_transfer=(op==5||op==6||op==34)&&transfer_shared(g);
 if(!ordered_transfer&&!shadow_upload&&!independent_upload&&!fresh_object&&op!=32&&!(op>=18&&op<=31)&&op!=38&&op!=39&&op!=40&&op!=41&&op!=43&&op!=45&&op!=46&&op!=0&&op!=2){uint64_t profile_cpu=g->profile?native_profile_cpu():0;bool pending=g->draw.encoder!=NULL;uint64_t started=now_ns();bool valid=draw_flush(g);uint64_t wall=now_ns()-started;g->metrics.command_flush_ns+=wall;if(pending&&g->profile){g->profile_preflushed=true;native_profile_stage(g->profile,NP_PREFLUSH,wall,profile_cpu);}if(!valid)return false;}
 switch(op){
 case 0:case 2:return true; /* Reference protocol treats both as no-ops. */
 case 3:return check(g,n==10,"Invalid texture command")&&create_texture(g,p);
 case 4:{if(!check(g,n==1,"Invalid destroy command"))return false;Object *o=object(g,gpu_word(p,0));if(o){query_destroy_scope(g,o);if(g->core_index)core_index_put(g->core_index,o->id,NULL);CFDictionaryRemoveValue(g->objects,(const void*)(uintptr_t)o->id);destroy(o);}return !atomic_load(&g->failed);}
 case 5:return check(g,n==7,"Invalid color clear command")&&clear(g,p,false);
 case 6:return check(g,n==6,"Invalid depth clear command")&&clear(g,p,true);
 case 7:return check(g,n==1,"Invalid present command")&&present(g,gpu_word(p,0));
 case 8:{if(!check(g,n==3,"Invalid fence command"))return false;uint64_t ptr=gpu_word(p,0)|((uint64_t)gpu_word(p,1)<<32);if(!check(g,ptr%4==0&&host_range(g->host,ptr,4),"Fence address outside shared heap"))return false;if(!completion_progress(g)||!completion_flush_writes(g)||!completed(g))return false;atomic_store_explicit(heap_word(g,ptr),gpu_word(p,2),memory_order_release);wake(g,ptr);return true;}
 case 10:{if(!check(g,n>=1&&gpu_word(p,0)<=(n-1)*4,"Invalid GPU log payload"))return false;return true;}
 case 11:return check(g,n==5,"Invalid buffer command")&&create_buffer(g,p);
 case 12:case 47:return upload_buffer(g,p,n);
 case 13:return check(g,n>=12&&((uint64_t)gpu_word(p,11)+3)/4<=n-12,"Invalid texture upload size")&&upload_texture(g,p);
 case 32:return check(g,n==6,"Invalid draw command")&&draw_execute(g,p);
 case 39:return check(g,n==3,"Invalid dispatch command")&&native_dispatch(g,p);
 case 40:return check(g,n==6,"Invalid UAV clear command")&&uav_clear(g,p);
 case 41:return check(g,n==1,"Invalid mip generation command")&&generate_mips(g,gpu_word(p,0));
 case 43:return check(g,n==3,"Invalid counter copy command")&&uav_copy_counter(g,p);
 case 14:case 15:case 16:case 17:case 37:case 44:{uint32_t min=op==14?5:op==15?2:op==16?2:op==44?4:9;if(!check(g,n>=min&&n<=4096,"Invalid GPU metadata payload"))return false;if(op==14&&!check(g,gpu_word(p,1)<3&&gpu_word(p,4)<=32&&n==5+4*gpu_word(p,4),"Invalid shader input signature"))return false;if(op==15&&!check(g,gpu_word(p,1)<=32&&n==2+7*gpu_word(p,1),"Invalid vertex layout"))return false;if(op==44){uint32_t type=gpu_word(p,1);uint64_t address=gpu_word(p,2)|((uint64_t)gpu_word(p,3)<<32);if(!check(g,n==4&&(type==0||type==1||type==2||type==3||type==5)&&address%4==0&&host_range(g->host,address,16),"Query type/descriptor/result address unsupported"))return false;if((type==1||type==5)&&!check(g,g->counting_queries,"Metal exact occlusion counting is unavailable on this device"))return false;}
 Object *o=create(g,gpu_word(p,0),op);if(!o)return false;o->meta=malloc(n*4);if(!check(g,o->meta!=NULL,"GPU metadata allocation"))return false;memcpy(o->meta,p,n*4);o->words=n;if(op==17||op==37){Object *source=object(g,o->meta[2]);if(!check(g,n==9&&o->meta[1]<=1&&source&&(o->meta[1]?source->buffer!=NULL:source->texture!=NULL),"SRV/UAV creation requires an existing resource and exact descriptor"))return false;o->source_generation=source->generation;}if(op==44)o->query.type=o->meta[1];return true;}
 case 18:case 19:case 20:case 21:case 22:case 23:case 24:case 25:case 26:case 27:case 28:case 29:case 30:case 31:case 38:if(!check(g,n>0,"Empty GPU state command"))return false;return state(g,op,p,n);
 case 33:if(!check(g,n==0,"Invalid clear-state command"))return false;reset_state(g);return true;
 case 34:{if(!check(g,n==14,"Invalid copy-region command"))return false;return copy_region(g,p);}
 case 35:return check(g,n==2,"Invalid copy-resource command")&&copy_resource(g,p);
 case 36:return check(g,n==4,"Invalid resolve command")&&resolve_resource(g,p);
 case 45:return check(g,n==1,"Invalid begin-query command")&&begin_query(g,p);
 case 46:return check(g,n==2,"Invalid end-query command")&&end_query(g,p);
 case 42:return check(g,n==17,"Invalid readback command")&&readback(g,p);
 default:{char text[320];Object *vs=object(g,g->shader[0]),*ps=object(g,g->shader[1]);snprintf(text,sizeof(text),"Graphics operation not yet ported: %s; VS %u (%08x%08x) PS %u (%08x%08x), %llu commands executed",gpu_opcode_name(op),g->shader[0],vs&&vs->kind==14?vs->meta[3]:0,vs&&vs->kind==14?vs->meta[2]:0,g->shader[1],ps&&ps->kind==14?ps->meta[3]:0,ps&&ps->kind==14?ps->meta[2]:0,(unsigned long long)g->ring.executed);fail(g,text);return false;}
 }
}
static bool execute(void *context,uint32_t op,const uint8_t *p,uint32_t n){NativeGpu *g=context;uint64_t profile_cpu=g->profile?native_profile_cpu():0,submits=g->submissions;g->profile_preflushed=false;uint64_t start=now_ns(),frames=g->presents;g->metrics.command_flush_ns=0;g->command_started=start;bool ok=execute_impl(context,op,p,n);uint64_t end=now_ns(),elapsed=end-start;
#ifdef NATIVE_PROBE_CLI
 if(g->test_measure_draw&&op==32)timing_add(&g->test_draw_times,elapsed);
#endif
 if(g->profile){uint64_t bytes=op==13&&n>=12?gpu_word(p,11):((op==12||op==47)&&n>=3?gpu_word(p,2):0);native_profile_operation(g->profile,op,elapsed,profile_cpu,bytes,g->submissions-submits,g->profile_preflushed);}
 g->metrics.frame_busy+=elapsed;if(op==12||op==13||op==47)g->metrics.frame_upload+=elapsed;if(op==3||op==4||op==11||op==14||op==15||op==16||op==17||op==37||op==44)g->metrics.frame_alloc+=elapsed;if(op==13&&n>=12)g->metrics.upload_bytes+=gpu_word(p,11);else if((op==12||op==47)&&n>=3)g->metrics.upload_bytes+=gpu_word(p,2);slow_command_observe(g,op,p,n,end,elapsed);if(ok&&op==7&&g->presents>frames)metrics_present(g,end);return ok;}
#include "frame_costs.inc"
static void release_hardware(NativeGpu *g){completion_report(g);if(g->host->progress){char transfer[420];snprintf(transfer,sizeof(transfer),"Ordered transfers: clear/copy operations retained %llu; copy reinterpretations/tiles %llu/%llu; GPU scratch %u KiB. No CPU readback or extra completion wait; up to64 transfers per retained batch; existing drain and staging bounds retained.",(unsigned long long)g->transfer_submits_avoided,(unsigned long long)g->copy_reinterpretations,(unsigned long long)g->copy_tiles,g->copy_scratch?1024:0);g->host->progress(g->host->progress_context,transfer);}if(g->host->progress){char line[400];snprintf(line,sizeof(line),"Buffer mutation dependencies: independent uploads %llu; required pending-draw flushes %llu; 8 metadata bytes per live object; ordered upload arenas reported separately. Ready-drain submissions avoided %llu; four-drain bound flushes %llu (4 ms checked between drains, never waits for more work).",(unsigned long long)g->independent_uploads,(unsigned long long)g->dependent_uploads,(unsigned long long)g->drain_submits_avoided,(unsigned long long)g->drain_bound_flushes);g->host->progress(g->host->progress_context,line);}metrics_report(g);frame_cost_report(g);native_profile_report(g->profile,g->host->progress,g->host->progress_context);atomic_store(&g->closing,true);draw_cleanup(g);if(g->copy_scratch){wgpuBufferDestroy(g->copy_scratch);wgpuBufferRelease(g->copy_scratch);g->copy_scratch=NULL;}query_cleanup(g);if(g->upload_group)wgpuBindGroupRelease(g->upload_group);if(g->upload_pipeline)wgpuComputePipelineRelease(g->upload_pipeline);if(g->upload_edges){wgpuBufferDestroy(g->upload_edges);wgpuBufferRelease(g->upload_edges);}if(g->upload_parameters){wgpuBufferDestroy(g->upload_parameters);wgpuBufferRelease(g->upload_parameters);}if(g->pixel_pipeline)wgpuRenderPipelineRelease(g->pixel_pipeline);if(g->pixel_texture){wgpuTextureDestroy(g->pixel_texture);wgpuTextureRelease(g->pixel_texture);}if(g->counter_source){wgpuBufferDestroy(g->counter_source);wgpuBufferRelease(g->counter_source);}if(g->zero_source){wgpuBufferDestroy(g->zero_source);wgpuBufferRelease(g->zero_source);}if(g->readback_buffer){readback_unmap(g);wgpuBufferDestroy(g->readback_buffer);wgpuBufferRelease(g->readback_buffer);g->readback_buffer=NULL;}if(g->core_index){core_index_clear(g->core_index);free(g->core_index);g->core_index=NULL;}CFDictionaryApplyFunction(g->objects,destroy_entry,NULL);CFDictionaryRemoveAllValues(g->objects);if(g->reference_shadow_cube){wgpuTextureDestroy(g->reference_shadow_cube);wgpuTextureRelease(g->reference_shadow_cube);g->reference_shadow_cube=NULL;}if(g->blit)wgpuRenderPipelineRelease(g->blit);if(g->blit_shader)wgpuShaderModuleRelease(g->blit_shader);if(g->surface){wgpuSurfaceUnconfigure(g->surface);wgpuSurfaceRelease(g->surface);}if(g->queue)wgpuQueueRelease(g->queue);if(g->device){wgpuDeviceDestroy(g->device);wgpuDeviceRelease(g->device);}if(g->adapter)wgpuAdapterRelease(g->adapter);if(g->instance)wgpuInstanceRelease(g->instance);
#ifdef NATIVE_PROBE_CLI
 free(g->test_scope_context);g->test_scope_context=NULL;
#endif
}
static void *loop(void *context){NativeGpu *g=context;@autoreleasepool{
 wasm_rt_init_thread();wasm_rt_trap_t trap=wasm_rt_impl_try();
 if(trap)fail(g,"GPU service shared-memory trap");
 else if(hardware(g)&&(!g->test_bc||(clip_sample_test(g)&&snorm_sample_test(g)&&norm16_sample_test(g)&&border_sample_test(g)&&bc_sample_test(g)&&draw_loader_test(g)))){atomic_store(&g->ready,true);atomic_store_explicit(heap_word(g,g->ring.base+8),1,memory_order_release);wake(g,g->ring.base+8);
  unsigned retained_drains=0;uint64_t retained_since=0;
  /* Driver/poll/progress temporaries must not live for the whole game session.
     Explicit WGPU handles and copied source data retain their normal owners. */
  while(!atomic_load(&g->stop)&&!atomic_load(&g->failed)){@autoreleasepool{
   uint64_t profile_cpu=g->profile?native_profile_cpu():0,profile_wall_before=g->profile?g->profile->command_wall:0,profile_cpu_before=g->profile?g->profile->command_cpu:0;uint64_t loop_cpu=profile_cpu;uint64_t drain_start=now_ns();int drained=gpu_ring_drain(&g->ring,256);if(g->profile)native_profile_residual(g->profile,now_ns()-drain_start,profile_cpu,profile_wall_before,profile_cpu_before);
   if(drained<0){if(!atomic_load(&g->failed)&&!atomic_load(&g->stop))fail(g,g->ring.error);break;}
   uint32_t ready_head=atomic_load_explicit(heap_word(g,g->ring.base),memory_order_acquire);
   if(g->draw.encoder){if(!retained_drains)retained_since=drain_start;retained_drains++;}else retained_drains=0;
   uint64_t flush_cpu=g->profile?native_profile_cpu():0;uint64_t flush_start=now_ns();bool keep=g->draw.encoder&&native_drain_keep(retained_drains,flush_start>=retained_since?flush_start-retained_since:UINT64_MAX,drained==256,ready_head!=g->ring.tail);
#ifdef NATIVE_PROBE_CLI
   /* Hardware replay controls only the policy clock; actual command timings,
      pixels and completion ordering remain measured. The real 4 ms threshold
      is covered independently by controlled-clock policy boundary tests. */
   if(g->test_drain_controlled_clock)keep=g->draw.encoder&&native_drain_keep(retained_drains,0,drained==256,ready_head!=g->ring.tail);
   if(g->test_drain_flush)keep=false;
#endif
   /* Keep every original pass usage/barrier/query boundary. Only the command
      encoder submission is coalesced, at most four full drains / 4 ms between
      checks; any blocking command retains its own earlier submission rules. */
   bool flushed=true;if(keep){draw_end_pass(g);g->drain_submits_avoided++;}else{if(g->draw.encoder&&retained_drains>=4)g->drain_bound_flushes++;flushed=completion_progress(g);retained_drains=0;}
   g->metrics.drain_flush_ns+=now_ns()-flush_start;if(g->profile&&!keep)native_profile_stage(g->profile,NP_DRAIN_FLUSH,now_ns()-flush_start,flush_cpu);if(!flushed)break;atomic_store(&g->live_commands,g->ring.executed);atomic_store(&g->live_frames,g->surface_frames);atomic_store(&g->live_draws,g->draw.draws);atomic_store(&g->live_phase,7);sample_memory(g);wgpuDevicePoll(g->device,false,NULL);uint32_t head=atomic_load(heap_word(g,g->ring.base));
   if(head==g->ring.tail){/* Never sleep with an unsubmitted encoder. */if(g->draw.encoder||(g->completion&&g->completion->deferred_count)){uint64_t at=now_ns();bool done=completion_progress(g);g->metrics.drain_flush_ns+=now_ns()-at;retained_drains=0;if(!done)break;}atomic_store(heap_word(g,g->ring.base+20),1);if(atomic_load(heap_word(g,g->ring.base))==head){uint64_t idle_start=now_ns();native_wait32(&g->host->memory,g->ring.base,head,(g->query_pending_pages||g->event_pending)?2000000:100000000);g->metrics.idle_wait_ns+=now_ns()-idle_start;}atomic_store(heap_word(g,g->ring.base+20),0);}
   if(g->profile){uint64_t end_cpu=native_profile_cpu();if(loop_cpu&&end_cpu>=loop_cpu)g->service_cpu_ns+=end_cpu-loop_cpu;else g->service_cpu_errors++;}
  }}
 }
 if(!atomic_load(&g->ready)&&!atomic_load(&g->failed)&&!atomic_load(&g->stop))fail(g,"Native BC/Metal initialization fixture did not complete");atomic_store(&g->live_phase,8);release_hardware(g);wasm_rt_free_thread();}return NULL;}
NativeGpu *native_gpu_create(struct w2c_env *h,void *layer,void(*cancel)(void*),void *context){NativeGpu *g=calloc(1,sizeof(*g));if(!g)return NULL;reset_state(g);g->profile=native_profile_create();g->host=h;if(native_profile_enabled()&&!g->profile&&h->progress)h->progress(h->progress_context,"Optional CPU profile UNAVAILABLE: allocation failed; game continues with existing diagnostics.");g->cancel=cancel;g->context=context;g->objects=CFDictionaryCreateMutable(NULL,0,NULL,NULL);if(!g->objects){free(g->profile);free(g);return NULL;}pthread_mutex_init(&g->error_lock,NULL);if(layer)g->layer=(__bridge_retained void*)(__bridge id)layer;
#ifdef NATIVE_PROBE_CLI
 const char *core=getenv("NATIVE_CPU_CORE_V2_TEST");if(core&&!strcmp(core,"1"))native_gpu_core_option(g,true);
 const char *execution=getenv("NATIVE_RENDERER_V2_TEST");if(execution&&strcmp(execution,"1")==0)native_gpu_execution_option(g,true);
#endif
 return g;}
void native_gpu_surface_size(NativeGpu *g,unsigned width,unsigned height){
 if(!g||!check(g,!g->started&&((width==640&&height==360)||(width==480&&height==270)||(width==320&&height==180)),"Invalid or late surface size"))return;g->surface_width=width;g->surface_height=height;
}
void native_gpu_core_option(NativeGpu *g,bool enabled){
 if(!g||!check(g,!g->started&&g->object_generation==0,"CPU core selection must precede all resource creation"))return;
 if(enabled&&!g->core_index){g->core_index=calloc(1,sizeof(*g->core_index));if(!check(g,g->core_index!=NULL,"CPU core index directory allocation failed"))return;}
 if(!enabled&&g->core_index){core_index_clear(g->core_index);free(g->core_index);g->core_index=NULL;}
 g->core_v2=enabled;g->core_pipeline.valid=false;
}
void native_gpu_execution_option(NativeGpu *g,bool enabled){if(!g||!check(g,!g->started,"Renderer selection must be frozen before GPU start"))return;if(enabled&&!g->completion){g->completion=calloc(1,sizeof(*g->completion));if(!check(g,g->completion!=NULL,"Renderer V2 completion allocation failed"))return;completion_init(g->completion);}else if(!enabled){free(g->completion);g->completion=NULL;}}
void native_gpu_reference_shadow_option(NativeGpu *g,bool enabled){if(g&&check(g,!g->started,"Reference shadow option must be frozen before GPU start"))g->reference_shadow_compat=enabled;}
void native_gpu_start(NativeGpu *g,uint64_t ring,uint64_t size,uint64_t dbg){(void)dbg;if(!g)return;if(!check(g,!g->started&&size<=UINT32_MAX,"Duplicate/invalid GPU service start"))return;if(!gpu_ring_init(&g->ring,(void*)g->host->memory.data,&g->host->memory.size,ring,(uint32_t)size,g,execute,wake)){fail(g,g->ring.error);return;}g->ring_valid=true;if(!check(g,pthread_create(&g->thread,NULL,loop,g)==0,"GPU service pthread creation"))return;g->started=true;}
void native_gpu_stop(NativeGpu *g){if(!g)return;atomic_store(&g->stop,true);if(g->started){wake(g,g->ring.base);pthread_join(g->thread,NULL);g->started=false;}}
void native_gpu_end(NativeGpu *g){if(!g)return;native_gpu_stop(g);if(g->layer){id held=CFBridgingRelease(g->layer);(void)held;}CFRelease(g->objects);pthread_mutex_destroy(&g->error_lock);free(g->profile);free(g->completion);free(g);}
const char *native_gpu_error(NativeGpu *g){return g&&atomic_load_explicit(&g->failed,memory_order_acquire)?g->error:NULL;}
bool native_gpu_ready(NativeGpu *g){return g&&atomic_load(&g->ready);}
void native_gpu_progress(NativeGpu *g,uint64_t *commands,uint64_t *frames){*commands=g?atomic_load(&g->live_commands):0;*frames=g?atomic_load(&g->live_frames):0;}
void native_gpu_snapshot(NativeGpu *g,char *text,size_t size){
 if(!g){snprintf(text,size,"GPU not started");return;}
 static const char *phases[]={"initializing","command execution","local shader read","WGSL compile","pipeline compile","drawable acquisition","GPU completion wait","queue idle","GPU cleanup"};
 uint32_t phase=atomic_load(&g->live_phase);
 snprintf(text,size,"GPU %s; commands %llu; frames %llu; draws %llu; drawable timeouts %llu; last %s; shader %016llx; logical engine GPU payload %llu KiB",phase<9?phases[phase]:"unknown",(unsigned long long)atomic_load(&g->live_commands),(unsigned long long)atomic_load(&g->live_frames),(unsigned long long)atomic_load(&g->live_draws),(unsigned long long)atomic_load(&g->drawable_timeouts),gpu_opcode_name(atomic_load(&g->last_op)),(unsigned long long)atomic_load(&g->live_shader),(unsigned long long)((atomic_load(&g->live_resource_bytes)+1023)/1024));
}
void native_gpu_summary(NativeGpu *g,char *text,size_t size){if(!g){snprintf(text,size,"Native GPU service not created");return;}snprintf(text,size,"Native GPU: %s; ready %u; native BC %u; BC sample formats %u; native format caps %u; SNORM cases %u; commands %llu; uploads %llu; readbacks %llu; surface frames %llu; draws %llu; pipelines %llu (hits %llu); last opcode %s; readback staging peak %u KiB (%u allocations); readback cases %u; instance cases %u; stepped draws/batches %llu/%llu; indirect decodes %llu; depth proxy cases %u; proxy allocations/refreshes/reuses %u/%llu/%llu; proxy peak %llu KiB; live proxy bytes/passes %llu/%u; UAV cases %u; compute pipelines/dispatches %u/%llu; counter reset/initial-count-unused/copy %llu/%llu/%llu; UAV clears %llu; view proxy peak %llu KiB (%u copies), live %llu; mips %llu (live passes %u); presented pixel samples %u, RGB %u/%u; normalized16 cases %u; border cases %u; clip cases %u",g->adapter_name,(unsigned)atomic_load(&g->ready),(unsigned)g->bc_enabled,g->bc_formats_tested,(unsigned)g->format_caps_enabled,g->snorm_cases_tested,(unsigned long long)g->ring.executed,(unsigned long long)g->uploads,(unsigned long long)g->readbacks,(unsigned long long)g->surface_frames,(unsigned long long)g->draw.draws,(unsigned long long)g->draw.creates,(unsigned long long)g->draw.hits,gpu_opcode_name(g->last_op),g->readback_capacity/1024,g->readback_allocations,g->readback_cases_tested,g->instance_cases_tested,(unsigned long long)g->draw.instance_draws,(unsigned long long)g->draw.instance_batches,(unsigned long long)g->draw.indirect_decodes,g->draw.proxy_cases,g->draw.proxy_allocations,(unsigned long long)g->draw.proxy_refreshes,(unsigned long long)g->draw.proxy_reuses,(unsigned long long)((g->draw.proxy_peak+1023)/1024),(unsigned long long)g->draw.proxy_bytes,g->draw.proxy_passes,g->draw.uav_cases,g->draw.compute_shaders,(unsigned long long)g->draw.dispatches,(unsigned long long)g->draw.counter_resets,(unsigned long long)g->draw.counter_ignored,(unsigned long long)g->draw.counter_copies,(unsigned long long)g->draw.uav_clears,(unsigned long long)(g->draw.storage_proxy_peak/1024),g->draw.storage_proxy_copies,(unsigned long long)g->draw.storage_proxy_bytes,(unsigned long long)g->draw.mip_generations,g->draw.mip_passes,g->pixel_samples,g->pixel_min,g->pixel_max,g->norm16_cases_tested,g->border_cases_tested,g->clip_cases_tested);size_t used=strlen(text);if(used<size)snprintf(text+used,size-used,"; byte-edge dispatches/words %llu/%llu; whole-copy subresources/submissions %llu/%llu; resource cases %u; queue submissions %llu",(unsigned long long)g->upload_edge_dispatches,(unsigned long long)g->upload_edge_words,(unsigned long long)g->whole_copy_subresources,(unsigned long long)g->whole_copy_submissions,g->resource_cases,(unsigned long long)g->submissions);used=strlen(text);if(used<size)snprintf(text+used,size-used,"; logical engine GPU payload live/peak %llu/%llu KiB (excludes native intermediates, driver overhead/residency; depth24 is estimated)",(unsigned long long)((g->buffer_bytes+g->texture_bytes+1023)/1024),(unsigned long long)((g->resource_peak+1023)/1024));used=strlen(text);if(used<size)snprintf(text+used,size-used,"; varying limit/links %u/%llu",g->inter_stage_limit,(unsigned long long)g->draw.inter_stage_links);used=strlen(text);if(used<size)snprintf(text+used,size-used,"; varying pixel cases %u",g->inter_stage_cases);used=strlen(text);if(used<size)snprintf(text+used,size-used,"; occlusion count cap %u; query cases %u; scope begins/ends %llu/%llu; segments/maps %llu/%llu; results/empty/stale %llu/%llu/%llu; query pages %u/%u; query GPU live/peak %llu/%llu KiB; pool waits %llu (%llu us)",g->counting_queries,g->query_cases,(unsigned long long)g->query_begins,(unsigned long long)g->query_ends,(unsigned long long)g->query_segments,(unsigned long long)g->query_map_requests,(unsigned long long)g->query_publications,(unsigned long long)g->query_empty,(unsigned long long)g->query_stale,g->query_pages_allocated,QUERY_PAGES,(unsigned long long)(g->query_gpu_bytes/1024),(unsigned long long)((g->query_gpu_peak+1023)/1024),(unsigned long long)g->query_pressure_waits,(unsigned long long)(g->query_pressure_ns/1000));used=strlen(text);if(used<size)snprintf(text+used,size-used,"; optional reference-shadow mode %u uses/allocations %llu/%llu (24 logical bytes, driver allocation unmeasured)",g->reference_shadow_compat,(unsigned long long)g->reference_shadow_uses,(unsigned long long)g->reference_shadow_allocations);}

 #include "proxy_range_fixture.inc"
#include "srv_fixture.inc"
#include "shadow_fixture.inc"
#include "shadow_transition_fixture.inc"
#ifdef NATIVE_PROBE_CLI
#include "shadow_semantic_fixture.inc"
#endif
#include "shadow_eviction_fixture.inc"
#include "perf_fixture.inc"
#include "stability_fixture.inc"
#include "reuse_fixture.inc"
#ifdef NATIVE_PROBE_CLI
#include "core_fixture.inc"
static bool core_benchmark_replays(struct w2c_env *h){
 for(unsigned pair=0;pair<8;pair++)for(unsigned phase=0;phase<2;phase++)for(unsigned order=0;order<2;order++){unsigned mode=(((pair+phase)&1)?5:7)+(((pair+order)&1)?0:1);char text[2400];if(!reuse_fixture(h,mode,text,sizeof(text))){puts(text);return false;}}return true;
}
int native_gpu_core_benchmark(void){
 wasm_rt_init();struct w2c_env *h=calloc(1,sizeof(*h));if(!h)return 1;host_init(h);
 wasm_rt_trap_t trap=wasm_rt_impl_try();if(trap)return 1;
 wasm_rt_allocate_memory_shared(&h->memory,128,65536,false,65536);
 bool ok=core_benchmark_replays(h);host_close(h);wasm_rt_free_memory_shared(&h->memory);pthread_mutex_destroy(&h->memory.mem_lock);free(h);wasm_rt_free_thread();return ok?0:1;
}
#endif
#include "submission_fixture.inc"
#include "ordered_fixture.inc"
#include "copy_cast_fixture.inc"
#include "transfer_fixture.inc"
#include "layout_cache_fixture.inc"
/* Replay uses the exact shared-memory protocol, not direct GPU test calls.
   This small fixture verifies resource uploads, clear/readback and fence
   publication independently of engine startup or game assets. */
bool native_gpu_replay_test(struct w2c_env *h,char *report,size_t capacity){
 if(atomic_load(&h->memory.size)<3*1024*1024){snprintf(report,capacity,"FAIL: replay needs a separate shared-memory fixture");return false;}
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g){snprintf(report,capacity,"FAIL: replay service allocation");return false;}
 g->test_bc=true;g->draw.fixture=true;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+60000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec d={0,1000000};nanosleep(&d,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,fixture_commands=0;uint32_t cmd[128];
 #define EMIT(op,count) do{if((count)>128||tail>127488-8-(count)*4){ok=false;fail(g,"Replay command fixture exceeds ring bounds");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;fixture_commands++;}}while(0)
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
 uint8_t alpha_guards[64];memset(alpha_guards,0xa5,sizeof(alpha_guards));host_store(h,dst+768,alpha_guards,sizeof(alpha_guards));
 memset(cmd,0,sizeof(cmd));cmd[0]=15;cmd[9]=(uint32_t)(dst+768);cmd[10]=(dst+768)>>32;cmd[11]=4;cmd[12]=16;cmd[14]=16;EMIT(42,17);
 cmd[0]=16;cmd[1]=0;cmd[2]=(uint32_t)(flag+8);cmd[3]=(flag+8)>>32;EMIT(44,4);
 cmd[0]=16;EMIT(45,1);cmd[1]=7;EMIT(46,2);EMIT(33,0);
 cmd[0]=17;cmd[1]=2;cmd[2]=4;cmd[3]=4;cmd[4]=1;cmd[5]=1;cmd[6]=1;cmd[7]=71;cmd[8]=8;cmd[9]=1;EMIT(3,10);
 memset(cmd,0,sizeof(cmd));cmd[0]=17;cmd[6]=4;cmd[7]=4;cmd[8]=1;cmd[9]=8;cmd[10]=1;cmd[11]=8;bc_color((uint8_t*)(cmd+12));EMIT(13,14);
 cmd[0]=18;cmd[1]=2;cmd[2]=4;cmd[3]=4;cmd[4]=1;cmd[5]=1;cmd[6]=1;cmd[7]=71;cmd[8]=8;cmd[9]=1;EMIT(3,10);
 cmd[0]=18;cmd[1]=17;EMIT(35,2);cmd[0]=17;EMIT(4,1);cmd[0]=18;EMIT(4,1);
 #include "draw_replay.inc"
 #include "readback_replay.inc"
 #include "instance_replay.inc"
 #include "depth_proxy_replay.inc"
 #include "uav_replay.inc"
 #include "mip_replay.inc"
 #include "resource_replay.inc"
 #include "inter_stage_replay.inc"
 cmd[0]=(uint32_t)(flag+4);cmd[1]=(flag+4)>>32;cmd[2]=99;host_put32(h,flag+4,0);EMIT(8,3);
 #undef EMIT
 atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);
 while(ok&&atomic_load(heap_word(g,flag+4))!=99&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec d={0,1000000};nanosleep(&d,NULL);}
 ok=ok&&!native_gpu_error(g)&&atomic_load(heap_word(g,flag+4))==99&&atomic_load(heap_word(g,flag))==0;
 native_gpu_stop(g);
 if(ok){uint32_t data[16];host_load(h,data,dst,64);for(unsigned i=0;i<16;i++)if(data[i]!=i*19+7)ok=false;host_load(h,data,dst+1024,64);for(unsigned i=0;i<16;i++)if(data[i]!=(i>=4&&i<8?i-4:i)*19+7)ok=false;
  uint8_t pixels[64];for(unsigned copy=0;copy<2;copy++){host_load(h,pixels,dst+256+copy*256,64);for(unsigned i=0;i<16;i++)if(abs(pixels[i*4]-64)>1||abs(pixels[i*4+1]-128)>1||abs(pixels[i*4+2]-191)>1||pixels[i*4+3]!=255)ok=false;}
  host_load(h,pixels,dst+768,64);for(unsigned i=0;i<64;i++)if(pixels[i]!=(i<16?i*17:0xa5))ok=false;if(atomic_load(heap_word(g,flag+8))!=7||host_u64(h,flag+16)!=1)ok=false;
  for(unsigned test=0;test<10&&ok;test++){uint8_t rendered[1024];host_load(h,rendered,dst+4096+test*1024,sizeof(rendered));for(unsigned i=0;i<256&&ok;i++){bool painted=test!=8&&(test!=6||(i%16>=4&&i%16<12&&i/16>=3&&i/16<13));float scale=test==0&&i%16>=8?.5f:(test%3+1)*.25f;int expected[4]={painted?(int)lroundf(200*scale):0,painted?(int)lroundf(100*scale):0,painted?(int)lroundf(50*scale):0,painted?255:0};if(test==7){expected[0]=101;expected[1]=51;expected[2]=57;expected[3]=128;}for(unsigned k=0;k<4;k++)if(abs(rendered[i*4+k]-expected[k])>1){ok=false;snprintf(g->error,sizeof(g->error),"Draw fixture case %u pixel %u channel %u: got %u expected %d",test,i,k,rendered[i*4+k],expected[k]);atomic_store(&g->failed,true);break;}}}
  /* Dynamic scissor shares its pipeline; the array-SRV diagnostic shader
     intentionally adds one distinct pipeline to the non-array baseline. */
  if(g->draw.draws!=41||g->draw.creates!=16||g->draw.hits!=25){fail(g,"Combined draw/pipeline counters failed");ok=false;}
 #include "readback_verify.inc"
 #include "instance_verify.inc"
 #include "depth_proxy_verify.inc"
 #include "uav_verify.inc"
 #include "mip_verify.inc"
 #include "resource_verify.inc"
 #include "inter_stage_verify.inc"
 }
 char summary[1800];native_gpu_summary(g,summary,sizeof(summary));snprintf(report,capacity,"%s: inter-stage output/input relabeling: 3 cases, all 256 pixels checked; consumed location15, flat provoking-vertex colors, unused location15 and preserved clip distances. Native CI limit 15; original vertex inputs and fragment color outputs retained.\n%s: native resource transitions: 44 cases; GPU-only upload neighbors, scratch reuse, array/3D copy bytes, source-destroyed copies and retained MSAA resolve. Bounded 40-byte upload scratch; no CPU readback or completion wait.\n%s: native format compatibility: 18 normalized16 output cases across six formats; exact upload/readback and signed sampling; 18 border sampler cases through production descriptors; all 16 pixels per case checked.\n%s: UAV/compute: 16 output cases; ordered reset/preserve/copy across staging reuse, independent per-view counters, raw/structured clears, original fill/copy with exact ranges, typed storage variants, original draw pixels, mip chains and presented-pixel sampling. 29 original compute pipelines compiled; compilation alone is not scene rendering.\n%s: depth proxy: 14 cases, all 256 pixels checked; depth/stencil float and integer sampling, float comparison, mip/layer ranges, draw/clear/upload/copy invalidation, unchanged-source reuse and cleanup.\n%s: native GPU shared-ring replay, buffer/texture uploads, copies/readbacks, bounded A8 expansion, event query and GPU-completion fence.\n%s: instance input: 10 cases, all 256 pixels checked; rate0/1/2/3, zero stride, mixed rates, nonzero starts, instance IDs, indexed/indirect and bounded counts; no vertex expansion.\n%s: raw readback: 36 cases across 30 formats, mip/array/3D boxes, compressed edges, A8, buffer boxes, guards and transfers above 1 MiB; one staging buffer capped at 1 MiB.\n%s: signed 8-bit R8/RG8/RGBA8 render targets: 9 cases; clear, additive draw, upload, copy, raw signed readback and sampling; all 16 pixels per case verified.\n%s: BC1-BC7 hardware sampling, 11 linear/sRGB format cases, all 16 pixels per case checked against format precision bounds; compressed BC1 ring upload/copy/destruction.\n%s: original startup IM shader pair through DRAW: 10 cases, all 256 pixels per case checked; direct/indexed/instanced/indirect draws, constant changes and offsets, blending, scissor, depth reject/pass; 11 draws; equivalent state reuses cached pipelines.\n%s\n%s",g->inter_stage_cases==3?"PASS":"FAIL",g->resource_cases==44?"PASS":"FAIL",g->norm16_cases_tested==18&&g->border_cases_tested==18?"PASS":"FAIL",g->draw.uav_cases==16?"PASS":"FAIL",g->draw.proxy_cases==14?"PASS":"FAIL",ok?"PASS":"FAIL",g->instance_cases_tested==10?"PASS":"FAIL",g->readback_cases_tested==36?"PASS":"FAIL",g->snorm_cases_tested==9?"PASS":"FAIL",g->bc_formats_tested==11?"PASS":"FAIL",ok?"PASS":"FAIL",summary,native_gpu_error(g)?native_gpu_error(g):"Offscreen fixtures; no game scene is claimed.");native_gpu_end(g);return ok;
}
#include "query_fixture.inc"
#include "coverage_ring_fixture.inc"
#include "query_stop_fixture.inc"
#include "event_fixture.inc"
#include "completion_fixture.inc"
int native_gpu_replay_probe(char *report,size_t capacity){
 wasm_rt_init();struct w2c_env *h=calloc(1,sizeof(*h));if(!h){snprintf(report,capacity,"FAIL: replay host allocation");return 1;}host_init(h);
 volatile bool allocated=false;volatile int result=1;wasm_rt_trap_t trap=wasm_rt_impl_try();
#ifdef NATIVE_PROBE_CLI
 /* The CLI-only 2 MiB copy round trip needs disjoint command and output
    regions. This is a separate test heap, not the production engine budget. */
 const unsigned fixture_pages=128;
#else
 const unsigned fixture_pages=64;
#endif
 if(!trap){wasm_rt_allocate_memory_shared(&h->memory,fixture_pages,65536,false,65536);allocated=true;result=native_gpu_replay_test(h,report,capacity)?0:1;
 if(!result){size_t used=strlen(report);if(used+2>=capacity)result=1;else{report[used++]='\n';report[used]=0;result=query_fixture(h,false,report+used,capacity-used)?0:1;}}
#ifdef NATIVE_PROBE_CLI
 if(!result){size_t used=strlen(report);if(used+2>=capacity)result=1;else{report[used++]='\n';report[used]=0;result=query_fixture(h,true,report+used,capacity-used)?0:1;}}
#endif
 if(!result){char ranges[600];if(!proxy_range_fixture(h,0,ranges,sizeof(ranges)))result=1;size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",ranges);for(unsigned i=1;i<=4&&!result;i++)if(!proxy_range_fixture(h,i,NULL,0))result=1;if(!result){used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\nPASS: 4 explicit SRV range rejection boundaries; no draws/proxy allocations published on invalid descriptors.");}}
 if(!result){char text[850];bool valid=shadow_fixture(h,0,text,sizeof(text));for(unsigned i=1;i<=5&&valid;i++)valid=shadow_fixture(h,i,NULL,0);size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s\n%s: five required/invalid/uncertified cube-binding rejections; no skipped draws or fabricated texture layers.",text,valid?"PASS":"FAIL");if(!valid)result=1;}
 if(!result){char text[900];bool valid=shadow_count_fixture(h,false,text,sizeof(text));size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);
#ifdef NATIVE_PROBE_CLI
 if(valid){valid=shadow_count_fixture(h,true,text,sizeof(text));used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);}
#endif
 if(!valid)result=1;}
 for(unsigned i=0;i<3&&!result;i++){char text[700];bool valid=shadow_transition_fixture(h,i,false,text,sizeof(text));size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);if(!valid)result=1;}
#ifdef NATIVE_PROBE_CLI
 for(unsigned i=0;i<5&&!result;i++){char text[900];bool valid=shadow_transition_fixture(h,i,true,text,sizeof(text));size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);if(!valid)result=1;}
 for(unsigned i=0;i<5&&!result;i++){char text[900];bool valid=shadow_transition_fixture(h,i,2,text,sizeof(text));printf("PS6e9619cde3dac275 %s\n",text);if(!valid)result=1;}
 if(!result&&!shadow_semantic_fixture(h))result=1;
#endif
 if(!result){char text[650];bool valid=shadow_eviction_fixture(h,text,sizeof(text));size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);if(!valid)result=1;}
 if(!result){char text[600];bool valid=layout_cache_fixture(h,false,text,sizeof(text));size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);
#ifdef NATIVE_PROBE_CLI
 if(valid){valid=layout_cache_fixture(h,true,text,sizeof(text));used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);}
#endif
 if(!valid)result=1;}
 if(!result){char text[900];bool valid=pipeline_state_fixture(h,false,text,sizeof(text));size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);if(valid){valid=fresh_object_fixture(h,false,text,sizeof(text));used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);}
#ifdef NATIVE_PROBE_CLI
 if(valid){valid=pipeline_state_fixture(h,true,text,sizeof(text));used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);}if(valid){valid=fresh_object_fixture(h,true,text,sizeof(text));used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);}
#endif
 if(!valid)result=1;}
 if(!result){char text[1000];bool valid=uniform_order_fixture(h,false,text,sizeof(text));size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);
#ifdef NATIVE_PROBE_CLI
 if(valid){valid=uniform_order_fixture(h,true,text,sizeof(text));used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);}
 for(unsigned legacy=0;legacy<2&&valid;legacy++){valid=uniform_map_fixture(h,legacy!=0,text,sizeof(text));used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s",text);}
 const unsigned policies[]={128,256,1,2};for(unsigned i=0;i<4&&valid;i++){valid=pipeline_working_set_fixture(h,policies[i],text,sizeof(text));printf("%s\n",text);}
#endif
 if(!valid)result=1;}
 if(!result){bool valid=srv_pixel_fixture(h);for(unsigned i=0;i<10&&valid;i++)valid=srv_invalid_fixture(h,i);size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s: SRV contract GPU suite: 30 cases / 480 pixels; twelve cube-array faces, nonzero first face, dynamic layer selection, 3D, multisample, uint, null, sRGB view/proxy, depth32/stencil aspects, content refresh and cache reuse; single-layer 2D/array float/uint/sint bindings; ten controlled descriptor/lifetime failures.",valid?"PASS":"FAIL");if(!valid)result=1;}
 if(!result){bool valid=query_stop_fixture(h);for(unsigned i=0;i<8&&valid;i++)valid=query_invalid_fixture(h,i);size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used,"\n%s: query cancellation and 8 explicit order/type/address/sample-mask boundary cases; no completion for an unfinished scope, all pool resources cleaned.",valid?"PASS":"FAIL");if(!valid)result=1;}
#ifdef NATIVE_PROBE_CLI
 if(!result&&!copy_cast_fixture(h))result=1;
 if(!result&&!copy_error_fixture(h))result=1;
 for(unsigned mode=0;mode<2&&!result;mode++)if(!transfer_fixture(h,mode==0))result=1;
 if(!result&&!coverage_ring_fixture(h,false))result=1;
 if(!result&&!coverage_ring_fixture(h,true))result=1;
 if(!result&&!event_callback_fixture(h))result=1;
 if(!result&&!completion_callback_fixture(h))result=1;
 if(!result&&!completion_write_fixture(h))result=1;
 if(!result&&!completion_idle_fixture(h))result=1;
 for(unsigned profile=0;profile<2&&!result;profile++)for(unsigned pair=0;pair<4&&!result;pair++)for(unsigned order=0;order<2&&!result;order++){
  if(!completion_replay(h,((pair+order)&1)!=0,profile!=0,pair,order))result=1;
 }
 for(unsigned legacy=0;legacy<2&&!result;legacy++){char text[1800];bool valid=event_query_fixture(h,legacy!=0,text,sizeof(text));printf("%s\n",text);if(!valid)result=1;}
 for(unsigned legacy=0;legacy<3&&!result;legacy++){char text[2400];bool valid=submission_fixture(h,legacy,text,sizeof(text));printf("%s\n",text);if(!valid)result=1;}
 for(unsigned mode=0;mode<2&&!result;mode++){char text[2400];bool valid=ordered_fixture(h,mode==0,text,sizeof(text));printf("%s\n",text);if(!valid)result=1;}
 if(!result&&!core_fixture())result=1;
 for(unsigned mode=0;mode<3&&!result;mode++){char text[2400];bool valid=reuse_fixture(h,mode,text,sizeof(text));printf("%s\n",text);if(!valid)result=1;}
 for(unsigned pair=0;pair<8&&!result;pair++)for(unsigned order=0;order<2&&!result;order++){unsigned mode=((pair+order)&1)?3:4;char text[2400];bool valid=reuse_fixture(h,mode,text,sizeof(text));printf("%s\n",text);if(!valid)result=1;}
 for(unsigned pair=0;pair<8&&!result;pair++)for(unsigned order=0;order<2&&!result;order++){unsigned mode=((pair+order)&1)?5:6;char text[2400];bool valid=reuse_fixture(h,mode,text,sizeof(text));printf("%s\n",text);if(!valid)result=1;}
#endif
 }else snprintf(report,capacity,"FAIL: GPU replay memory trap %d",trap);
 host_close(h); if(allocated){wasm_rt_free_memory_shared(&h->memory);pthread_mutex_destroy(&h->memory.mem_lock);}free(h);wasm_rt_free_thread();return result;
}

#ifdef NATIVE_PROBE_CLI
#include "shader_audit.inc"
#endif
