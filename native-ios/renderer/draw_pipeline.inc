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
static DrawPipeline *draw_pipeline(NativeGpu *g,Object *vs,Object *ps,DrawProgram *vp,DrawProgram *pp){
 DrawKey key={0};memcpy(key.storage_formats[0],vp->storage_formats,sizeof(vp->storage_formats));if(pp)memcpy(key.storage_formats[1],pp->storage_formats,sizeof(pp->storage_formats));key.shader[0]=vs->meta[2];key.shader[1]=vs->meta[3];if(ps){key.shader[2]=ps->meta[2];key.shader[3]=ps->meta[3];}
 Object *layout=object(g,g->layout);if(!check(g,layout&&layout->kind==15,"Draw input layout missing"))return NULL;memcpy(key.layout,layout->meta,layout->words*4);
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
 DrawState *s=&g->draw;unsigned slot=0;for(unsigned i=0;i<DRAW_PIPELINES;i++){DrawPipeline *p=s->pipelines+i;if(p->pipeline&&!memcmp(&p->key,&key,sizeof(key))){p->age=++s->age;s->hits++;return p;}if(!p->pipeline){slot=i;break;}if(p->age<s->pipelines[slot].age)slot=i;}
 DrawPipeline *p=s->pipelines+slot;if(p->pipeline)wgpuRenderPipelineRelease(p->pipeline);memset(p,0,sizeof(*p));p->key=key;p->age=++s->age;
 WGPUVertexAttribute attributes[16][32];WGPUVertexBufferLayout buffers[16];for(unsigned i=0;i<16;i++){buffers[i]=(WGPUVertexBufferLayout)WGPU_VERTEX_BUFFER_LAYOUT_INIT;buffers[i].attributes=attributes[i];buffers[i].arrayStride=key.strides[i];buffers[i].stepMode=WGPUVertexStepMode_Vertex;}
 for(unsigned i=0;i<layout->meta[1];i++){const uint32_t *e=layout->meta+2+i*7;const uint32_t *sig=NULL;for(unsigned j=0;j<vs->meta[4];j++){const uint32_t *v=vs->meta+5+j*4;if(v[0]==e[0]&&v[1]==e[1]){sig=v;break;}}if(!sig)continue;
  unsigned slot=e[3],bytes=0;if(slot>=16||e[5]>1||(!e[5]&&e[6])||sig[2]>=32){char why[360];snprintf(why,sizeof(why),"Invalid vertex layout: VS %s layout %u element %u slot %u location %u class %u rate %u DXGI %u offset %u",vp->hash,g->layout,i,slot,sig[2],e[5],e[6],e[2],e[4]);fail(g,why);return NULL;}
  if(buffers[slot].attributeCount&&!check(g,p->instance[slot]==(e[5]!=0)&&p->rate[slot]==e[6],"Vertex slot has inconsistent class/instance rate"))return NULL;WGPUVertexFormat vf=draw_vertex_format(e[2],&bytes);if(!check(g,bytes&&e[4]<=UINT32_MAX-bytes,"Unsupported vertex format/offset"))return NULL;
  WGPUVertexAttribute *a=&attributes[slot][buffers[slot].attributeCount++];*a=(WGPUVertexAttribute)WGPU_VERTEX_ATTRIBUTE_INIT;a->format=vf;a->offset=e[4];a->shaderLocation=sig[2];buffers[slot].stepMode=e[5]?WGPUVertexStepMode_Instance:WGPUVertexStepMode_Vertex;buffers[slot].arrayStride=e[5]&&e[6]!=1?0:key.strides[slot];p->rate[slot]=e[6];p->instance[slot]=e[5]!=0;if(e[5]&&e[6]!=1)p->manual_instances=true;p->stride[slot]=key.strides[slot];if(e[4]+bytes>p->last[slot])p->last[slot]=e[4]+bytes;if(slot+1>p->slots)p->slots=slot+1;
 }
 WGPUVertexBufferLayout packed_buffers[16];unsigned used_buffers=0;for(unsigned i=0;i<p->slots;i++)if(buffers[i].attributeCount){p->gpu_slot[i]=(uint8_t)used_buffers;packed_buffers[used_buffers++]=buffers[i];}
 WGPUBindGroupLayout bgl[2]={vp->layout,pp?pp->layout:NULL};WGPUBindGroupLayout empty=NULL;if(!pp){WGPUBindGroupLayoutDescriptor e=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;empty=wgpuDeviceCreateBindGroupLayout(g->device,&e);bgl[1]=empty;}
 WGPUPipelineLayoutDescriptor ld=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;ld.bindGroupLayoutCount=2;ld.bindGroupLayouts=bgl;WGPUPipelineLayout pipeline_layout=wgpuDeviceCreatePipelineLayout(g->device,&ld);
 WGPURenderPipelineDescriptor d=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;d.layout=pipeline_layout;d.vertex.module=vp->module;d.vertex.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};d.vertex.bufferCount=used_buffers;d.vertex.buffers=packed_buffers;
 LinkedModules linked={0};if(!draw_link_modules(g,@(vp->source),pp?@(pp->source):nil,pp?pp->flat_inputs:0,&linked)){linked_modules_free(&linked);if(pipeline_layout)wgpuPipelineLayoutRelease(pipeline_layout);if(empty)wgpuBindGroupLayoutRelease(empty);return NULL;}if(linked.vertex)d.vertex.module=linked.vertex;
 static const WGPUPrimitiveTopology topo[]={WGPUPrimitiveTopology_Undefined,WGPUPrimitiveTopology_PointList,WGPUPrimitiveTopology_LineList,WGPUPrimitiveTopology_LineStrip,WGPUPrimitiveTopology_TriangleList,WGPUPrimitiveTopology_TriangleStrip};if(!check(g,key.topology>=1&&key.topology<=5,"Primitive topology unsupported"))goto done;d.primitive.topology=topo[key.topology];if((key.topology==3||key.topology==5)&&key.index_format)d.primitive.stripIndexFormat=key.index_format==57?WGPUIndexFormat_Uint16:WGPUIndexFormat_Uint32;
 d.primitive.frontFace=key.raster[2]?WGPUFrontFace_CCW:WGPUFrontFace_CW;d.primitive.cullMode=key.raster[1]==2?WGPUCullMode_Front:key.raster[1]==3?WGPUCullMode_Back:WGPUCullMode_None;
 if(!key.raster[6]){if(!check(g,wgpuDeviceHasFeature(g->device,WGPUFeatureName_DepthClipControl),"Unclipped depth requires native feature"))goto done;d.primitive.unclippedDepth=true;}
 d.multisample.count=key.samples;d.multisample.mask=key.mask;d.multisample.alphaToCoverageEnabled=key.samples>1&&key.blend[0];
 if(!check(g,!key.blend[0]||key.samples>1,"Single-sample alpha-to-coverage shader variant not yet ported"))goto done;
 WGPUDepthStencilState depth=WGPU_DEPTH_STENCIL_STATE_INIT;if(ds){depth.format=key.formats[8];depth.depthWriteEnabled=key.depth[0]&&key.depth[1]==1&&!(key.ro&1)?WGPUOptionalBool_True:WGPUOptionalBool_False;depth.depthCompare=key.depth[0]?draw_compare(key.depth[2]):WGPUCompareFunction_Always;depth.depthBias=(int32_t)key.raster[3];depth.depthBiasClamp=draw_float(key.raster[4]);depth.depthBiasSlopeScale=draw_float(key.raster[5]);
  if(draw_stencil(depth.format)&&key.depth[3]){depth.stencilReadMask=key.depth[4]&255;depth.stencilWriteMask=key.ro&2?0:(key.depth[4]>>8)&255;WGPUStencilFaceState *face[2]={&depth.stencilFront,&depth.stencilBack};for(unsigned f=0;f<2;f++){unsigned q=5+f*4;face[f]->failOp=draw_stencil_op(key.depth[q]);face[f]->depthFailOp=draw_stencil_op(key.depth[q+1]);face[f]->passOp=draw_stencil_op(key.depth[q+2]);face[f]->compare=draw_compare(key.depth[q+3]);}}
  d.depthStencil=&depth;
 }
 WGPUColorTargetState targets[8];WGPUBlendState blends[8];for(unsigned i=0;i<colors;i++){targets[i]=(WGPUColorTargetState)WGPU_COLOR_TARGET_STATE_INIT;targets[i].format=key.formats[i];unsigned q=2+(key.blend[1]?i:0)*8;targets[i].writeMask=pp&&(pp->outputs&(1u<<i))?key.blend[q+7]&15:0;
  if(key.formats[i]&&key.blend[q]){blends[i]=(WGPUBlendState)WGPU_BLEND_STATE_INIT;blends[i].color.srcFactor=draw_factor(key.blend[q+1]);blends[i].color.dstFactor=draw_factor(key.blend[q+2]);blends[i].color.operation=draw_blend_op(key.blend[q+3]);blends[i].alpha.srcFactor=draw_factor(key.blend[q+4]);blends[i].alpha.dstFactor=draw_factor(key.blend[q+5]);blends[i].alpha.operation=draw_blend_op(key.blend[q+6]);if(!check(g,blends[i].color.srcFactor&&blends[i].color.dstFactor&&blends[i].alpha.srcFactor&&blends[i].alpha.dstFactor&&blends[i].color.operation&&blends[i].alpha.operation,"Dual-source or invalid blend descriptor not yet supported"))goto done;targets[i].blend=blends+i;}
 }
 WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;if(pp){fragment.module=linked.fragment?linked.fragment:pp->module;fragment.entryPoint=(WGPUStringView){"main",WGPU_STRLEN};fragment.targetCount=colors;fragment.targets=targets;d.fragment=&fragment;}
 char label[64];snprintf(label,sizeof(label),"%s + %s",vp->hash,pp?pp->hash:"depth");d.label=(WGPUStringView){label,WGPU_STRLEN};atomic_store(&g->live_phase,4);uint64_t pipeline_start=now_ns();p->pipeline=wgpuDeviceCreateRenderPipeline(g->device,&d);timing_add(&g->metrics.windows[METRIC_PIPELINE],now_ns()-pipeline_start);atomic_store(&g->live_phase,1);s->creates++;
 done:linked_modules_free(&linked);if(pipeline_layout)wgpuPipelineLayoutRelease(pipeline_layout);if(empty)wgpuBindGroupLayoutRelease(empty);
 if(!check(g,p->pipeline&&!atomic_load(&g->failed),"Native draw pipeline validation failed"))return NULL;return p;
}
