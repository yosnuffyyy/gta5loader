/* Exact state of one live render-pass encoder. Retain cached handles so that
   released/recreated frontend objects cannot alias a stale pointer. Reset at
   every pass boundary; no state is inferred across encoders or submissions. */
static void pass_state_reset(NativeGpu *g){
 PassState *s=&g->draw.bound;if(s->pipeline)wgpuRenderPipelineRelease(s->pipeline);
 for(unsigned i=0;i<2;i++)if(s->groups[i])wgpuBindGroupRelease(s->groups[i]);
 for(unsigned i=0;i<16;i++)if(s->vertex[i].buffer)wgpuBufferRelease(s->vertex[i].buffer);
 if(s->index.buffer)wgpuBufferRelease(s->index.buffer);memset(s,0,sizeof(*s));
}
static bool pass_emit(NativeGpu *g,bool equal){
 if(equal){g->draw.state_skipped++;return false;}g->draw.state_emitted++;return true;
}
/* Off control calls the original API directly: no cache retain/release cost
   may be charged to the reference when measuring this optimization. */
#ifdef NATIVE_PROBE_CLI
#define PASS_BASELINE(call) do{if(g->test_no_pass_reuse){g->draw.state_emitted++;call;return;}}while(0)
#else
#define PASS_BASELINE(call) ((void)0)
#endif
static void pass_pipeline(NativeGpu *g,WGPURenderPipeline p){PASS_BASELINE(wgpuRenderPassEncoderSetPipeline(g->draw.pass,p));PassState *s=&g->draw.bound;if(!pass_emit(g,s->pipeline==p))return;wgpuRenderPipelineAddRef(p);if(s->pipeline)wgpuRenderPipelineRelease(s->pipeline);s->pipeline=p;wgpuRenderPassEncoderSetPipeline(g->draw.pass,p);}
static void pass_group(NativeGpu *g,unsigned i,WGPUBindGroup p,unsigned count,const uint32_t *offset){PASS_BASELINE(wgpuRenderPassEncoderSetBindGroup(g->draw.pass,i,p,count,offset));PassState *s=&g->draw.bound;uint32_t at=count?*offset:0;if(!pass_emit(g,s->groups[i]==p&&s->offsets[i]==at&&s->counts[i]==count))return;wgpuBindGroupAddRef(p);if(s->groups[i])wgpuBindGroupRelease(s->groups[i]);s->groups[i]=p;s->offsets[i]=at;s->counts[i]=count;wgpuRenderPassEncoderSetBindGroup(g->draw.pass,i,p,count,offset);}
static void pass_vertex(NativeGpu *g,unsigned i,WGPUBuffer p,uint64_t at,uint64_t bytes){PASS_BASELINE(wgpuRenderPassEncoderSetVertexBuffer(g->draw.pass,i,p,at,bytes));PassBuffer *s=g->draw.bound.vertex+i;if(!pass_emit(g,s->buffer==p&&s->offset==at&&s->size==bytes))return;wgpuBufferAddRef(p);if(s->buffer)wgpuBufferRelease(s->buffer);*s=(PassBuffer){p,at,bytes};wgpuRenderPassEncoderSetVertexBuffer(g->draw.pass,i,p,at,bytes);}
static void pass_index(NativeGpu *g,WGPUBuffer p,WGPUIndexFormat f,uint64_t at,uint64_t bytes){PASS_BASELINE(wgpuRenderPassEncoderSetIndexBuffer(g->draw.pass,p,f,at,bytes));PassState *b=&g->draw.bound;PassBuffer *s=&b->index;if(!pass_emit(g,s->buffer==p&&b->index_format==f&&s->offset==at&&s->size==bytes))return;wgpuBufferAddRef(p);if(s->buffer)wgpuBufferRelease(s->buffer);*s=(PassBuffer){p,at,bytes};b->index_format=f;wgpuRenderPassEncoderSetIndexBuffer(g->draw.pass,p,f,at,bytes);}
static void pass_viewport(NativeGpu *g,float x,float y,float w,float h,float lo,float hi){PASS_BASELINE(wgpuRenderPassEncoderSetViewport(g->draw.pass,x,y,w,h,lo,hi));PassState *s=&g->draw.bound;float v[]={x,y,w,h,lo,hi};if(!pass_emit(g,(s->valid&1)&&!memcmp(s->viewport,v,sizeof(v))))return;memcpy(s->viewport,v,sizeof(v));s->valid|=1;wgpuRenderPassEncoderSetViewport(g->draw.pass,x,y,w,h,lo,hi);}
static void pass_scissor(NativeGpu *g,uint32_t x,uint32_t y,uint32_t w,uint32_t h){PASS_BASELINE(wgpuRenderPassEncoderSetScissorRect(g->draw.pass,x,y,w,h));PassState *s=&g->draw.bound;uint32_t v[]={x,y,w,h};if(!pass_emit(g,(s->valid&2)&&!memcmp(s->scissor,v,sizeof(v))))return;memcpy(s->scissor,v,sizeof(v));s->valid|=2;wgpuRenderPassEncoderSetScissorRect(g->draw.pass,x,y,w,h);}
static void pass_blend(NativeGpu *g,const WGPUColor *v){PASS_BASELINE(wgpuRenderPassEncoderSetBlendConstant(g->draw.pass,v));PassState *s=&g->draw.bound;if(!pass_emit(g,(s->valid&4)&&!memcmp(&s->blend,v,sizeof(*v))))return;s->blend=*v;s->valid|=4;wgpuRenderPassEncoderSetBlendConstant(g->draw.pass,v);}
static void pass_stencil(NativeGpu *g,uint32_t v){PASS_BASELINE(wgpuRenderPassEncoderSetStencilReference(g->draw.pass,v));PassState *s=&g->draw.bound;if(!pass_emit(g,(s->valid&8)&&s->stencil==v))return;s->stencil=v;s->valid|=8;wgpuRenderPassEncoderSetStencilReference(g->draw.pass,v);}

#undef PASS_BASELINE
