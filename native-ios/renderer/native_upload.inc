#include "buffer_upload.h"
static bool upload_buffer(NativeGpu *g,const uint8_t *p,uint32_t n){
 if(!check(g,n>=3,"Invalid buffer upload"))return false;Object *o=object(g,gpu_word(p,0));uint32_t at=gpu_word(p,1),bytes=gpu_word(p,2);
 if(!check(g,o&&o->kind==11&&at<=o->size&&bytes<=o->size-at&&((uint64_t)bytes+3)/4<=n-3,"Buffer upload outside bounds"))return false;
 if(o->shadow)memcpy(o->shadow+at,p+12,bytes);
 if(o->buffer){UploadPlan plan;if(!check(g,buffer_upload_plan(o->size,at,bytes,p+12,&plan),"Invalid buffer byte-update plan"))return false;
  if(plan.bytes)wgpuQueueWriteBuffer(g->queue,o->buffer,plan.offset,p+12+plan.source,plan.bytes);
  if(plan.count){
   if(!g->upload_pipeline){
    const char *code="@group(0) @binding(0) var<storage,read_write> words:array<u32>; @group(0) @binding(1) var<uniform> edge_values:array<vec4u,2>; @compute @workgroup_size(1) fn main(@builtin(workgroup_id) id:vec3u){let i=id.x;words[i]=(words[i]&~edge_values[i].x)|edge_values[i].y;}";
    WGPUShaderSourceWGSL src=WGPU_SHADER_SOURCE_WGSL_INIT;src.code=(WGPUStringView){code,WGPU_STRLEN};WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&src.chain;WGPUShaderModule module=wgpuDeviceCreateShaderModule(g->device,&md);if(!module)return false;
    WGPUComputePipelineDescriptor pd=WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;pd.compute.module=module;pd.compute.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};g->upload_pipeline=wgpuDeviceCreateComputePipeline(g->device,&pd);wgpuShaderModuleRelease(module);if(!g->upload_pipeline||atomic_load(&g->failed))return false;
    WGPUBufferDescriptor bd=WGPU_BUFFER_DESCRIPTOR_INIT;bd.size=8;bd.usage=WGPUBufferUsage_Storage|WGPUBufferUsage_CopySrc|WGPUBufferUsage_CopyDst;g->upload_edges=wgpuDeviceCreateBuffer(g->device,&bd);bd.size=32;bd.usage=WGPUBufferUsage_Uniform|WGPUBufferUsage_CopyDst;g->upload_parameters=wgpuDeviceCreateBuffer(g->device,&bd);if(!g->upload_edges||!g->upload_parameters)return false;
    WGPUBindGroupLayout layout=wgpuComputePipelineGetBindGroupLayout(g->upload_pipeline,0);WGPUBindGroupEntry entries[2]={WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT};entries[0].buffer=g->upload_edges;entries[0].size=8;entries[1].binding=1;entries[1].buffer=g->upload_parameters;entries[1].size=32;WGPUBindGroupDescriptor bg=WGPU_BIND_GROUP_DESCRIPTOR_INIT;bg.layout=layout;bg.entryCount=2;bg.entries=entries;g->upload_group=wgpuDeviceCreateBindGroup(g->device,&bg);wgpuBindGroupLayoutRelease(layout);if(!g->upload_group||atomic_load(&g->failed))return false;
   }
   uint32_t parameters[8]={0};for(unsigned i=0;i<plan.count;i++){parameters[i*4]=plan.edge[i].mask;parameters[i*4+1]=plan.edge[i].value;}
   /* Previous pending draws were flushed by execute. Queue writes precede this
      submit, and scratch is reused only in later submitted commands. */
   wgpuQueueWriteBuffer(g->queue,g->upload_parameters,0,parameters,sizeof(parameters));WGPUCommandEncoder e=encoder(g);if(!e)return false;
   for(unsigned i=0;i<plan.count;i++)wgpuCommandEncoderCopyBufferToBuffer(e,o->buffer,plan.edge[i].word,g->upload_edges,i*4,4);
   WGPUComputePassDescriptor d=WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;WGPUComputePassEncoder pass=wgpuCommandEncoderBeginComputePass(e,&d);if(!pass){wgpuCommandEncoderRelease(e);return false;}wgpuComputePassEncoderSetPipeline(pass,g->upload_pipeline);wgpuComputePassEncoderSetBindGroup(pass,0,g->upload_group,0,NULL);wgpuComputePassEncoderDispatchWorkgroups(pass,plan.count,1,1);wgpuComputePassEncoderEnd(pass);wgpuComputePassEncoderRelease(pass);
   for(unsigned i=0;i<plan.count;i++)wgpuCommandEncoderCopyBufferToBuffer(e,g->upload_edges,i*4,o->buffer,plan.edge[i].word,4);
   if(!submit(g,e))return false;g->upload_edge_dispatches++;g->upload_edge_words+=plan.count;
  }
 }
 g->uploads++;return !atomic_load(&g->failed);
}
