/* Exact immutable preparation inputs, not a hash. Upload contents, dynamic
   offsets and shadow guards are deliberately NOT cached here: storage_program
   and shadow_program run before this probe on EVERY draw. The resulting variant
   masks/formats are compared. Resource lifecycle changes invalidate globally.
   Actual buffer bounds, usages, proxies, uniforms and pass state are still
   validated by draw_execute. No GPU handle or Object is owned by this cache. */
static CorePipelineInputs core_pipeline_inputs(NativeGpu *g,DrawProgram *vp,DrawProgram *pp){
 CorePipelineInputs input={0};input.created=g->object_generation;input.destroyed=g->draw.resource_epoch;
 input.state[0]=g->shader[0];input.state[1]=g->shader[1];input.state[2]=g->layout;
 input.state[3]=g->topology;input.state[4]=g->ib[0]?g->ib[1]:0;
 input.state[5]=g->blend[0];input.state[6]=g->blend[5];input.state[7]=g->depth[0];input.state[8]=g->raster;
 for(unsigned i=0;i<16;i++)input.strides[i]=g->vb[i][0]?g->vb[i][1]:0;
 memcpy(input.targets,g->targets,sizeof(input.targets));
 input.masks[0]=vp->inactive_mask;input.masks[1]=pp?pp->inactive_mask:0;
 memcpy(input.formats[0],vp->storage_formats,sizeof(input.formats[0]));
 if(pp)memcpy(input.formats[1],pp->storage_formats,sizeof(input.formats[1]));return input;
}
static DrawPipeline *core_pipeline_hit(NativeGpu *g,const CorePipelineInputs *input){
 CorePipeline *cache=&g->core_pipeline;
 if(!cache->valid||cache->epoch!=g->core_pipeline_epoch||memcmp(&cache->input,input,sizeof(*input)))return NULL;
 DrawPipeline *p=&g->draw.pipelines[cache->slot];
 if(!p->pipeline)return NULL;
 p->age=++g->draw.age;p->last_ns=now_ns();p->last_present=g->presents;g->draw.hits++;cache->hits++;return p;
}
static DrawPipeline *core_pipeline_remember(NativeGpu *g,const CorePipelineInputs *input,DrawPipeline *p){
 if(g->core_v2){g->core_pipeline.input=*input;g->core_pipeline.slot=(unsigned)(p-g->draw.pipelines);g->core_pipeline.epoch=g->core_pipeline_epoch;g->core_pipeline.valid=true;g->core_pipeline.misses++;}return p;
}
