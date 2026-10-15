/* GPU-thread-owned draw state. All caches and uniform storage are bounded. */
#include "shadow_guard.h"
enum { DRAW_BINDINGS=64, DRAW_MEMBERS=16, DRAW_PIPELINES=128,
       DRAW_UBO_BYTES=512*1024, DRAW_UBO_SLOTS=3 };
typedef struct DrawProgram DrawProgram;
typedef struct DepthProxy DepthProxy;
typedef struct MipChain MipChain;
typedef struct {
 uint32_t binding,kind,slot; WGPUTextureViewDimension dim;
 WGPUTextureSampleType sample; bool ms;
 WGPUTextureView view; uint32_t view_id; uint64_t view_epoch;
 WGPUTexture dummy;
 WGPUBuffer dummy_buffer;WGPUTextureFormat format;WGPUStorageTextureAccess access;
} DrawBinding;
struct DrawProgram {
 WGPUShaderModule module; WGPUBindGroupLayout layout;
 WGPUBuffer constants; DrawBinding bindings[DRAW_BINDINGS];
 unsigned count,member_count,pack_size,stage,group,outputs;
 struct {uint32_t slot,offset,size;} members[DRAW_MEMBERS];
 WGPUBindGroup groups[DRAW_UBO_SLOTS];
 uint32_t group_ids[DRAW_UBO_SLOTS][DRAW_BINDINGS];
 uint64_t group_epochs[DRAW_UBO_SLOTS];
 bool vertex_id,instance_id; char hash[17];
 uint32_t proxy_ids[DRAW_BINDINGS]; uint64_t proxy_epoch;
 bool proxy_scan_valid,proxy_any;
 char *source; uint32_t flat_inputs;
 WGPUComputePipeline compute;bool variant;
 DrawProgram *variants[8];uint64_t variant_ages[8],variant_age;
 WGPUTextureFormat storage_formats[8];
 ShadowPlan *shadow_plan;uint32_t inactive_mask;DrawProgram *shadow_variants[SHADOW_VARIANTS];uint64_t shadow_ages[SHADOW_VARIANTS],shadow_age;
};
#include "pipeline_key.h"
typedef struct {
 DrawKey key; WGPURenderPipeline pipeline; uint64_t age;
 unsigned slots; uint32_t last[16],stride[16],rate[16]; bool instance[16],manual_instances;uint8_t gpu_slot[16];
} DrawPipeline;
typedef struct {
 DrawPipeline pipelines[DRAW_PIPELINES]; DrawKey evicted_keys[64];unsigned evicted_count,evicted_next;uint64_t recent_recompiles; uint64_t age,hits,creates,draws,batches,instance_draws,instance_batches,indirect_decodes,inter_stage_links;
 WGPUCommandEncoder encoder; WGPURenderPassEncoder pass;
 uint32_t pass_targets[37]; unsigned width,height;
 WGPUBuffer uniforms[DRAW_UBO_SLOTS]; uint8_t *mirror; unsigned slot,offset;
 uint64_t resource_epoch; CFDictionaryRef index; char directory[80];
 unsigned alignment;
 bool fixture; WGPUSampler dummy_sampler[2]; WGPUBuffer zero_vertex;
 WGPURenderPipeline proxy_pipelines[3]; WGPUBindGroupLayout proxy_layouts[3];
 uint64_t proxy_bytes,proxy_peak,proxy_refreshes,proxy_reuses;
 unsigned proxy_allocations,proxy_passes,proxy_cases;
 uint64_t dispatches,counter_resets,counter_ignored,counter_copies,uav_clears,storage_proxy_bytes,storage_proxy_peak;
 unsigned storage_proxy_copies,uav_cases,compute_shaders;
 struct {WGPUTextureFormat format;WGPURenderPipeline pipeline;} mip_pipelines[16];
 WGPUSampler mip_sampler;uint64_t mip_generations;unsigned mip_passes;
} DrawState;
