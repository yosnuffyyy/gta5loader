#include "gpu_ring.h"
#include <stdio.h>
#include <string.h>
static _Atomic uint32_t *word(GpuRing *r,unsigned i){return (_Atomic uint32_t*)(r->memory+r->base+4*i);}
uint32_t gpu_word(const uint8_t *p,unsigned i){uint32_t x;memcpy(&x,p+4*i,4);return x;}
float gpu_float(const uint8_t *p,unsigned i){float x;memcpy(&x,p+4*i,4);return x;}
const char *gpu_opcode_name(uint32_t op){static const char *names[]={"NOP","WRAP","INIT","CREATE_TEXTURE","DESTROY_OBJECT","CLEAR_RT","CLEAR_DS","PRESENT","FENCE","SCREENSHOT","LOG","CREATE_BUFFER","UPLOAD_BUFFER","UPLOAD_TEXTURE","CREATE_SHADER","CREATE_LAYOUT","CREATE_STATE","CREATE_SRV","SET_SHADER","SET_INPUT_LAYOUT","SET_TOPOLOGY","SET_VERTEX_BUFFERS","SET_INDEX_BUFFER","SET_CBUFFERS","SET_SRVS","SET_SAMPLERS","SET_RENDER_TARGETS","SET_BLEND","SET_DEPTH_STENCIL","SET_RASTER","SET_VIEWPORTS","SET_SCISSORS","DRAW","CLEAR_STATE","COPY_REGION","COPY_RESOURCE","RESOLVE","CREATE_UAV","SET_UAVS","DISPATCH","CLEAR_UAV","GENERATE_MIPS","READBACK","COPY_COUNTER","CREATE_QUERY","BEGIN_QUERY","END_QUERY","UPLOAD_BUFFER_NO_OVERWRITE"};return op<48?names[op]:"UNKNOWN";}
bool gpu_ring_init(GpuRing *r,uint8_t *memory,_Atomic uint64_t *size,uint64_t base,uint32_t capacity,void *context,gpu_execute execute,gpu_wake wake){
 memset(r,0,sizeof(*r));r->memory=memory;r->memory_size=size;r->base=base;r->capacity=capacity;r->tail=64;r->context=context;r->execute=execute;r->wake=wake;
 if(!memory||!size||!execute||base%4||capacity%4||capacity<128||capacity>64*1024*1024){snprintf(r->error,sizeof(r->error),"invalid GPU ring alignment/capacity");return false;}
 uint64_t bytes=atomic_load_explicit(size,memory_order_acquire);if(base>bytes||capacity>bytes-base){snprintf(r->error,sizeof(r->error),"GPU ring outside shared heap");return false;}
 atomic_store_explicit(word(r,1),64,memory_order_release);return true;
}
void gpu_ring_failed(GpuRing *r){atomic_store_explicit(word(r,2),2,memory_order_release);if(r->wake){r->wake(r->context,r->base);r->wake(r->context,r->base+4);r->wake(r->context,r->base+8);}}
static int fail(GpuRing *r,const char *reason){snprintf(r->error,sizeof(r->error),"%s (tail %u)",reason,r->tail);gpu_ring_failed(r);return -1;}
int gpu_ring_drain(GpuRing *r,unsigned budget){
 uint32_t head=atomic_load_explicit(word(r,0),memory_order_acquire);if(head<64||head>r->capacity||head%4)return fail(r,"invalid published GPU head");
 unsigned count=0;bool wrapped=false;
 while(r->tail!=head&&count<budget){
  if(r->tail<64||r->tail>r->capacity-8||r->tail%4)return fail(r,"GPU command header outside ring");
  const uint8_t *command=r->memory+r->base+r->tail;uint32_t op=gpu_word(command,0),n=gpu_word(command,1);
  if(op==1){if(wrapped||head>=r->tail)return fail(r,"invalid or repeated GPU wrap");wrapped=true;r->tail=64;continue;}
  if(op>=48)return fail(r,"unknown GPU opcode");
  if(n>(r->capacity-r->tail-8)/4)return fail(r,"GPU payload exceeds ring");
  uint32_t next=r->tail+8+n*4;
  if(head>r->tail&&next>head)return fail(r,"GPU payload exceeds published head");
  /* Payload is immutable after acquired head publication, until tail release.
     The synchronous executor must finish copying every inline byte before
     returning. Shared heap growth keeps its mmap base stable. */
  if(!r->execute(r->context,op,command+8,n)){snprintf(r->error,sizeof(r->error),"GPU executor stopped at %s (%u), tail %u",gpu_opcode_name(op),op,r->tail);gpu_ring_failed(r);return -1;}
  r->tail=next;r->executed++;count++;
 }
 atomic_fetch_add_explicit(word(r,3),count,memory_order_relaxed);atomic_store_explicit(word(r,1),r->tail,memory_order_release);if(count&&r->wake)r->wake(r->context,r->base+4);return (int)count;
}
