/* GPU-thread-owned draw state. All caches and uniform storage are bounded. */
enum { DRAW_BINDINGS=64, DRAW_MEMBERS=16, DRAW_PIPELINES=128,
       DRAW_UBO_BYTES=512*1024, DRAW_UBO_SLOTS=3 };
typedef struct DrawProgram DrawProgram;
typedef struct {
 uint32_t binding,kind,slot; WGPUTextureViewDimension dim;
 WGPUTextureSampleType sample; bool ms;
 WGPUTextureView view; uint32_t view_id; uint64_t view_epoch;
 WGPUTexture dummy;
} DrawBinding;
struct DrawProgram {
 WGPUShaderModule module; WGPUBindGroupLayout layout;
 WGPUBuffer constants; DrawBinding bindings[DRAW_BINDINGS];
 unsigned count,member_count,pack_size,stage,outputs;
 struct {uint32_t slot,offset,size;} members[DRAW_MEMBERS];
 WGPUBindGroup groups[DRAW_UBO_SLOTS];
 uint32_t group_ids[DRAW_UBO_SLOTS][DRAW_BINDINGS];
 uint64_t group_epochs[DRAW_UBO_SLOTS];
 bool vertex_id,instance_id; char hash[17];
 char *source; uint32_t flat_inputs;
};
typedef struct {
 uint32_t shader[4],layout[226],strides[16],topology,index_format;
 uint32_t blend[66],depth[13],raster[10],formats[9],samples,ro,mask;
} DrawKey;
typedef struct {
 DrawKey key; WGPURenderPipeline pipeline; uint64_t age;
 unsigned slots; uint32_t last[16],stride[16],rate[16]; bool instance[16],manual_instances;
} DrawPipeline;
typedef struct {
 DrawPipeline pipelines[DRAW_PIPELINES]; uint64_t age,hits,creates,draws,batches,instance_draws,instance_batches,indirect_decodes;
 WGPUCommandEncoder encoder; WGPURenderPassEncoder pass;
 uint32_t pass_targets[37]; unsigned width,height;
 WGPUBuffer uniforms[DRAW_UBO_SLOTS]; uint8_t *mirror; unsigned slot,offset;
 uint64_t resource_epoch; CFDictionaryRef index; char directory[80];
 unsigned alignment;
 bool fixture; WGPUSampler dummy_sampler[2]; WGPUBuffer zero_vertex;
} DrawState;
