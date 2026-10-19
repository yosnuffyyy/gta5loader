static float draw_float(uint32_t bits){float f;memcpy(&f,&bits,4);return f;}
static bool draw_stencil(WGPUTextureFormat f){return f==WGPUTextureFormat_Depth24PlusStencil8||f==WGPUTextureFormat_Depth32FloatStencil8;}
static bool draw_depth(WGPUTextureFormat f){return draw_stencil(f)||f==WGPUTextureFormat_Depth32Float||f==WGPUTextureFormat_Depth16Unorm;}
static WGPUCompareFunction draw_compare(uint32_t n){static const WGPUCompareFunction a[]={WGPUCompareFunction_Always,WGPUCompareFunction_Never,WGPUCompareFunction_Less,WGPUCompareFunction_Equal,WGPUCompareFunction_LessEqual,WGPUCompareFunction_Greater,WGPUCompareFunction_NotEqual,WGPUCompareFunction_GreaterEqual,WGPUCompareFunction_Always};return n<9?a[n]:WGPUCompareFunction_Undefined;}
static WGPUStencilOperation draw_stencil_op(uint32_t n){static const WGPUStencilOperation a[]={WGPUStencilOperation_Undefined,WGPUStencilOperation_Keep,WGPUStencilOperation_Zero,WGPUStencilOperation_Replace,WGPUStencilOperation_IncrementClamp,WGPUStencilOperation_DecrementClamp,WGPUStencilOperation_Invert,WGPUStencilOperation_IncrementWrap,WGPUStencilOperation_DecrementWrap};return n<9?a[n]:WGPUStencilOperation_Undefined;}
static WGPUBlendFactor draw_factor(uint32_t n){static const WGPUBlendFactor a[]={WGPUBlendFactor_Undefined,WGPUBlendFactor_Zero,WGPUBlendFactor_One,WGPUBlendFactor_Src,WGPUBlendFactor_OneMinusSrc,WGPUBlendFactor_SrcAlpha,WGPUBlendFactor_OneMinusSrcAlpha,WGPUBlendFactor_DstAlpha,WGPUBlendFactor_OneMinusDstAlpha,WGPUBlendFactor_Dst,WGPUBlendFactor_OneMinusDst,WGPUBlendFactor_SrcAlphaSaturated,WGPUBlendFactor_Undefined,WGPUBlendFactor_Undefined,WGPUBlendFactor_Constant,WGPUBlendFactor_OneMinusConstant};return n<16?a[n]:WGPUBlendFactor_Undefined;}
static WGPUBlendOperation draw_blend_op(uint32_t n){static const WGPUBlendOperation a[]={WGPUBlendOperation_Undefined,WGPUBlendOperation_Add,WGPUBlendOperation_Subtract,WGPUBlendOperation_ReverseSubtract,WGPUBlendOperation_Min,WGPUBlendOperation_Max};return n<6?a[n]:WGPUBlendOperation_Undefined;}
static WGPUVertexFormat draw_vertex_format(uint32_t f,unsigned *bytes){switch(f){
 #define VF(dx,name,n) case dx:*bytes=n;return WGPUVertexFormat_##name;
 VF(2,Float32x4,16) VF(6,Float32x3,12) VF(16,Float32x2,8) VF(41,Float32,4)
 VF(3,Uint32x4,16) VF(7,Uint32x3,12) VF(17,Uint32x2,8) VF(42,Uint32,4)
 VF(4,Sint32x4,16) VF(8,Sint32x3,12) VF(18,Sint32x2,8) VF(43,Sint32,4)
 VF(10,Float16x4,8) VF(34,Float16x2,4) VF(11,Unorm16x4,8) VF(35,Unorm16x2,4)
 VF(12,Uint16x4,8) VF(36,Uint16x2,4) VF(13,Snorm16x4,8) VF(37,Snorm16x2,4)
 VF(14,Sint16x4,8) VF(38,Sint16x2,4) VF(28,Unorm8x4,4) VF(30,Uint8x4,4)
 VF(31,Snorm8x4,4) VF(32,Sint8x4,4) VF(49,Unorm8x2,2) VF(50,Uint8x2,2)
 VF(51,Snorm8x2,2) VF(52,Sint8x2,2) VF(87,Unorm8x4BGRA,4) VF(24,Unorm10_10_10_2,4)
 #undef VF
 default:*bytes=0;return WGPUVertexFormat_Force32;
}}
static bool draw_state_words(NativeGpu *g,uint32_t id,uint32_t kind,uint32_t *dst,unsigned count){if(!id)return true;Object *o=object(g,id);if(!check(g,o&&o->kind==16&&o->words==count+2&&o->meta[1]==kind,"Invalid draw state object/descriptor size"))return false;memcpy(dst,o->meta+2,count*4);return true;}
/* Releasing a cache reference preserves references held by encoded/submitted
   commands. No GPU wait is required for LRU eviction. */
static void pipeline_evict(NativeGpu *g,DrawPipeline *p,bool pressure){DrawState *s=&g->draw;if(!p->pipeline)return;s->evicted_keys[s->evicted_next++%64]=p->key;if(s->evicted_count<64)s->evicted_count++;g->metrics.pipeline_evictions++;if(pressure)s->pipeline_pressure_evictions++;wgpuRenderPipelineRelease(p->pipeline);p->pipeline=NULL;s->pipeline_live--;}
static void pipeline_prune_cold(NativeGpu *g,uint64_t now,uint64_t frame){DrawState *s=&g->draw;
 for(unsigned i=0;i<DRAW_PIPELINES&&s->pipeline_live>PIPELINE_PRESSURE_CAPACITY;i++){DrawPipeline *p=s->pipelines+i;if(p->pipeline&&pipeline_cold(now,p->last_ns,frame,p->last_present))pipeline_evict(g,p,true);}
}
static unsigned pipeline_capacity(NativeGpu *g){DrawState *s=&g->draw;
 if(!s->pipeline_capacity)s->pipeline_capacity=PIPELINE_NORMAL_CAPACITY;
#ifdef NATIVE_PROBE_CLI
 if(g->test_pipeline_capacity==2)pipeline_prune_cold(g,now_ns(),g->presents);
 if(g->test_pipeline_capacity)s->pipeline_capacity=g->test_pipeline_capacity==1?(s->draws>=192&&s->draws<384?128:256):g->test_pipeline_capacity==2?256:g->test_pipeline_capacity;
#endif
#if TARGET_OS_IOS
 uint64_t now=now_ns();if(now-s->pipeline_budget_check>=1000000000ull){s->pipeline_budget_check=now;if(os_proc_available_memory()<UINT64_C(256)*1024*1024){pipeline_prune_cold(g,now,g->presents);}}
#endif
 while(s->pipeline_live>s->pipeline_capacity){unsigned oldest=DRAW_PIPELINES;for(unsigned i=0;i<DRAW_PIPELINES;i++)if(s->pipelines[i].pipeline&&(oldest==DRAW_PIPELINES||s->pipelines[i].age<s->pipelines[oldest].age))oldest=i;if(oldest==DRAW_PIPELINES)break;pipeline_evict(g,s->pipelines+oldest,true);}
 return s->pipeline_capacity;}
static DrawPipeline *draw_pipeline(NativeGpu *g,Object *vs,Object *ps,DrawProgram *vp,DrawProgram *pp){
 DrawKey key={0};key.inactive_masks[0]=vp->inactive_mask;key.inactive_masks[1]=pp?pp->inactive_mask:0;memcpy(key.storage_formats[0],vp->storage_formats,sizeof(vp->storage_formats));if(pp)memcpy(key.storage_formats[1],pp->storage_formats,sizeof(pp->storage_formats));key.shader[0]=vs->meta[2];key.shader[1]=vs->meta[3];if(ps){key.shader[2]=ps->meta[2];key.shader[3]=ps->meta[3];}
 Object *layout=object(g,g->layout);if(!check(g,layout&&layout->kind==15,"Draw input layout missing"))return NULL;memcpy(key.layout,layout->meta,layout->words*4);
 /* Object IDs are ownership handles, not vertex-layout semantics. */
#ifdef NATIVE_PROBE_CLI
 if(!g->test_legacy_layout_ids)
#endif
 key.layout[0]=0;
 key.topology=g->topology;key.index_format=g->ib[0]?g->ib[1]:0;key.mask=g->blend[5];
 for(unsigned i=0;i<16;i++)key.strides[i]=g->vb[i][0]?g->vb[i][1]:0;
 if(!draw_state_words(g,g->blend[0],0,key.blend,66)||!draw_state_words(g,g->depth[0],1,key.depth,13)||!draw_state_words(g,g->raster,2,key.raster,10))return NULL;
 if(!g->blend[0])for(unsigned i=0;i<8;i++)key.blend[2+8*i+7]=15;
 if(!g->raster){key.raster[0]=3;key.raster[1]=3;key.raster[6]=1;}
 if(!check(g,key.raster[0]==3,"Wireframe rasterization not yet ported"))return NULL;
 key.samples=1;unsigned colors=0;for(unsigned i=0;i<8;i++){Object *o=object(g,g->targets[4*i]);if(o){if(!check(g,o->texture!=NULL,"Color attachment is not a texture"))return NULL;key.formats[i]=g->targets[4*i+3]?format(g->targets[4*i+3],o->bind):o->td.format;key.samples=o->td.sampleCount;colors=i+1;}}
 Object *ds=object(g,g->targets[32]);if(ds){if(!check(g,ds->texture&&draw_depth(ds->td.format),"Depth attachment format invalid"))return NULL;key.formats[8]=ds->td.format;key.samples=ds->td.sampleCount;key.ro=g->targets[36];}
 /* Pinned Metal HAL silently ignores multisample.mask (device.rs TODO).
    Reject only masks that actually disable a covered sample. Do not fabricate
    query counts or render unmasked pixels until an exact implementation exists. */
 uint32_t sample_bits=key.samples>=32?UINT32_MAX:(1u<<key.samples)-1;
 if(!check(g,(key.mask&sample_bits)==sample_bits,"Metal partial/zero pipeline sample mask is not implemented by pinned backend"))return NULL;
 uint32_t used_slots=0;
#ifdef NATIVE_PROBE_CLI
 if(g->test_unused_layout_keys){for(unsigned i=0;i<layout->meta[1];i++){const uint32_t *e=layout->meta+2+i*7;for(unsigned j=0;j<vs->meta[4];j++){const uint32_t *v=vs->meta+5+j*4;if(v[0]==e[0]&&v[1]==e[1]&&e[3]<16)used_slots|=1u<<e[3];}}}
 else
#endif
 used_slots=pipeline_key_inputs(&key,vs->meta+5,vs->meta[4]);
#ifdef NATIVE_PROBE_CLI
 if(!g->test_raw_pipeline_keys)
#endif
 pipeline_key_normalize(&key,used_slots,pp?pp->outputs:0,ds&&draw_stencil(ds->td.format));
 DrawState *s=&g->draw;unsigned capacity=pipeline_capacity(g);
 /* Preserve the cheap first-entry hit before hashing the full key. Equality
    remains exact, including specializations and all normalized state. */
 DrawPipeline *first=s->pipelines;if(first->pipeline&&!memcmp(&first->key,&key,sizeof(key))){first->age=++s->age;first->last_ns=now_ns();first->last_present=g->presents;s->hits++;return first;}
 uint64_t fingerprint=pipeline_fingerprint(&key,sizeof(key));unsigned empty_slot=DRAW_PIPELINES,oldest=DRAW_PIPELINES;
 for(unsigned i=0;i<DRAW_PIPELINES;i++){DrawPipeline *q=s->pipelines+i;if(!q->pipeline){if(empty_slot==DRAW_PIPELINES)empty_slot=i;continue;}if(q->fingerprint==fingerprint&&!memcmp(&q->key,&key,sizeof(key))){q->age=++s->age;q->last_ns=now_ns();q->last_present=g->presents;s->hits++;return q;}if(oldest==DRAW_PIPELINES||q->age<s->pipelines[oldest].age)oldest=i;}
 unsigned slot=s->pipeline_live<capacity?empty_slot:oldest;if(!check(g,slot<DRAW_PIPELINES,"Pipeline cache slot accounting"))return NULL;
 bool recreated=false;DrawPipeline *p=s->pipelines+slot;for(unsigned i=0;i<s->evicted_count;i++)if(!memcmp(s->evicted_keys+i,&key,sizeof(key))){s->recent_recompiles++;recreated=true;break;}
 if(p->pipeline)pipeline_evict(g,p,false);memset(p,0,sizeof(*p));p->key=key;p->fingerprint=fingerprint;p->last_ns=now_ns();p->last_present=g->presents;p->age=++s->age;
 WGPUShaderModule coverage_module=NULL;WGPUVertexAttribute attributes[16][32];WGPUVertexBufferLayout buffers[16];for(unsigned i=0;i<16;i++){buffers[i]=(WGPUVertexBufferLayout)WGPU_VERTEX_BUFFER_LAYOUT_INIT;buffers[i].attributes=attributes[i];buffers[i].arrayStride=key.strides[i];buffers[i].stepMode=WGPUVertexStepMode_Vertex;}
 for(unsigned i=0;i<layout->meta[1];i++){const uint32_t *e=layout->meta+2+i*7;const uint32_t *sig=NULL;for(unsigned j=0;j<vs->meta[4];j++){const uint32_t *v=vs->meta+5+j*4;if(v[0]==e[0]&&v[1]==e[1]){sig=v;break;}}if(!sig)continue;
  unsigned slot=e[3],bytes=0;if(slot>=16||e[5]>1||(!e[5]&&e[6])||sig[2]>=32){char why[360];snprintf(why,sizeof(why),"Invalid vertex layout: VS %s layout %u element %u slot %u location %u class %u rate %u DXGI %u offset %u",vp->hash,g->layout,i,slot,sig[2],e[5],e[6],e[2],e[4]);fail(g,why);return NULL;}
  if(buffers[slot].attributeCount&&!check(g,p->instance[slot]==(e[5]!=0)&&p->rate[slot]==e[6],"Vertex slot has inconsistent class/instance rate"))return NULL;WGPUVertexFormat vf=draw_vertex_format(e[2],&bytes);if(!check(g,bytes&&e[4]<=UINT32_MAX-bytes,"Unsupported vertex format/offset"))return NULL;
  WGPUVertexAttribute *a=&attributes[slot][buffers[slot].attributeCount++];*a=(WGPUVertexAttribute)WGPU_VERTEX_ATTRIBUTE_INIT;a->format=vf;a->offset=e[4];a->shaderLocation=sig[2];buffers[slot].stepMode=e[5]?WGPUVertexStepMode_Instance:WGPUVertexStepMode_Vertex;buffers[slot].arrayStride=e[5]&&e[6]!=1?0:key.strides[slot];p->rate[slot]=e[6];p->instance[slot]=e[5]!=0;if(e[5]&&e[6]!=1)p->manual_instances=true;p->stride[slot]=key.strides[slot];if(e[4]+bytes>p->last[slot])p->last[slot]=e[4]+bytes;if(slot+1>p->slots)p->slots=slot+1;
 }
 WGPUVertexBufferLayout packed_buffers[16];unsigned used_buffers=0;for(unsigned i=0;i<p->slots;i++)if(buffers[i].attributeCount){p->gpu_slot[i]=(uint8_t)used_buffers;packed_buffers[used_buffers++]=buffers[i];}
 WGPUBindGroupLayout bgl[2]={vp->layout,pp?pp->layout:NULL};WGPUBindGroupLayout empty=NULL;if(!pp){WGPUBindGroupLayoutDescriptor e=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;empty=wgpuDeviceCreateBindGroupLayout(g->device,&e);bgl[1]=empty;}
 WGPUPipelineLayoutDescriptor ld=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;ld.bindGroupLayoutCount=2;ld.bindGroupLayouts=bgl;WGPUPipelineLayout pipeline_layout=wgpuDeviceCreatePipelineLayout(g->device,&ld);
 WGPURenderPipelineDescriptor d=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;d.layout=pipeline_layout;d.vertex.module=vp->module;d.vertex.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};d.vertex.bufferCount=used_buffers;d.vertex.buffers=packed_buffers;
 LinkedModules linked={0};uint64_t link_cpu=g->profile?native_profile_cpu():0;uint64_t link_start=now_ns();bool linked_ok=draw_link_modules(g,@(vp->source),pp?@(pp->source):nil,pp?pp->flat_inputs:0,&linked);timing_add(&g->metrics.windows[METRIC_LINK],now_ns()-link_start);if(g->profile)native_profile_stage(g->profile,NP_LINK,now_ns()-link_start,link_cpu);if(!linked_ok){linked_modules_free(&linked);if(pipeline_layout)wgpuPipelineLayoutRelease(pipeline_layout);if(empty)wgpuBindGroupLayoutRelease(empty);return NULL;}if(linked.vertex)d.vertex.module=linked.vertex;
 static const WGPUPrimitiveTopology topo[]={WGPUPrimitiveTopology_Undefined,WGPUPrimitiveTopology_PointList,WGPUPrimitiveTopology_LineList,WGPUPrimitiveTopology_LineStrip,WGPUPrimitiveTopology_TriangleList,WGPUPrimitiveTopology_TriangleStrip};if(!check(g,key.topology>=1&&key.topology<=5,"Primitive topology unsupported"))goto done;d.primitive.topology=topo[key.topology];if((key.topology==3||key.topology==5)&&key.index_format)d.primitive.stripIndexFormat=key.index_format==57?WGPUIndexFormat_Uint16:WGPUIndexFormat_Uint32;
 d.primitive.frontFace=key.raster[2]?WGPUFrontFace_CCW:WGPUFrontFace_CW;d.primitive.cullMode=key.raster[1]==2?WGPUCullMode_Front:key.raster[1]==3?WGPUCullMode_Back:WGPUCullMode_None;
 if(!key.raster[6]){if(!check(g,wgpuDeviceHasFeature(g->device,WGPUFeatureName_DepthClipControl),"Unclipped depth requires native feature"))goto done;d.primitive.unclippedDepth=true;}
 d.multisample.count=key.samples;d.multisample.mask=key.mask;d.multisample.alphaToCoverageEnabled=key.samples>1&&key.blend[0];
 if(key.blend[0]&&pp){
  /* Explicit shader coverage disables A2C in D3D11. Inspect only the result
     interface, never a sample_mask input. The single-sample variant is compiled
     after varying linking; all original outputs/depth and their types remain. */
  NSString *source=(__bridge NSString*)linked.fragment_source;NSArray *e=draw_matches(@"@fragment\\s*fn\\s+main\\s*\\((?:[^()]|\\([^)]*\\))*\\)\\s*->\\s*([^\\{]+)\\{",source);
  NSString *result=e.count==1?draw_capture(source,e[0],1):@"";
  if(![result containsString:@"@"]){NSArray *st=draw_matches([NSString stringWithFormat:@"struct\\s+%@\\s*\\{([^}]*)\\}",[NSRegularExpression escapedPatternForString:[result stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet]]],source);if(st.count==1)result=draw_capture(source,st[0],1);}
  bool coverage=[result containsString:@"@builtin(sample_mask)"];if(coverage)d.multisample.alphaToCoverageEnabled=false;
  if(key.samples==1&&!coverage){coverage_module=alpha_coverage_module(g,pp,source);if(!check(g,coverage_module&&!atomic_load(&g->failed),"Single-sample alpha coverage specialization failed"))goto done;}
 }
 WGPUDepthStencilState depth=WGPU_DEPTH_STENCIL_STATE_INIT;if(ds){depth.format=key.formats[8];depth.depthWriteEnabled=key.depth[0]&&key.depth[1]==1&&!(key.ro&1)?WGPUOptionalBool_True:WGPUOptionalBool_False;depth.depthCompare=key.depth[0]?draw_compare(key.depth[2]):WGPUCompareFunction_Always;depth.depthBias=(int32_t)key.raster[3];depth.depthBiasClamp=draw_float(key.raster[4]);depth.depthBiasSlopeScale=draw_float(key.raster[5]);
  if(draw_stencil(depth.format)&&key.depth[3]){depth.stencilReadMask=key.depth[4]&255;depth.stencilWriteMask=key.ro&2?0:(key.depth[4]>>8)&255;WGPUStencilFaceState *face[2]={&depth.stencilFront,&depth.stencilBack};for(unsigned f=0;f<2;f++){unsigned q=5+f*4;face[f]->failOp=draw_stencil_op(key.depth[q]);face[f]->depthFailOp=draw_stencil_op(key.depth[q+1]);face[f]->passOp=draw_stencil_op(key.depth[q+2]);face[f]->compare=draw_compare(key.depth[q+3]);}}
  d.depthStencil=&depth;
 }
 WGPUColorTargetState targets[8];WGPUBlendState blends[8];for(unsigned i=0;i<colors;i++){targets[i]=(WGPUColorTargetState)WGPU_COLOR_TARGET_STATE_INIT;targets[i].format=key.formats[i];unsigned q=2+(key.blend[1]?i:0)*8;targets[i].writeMask=pp&&(pp->outputs&(1u<<i))?key.blend[q+7]&15:0;
  if(key.formats[i]&&key.blend[q]){blends[i]=(WGPUBlendState)WGPU_BLEND_STATE_INIT;blends[i].color.srcFactor=draw_factor(key.blend[q+1]);blends[i].color.dstFactor=draw_factor(key.blend[q+2]);blends[i].color.operation=draw_blend_op(key.blend[q+3]);blends[i].alpha.srcFactor=draw_factor(key.blend[q+4]);blends[i].alpha.dstFactor=draw_factor(key.blend[q+5]);blends[i].alpha.operation=draw_blend_op(key.blend[q+6]);if(!check(g,blends[i].color.srcFactor&&blends[i].color.dstFactor&&blends[i].alpha.srcFactor&&blends[i].alpha.dstFactor&&blends[i].color.operation&&blends[i].alpha.operation,"Dual-source or invalid blend descriptor not yet supported"))goto done;targets[i].blend=blends+i;}
 }
 WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;if(pp){fragment.module=coverage_module?coverage_module:linked.fragment?linked.fragment:pp->module;fragment.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};fragment.targetCount=colors;fragment.targets=targets;d.fragment=&fragment;}
 char label[64];snprintf(label,sizeof(label),"%s + %s",vp->hash,pp?pp->hash:"depth");d.label=(WGPUStringView){label,WGPU_STRLEN};atomic_store(&g->live_phase,4);uint64_t pipeline_cpu=g->profile?native_profile_cpu():0;uint64_t pipeline_start=now_ns();p->pipeline=wgpuDeviceCreateRenderPipeline(g->device,&d);uint64_t pipeline_elapsed=now_ns()-pipeline_start;timing_add(&g->metrics.windows[METRIC_PIPELINE],pipeline_elapsed);if(g->profile)native_profile_stage(g->profile,NP_PIPELINE,pipeline_elapsed,pipeline_cpu);pipeline_cost_record(g,&key,recreated,pipeline_elapsed);atomic_store(&g->live_phase,1);s->creates++;if(p->pipeline){s->pipeline_live++;if(s->pipeline_live>s->pipeline_peak)s->pipeline_peak=s->pipeline_live;}
 done:if(coverage_module)wgpuShaderModuleRelease(coverage_module);linked_modules_free(&linked);if(pipeline_layout)wgpuPipelineLayoutRelease(pipeline_layout);if(empty)wgpuBindGroupLayoutRelease(empty);
 if(!check(g,p->pipeline&&!atomic_load(&g->failed),"Native draw pipeline validation failed"))return NULL;return p;
}
