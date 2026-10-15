#include "depth_proxy_layout.h"
static void texture_history(NativeGpu *g,Object *o){ if(g->host->progress){char line[650];snprintf(line,sizeof(line),"SRV source lifetime: created command %llu; original CREATE_TEXTURE[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]; uploads %llu last command %llu, UPLOAD_TEXTURE[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u], first <=8 bytes %08x/%08x; last bound command %llu stage/slot %u/%u. Live state only; no earlier device history inferred.",(unsigned long long)o->created_command,o->meta&&o->words==10?o->meta[0]:0,o->meta&&o->words==10?o->meta[1]:0,o->meta&&o->words==10?o->meta[2]:0,o->meta&&o->words==10?o->meta[3]:0,o->meta&&o->words==10?o->meta[4]:0,o->meta&&o->words==10?o->meta[5]:0,o->meta&&o->words==10?o->meta[6]:0,o->meta&&o->words==10?o->meta[7]:0,o->meta&&o->words==10?o->meta[8]:0,o->meta&&o->words==10?o->meta[9]:0,(unsigned long long)o->texture_uploads,(unsigned long long)o->last_upload_command,o->last_upload[0],o->last_upload[1],o->last_upload[2],o->last_upload[3],o->last_upload[4],o->last_upload[5],o->last_upload[6],o->last_upload[7],o->last_upload[8],o->last_upload[9],o->last_upload[10],o->last_upload[11],o->upload_prefix[0],o->upload_prefix[1],(unsigned long long)o->last_bound_command,o->last_bind[0],o->last_bind[1]);g->host->progress(g->host->progress_context,line);}
}
/* Match the browser's D3D depth/stencil/float SRV adapters. All conversions
   remain on the GPU and in the same encoder as the dependent game draw.
   Each source owns its proxies; passes/views are cached lazily. Only requested
   mip/layer ranges refresh, and only after a source write. */
typedef struct {WGPUTextureView dst;WGPUBindGroup group;uint64_t version;} ProxyPass;
struct DepthProxy {NativeGpu *owner;WGPUTexture texture;ProxyPass *passes;uint32_t count;uint64_t bytes;};
static void proxy_destroy(Object *o){for(unsigned kind=0;kind<4;kind++){DepthProxy *px=o->proxies[kind];if(!px)continue;
 for(unsigned i=0;i<px->count;i++){if(px->passes[i].group){wgpuBindGroupRelease(px->passes[i].group);px->owner->draw.proxy_passes--;}if(px->passes[i].dst)wgpuTextureViewRelease(px->passes[i].dst);}
 if(px->texture){wgpuTextureDestroy(px->texture);wgpuTextureRelease(px->texture);}px->owner->draw.proxy_bytes-=px->bytes;free(px->passes);free(px);o->proxies[kind]=NULL;
}}
static void proxy_dirty(Object *o){if(!o||!o->texture)return;if(++o->version)return;o->version=1;
 for(unsigned k=0;k<4;k++)if(o->proxies[k])for(unsigned i=0;i<o->proxies[k]->count;i++)o->proxies[k]->passes[i].version=0;
}
static int proxy_kind(Object *o,Object *srv,DrawBinding *b){
 if(draw_depth(o->td.format)){if(b->sample==WGPUTextureSampleType_Depth)return -1;return b->sample==WGPUTextureSampleType_Uint?1:0;}
 return b->sample==WGPUTextureSampleType_Depth?(srv->meta[3]&&format(srv->meta[3],0)!=o->td.format?3:2):-1;
}
static bool proxy_pipeline(NativeGpu *g,unsigned kind){DrawState *s=&g->draw;if(s->proxy_pipelines[kind])return true;
 const char *types[]={"texture_depth_2d","texture_2d<u32>","texture_2d<f32>"};
 const char *fragments[]={"@fragment fn fs(@builtin(position) p:vec4f)->@location(0) vec4f{return vec4f(textureLoad(src,vec2i(p.xy),0),0,0,1);}","@fragment fn fs(@builtin(position) p:vec4f)->@location(0) vec4<u32>{return textureLoad(src,vec2i(p.xy),0);}","@fragment fn fs(@builtin(position) p:vec4f)->@builtin(frag_depth) f32{return textureLoad(src,vec2i(p.xy),0).x;}"};
 char code[1200];snprintf(code,sizeof(code),"@group(0) @binding(0) var src:%s; @vertex fn vs(@builtin(vertex_index) i:u32)->@builtin(position) vec4f{var p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));return vec4f(p[i],0,1);} %s",types[kind],fragments[kind]);
 WGPUShaderSourceWGSL wgsl=WGPU_SHADER_SOURCE_WGSL_INIT;wgsl.code=(WGPUStringView){code,WGPU_STRLEN};WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&wgsl.chain;WGPUShaderModule module=wgpuDeviceCreateShaderModule(g->device,&md);if(!module)return false;
 WGPUBindGroupLayoutEntry entry=WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;entry.binding=0;entry.visibility=WGPUShaderStage_Fragment;entry.texture.viewDimension=WGPUTextureViewDimension_2D;entry.texture.sampleType=kind==0?WGPUTextureSampleType_Depth:kind==1?WGPUTextureSampleType_Uint:WGPUTextureSampleType_UnfilterableFloat;
 WGPUBindGroupLayoutDescriptor bd=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;bd.entryCount=1;bd.entries=&entry;s->proxy_layouts[kind]=wgpuDeviceCreateBindGroupLayout(g->device,&bd);
 WGPUPipelineLayoutDescriptor ld=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;ld.bindGroupLayoutCount=1;ld.bindGroupLayouts=&s->proxy_layouts[kind];WGPUPipelineLayout layout=wgpuDeviceCreatePipelineLayout(g->device,&ld);
 WGPUColorTargetState color=WGPU_COLOR_TARGET_STATE_INIT;color.format=kind==0?WGPUTextureFormat_R32Float:WGPUTextureFormat_R8Uint;
 WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;fragment.module=module;fragment.entryPoint=(WGPUStringView){"fs",WGPU_STRLEN};fragment.targetCount=kind==2?0:1;fragment.targets=kind==2?NULL:&color;
 WGPUDepthStencilState depth=WGPU_DEPTH_STENCIL_STATE_INIT;depth.format=WGPUTextureFormat_Depth32Float;depth.depthWriteEnabled=WGPUOptionalBool_True;depth.depthCompare=WGPUCompareFunction_Always;
 WGPURenderPipelineDescriptor pd=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;pd.layout=layout;pd.vertex.module=module;pd.vertex.entryPoint=(WGPUStringView){"vs",WGPU_STRLEN};pd.fragment=&fragment;if(kind==2)pd.depthStencil=&depth;
 s->proxy_pipelines[kind]=wgpuDeviceCreateRenderPipeline(g->device,&pd);if(layout)wgpuPipelineLayoutRelease(layout);wgpuShaderModuleRelease(module);return check(g,s->proxy_pipelines[kind]&&!atomic_load(&g->failed),"Depth proxy pipeline validation failed");
}
static bool draw_srv_range(NativeGpu *g,Object *o,Object *srv,DrawBinding *b,uint32_t *mips,uint32_t *layers){
 uint32_t requested=b->dim==WGPUTextureViewDimension_2D||b->dim==WGPUTextureViewDimension_1D?1:b->dim==WGPUTextureViewDimension_Cube?6:srv->meta[8];
 SrvRange range;const char *reason=srv_range_contract(o,srv,b,&range);if(!reason){*mips=range.mips;*layers=range.layers;return true;}
 texture_history(g,o);
 char why[700];snprintf(why,sizeof(why),"SRV range outside texture: source %u DXGI %u native %u, dimension %u size %ux%ux%u mips %u samples %u version %llu; SRV %u raw[%u,%u,%u,%u,%u,%u,%u,%u,%u]; shader binding %u slot %u dimension %u sample %u MS %u; requested layers %u. Explicit invalid ranges are not clamped; %s",o->id,o->dxgi,o->td.format,o->td.dimension,o->td.size.width,o->td.size.height,o->td.size.depthOrArrayLayers,o->td.mipLevelCount,o->td.sampleCount,(unsigned long long)o->version,srv->id,srv->meta[0],srv->meta[1],srv->meta[2],srv->meta[3],srv->meta[4],srv->meta[5],srv->meta[6],srv->meta[7],srv->meta[8],b->binding,b->slot,b->dim,b->sample,b->ms,requested,reason);fail(g,why);return false;
}
static WGPUTexture draw_proxy(NativeGpu *g,Object *o,Object *srv,DrawBinding *b,unsigned kind){
 if(o->td.dimension!=WGPUTextureDimension_2D||o->td.sampleCount!=1||b->ms||b->sample==WGPUTextureSampleType_Sint||(kind==1&&!draw_stencil(o->td.format))){char why[300];snprintf(why,sizeof(why),"Unsupported depth proxy: texture %u DXGI %u, dimension %u, samples %u; SRV %u, shader sample %u dimension %u",o->id,o->dxgi,o->td.dimension,o->td.sampleCount,srv->id,b->sample,b->dim);fail(g,why);return NULL;}
 if(kind==0&&b->sample==WGPUTextureSampleType_Float&&!check(g,wgpuDeviceHasFeature(g->device,WGPUFeatureName_Float32Filterable),"Depth-as-float sampling requires the native Float32Filterable capability"))return NULL;
 uint32_t mip=srv->meta[5],first=srv->meta[7],mips=0,layers=0;
 if(!check(g,b->dim==WGPUTextureViewDimension_2D||b->dim==WGPUTextureViewDimension_2DArray||b->dim==WGPUTextureViewDimension_Cube||b->dim==WGPUTextureViewDimension_CubeArray,"Unsupported depth proxy shader dimension")||!draw_srv_range(g,o,srv,b,&mips,&layers))return NULL;
 DepthProxy *px=o->proxies[kind];if(!px){uint64_t bytes=0;if(!check(g,depth_proxy_size(o->td.size.width,o->td.size.height,o->td.mipLevelCount,o->td.size.depthOrArrayLayers,kind==1?1:4,&bytes)&&bytes<=DEPTH_PROXY_BUDGET-g->draw.proxy_bytes,"Depth proxy storage exceeds the 256 MiB total budget"))return NULL;
  px=calloc(1,sizeof(*px));if(!check(g,px!=NULL,"Depth proxy allocation"))return NULL;o->proxies[kind]=px;px->owner=g;px->count=o->td.mipLevelCount*o->td.size.depthOrArrayLayers;px->bytes=bytes;g->draw.proxy_bytes+=bytes;if(g->draw.proxy_bytes>g->draw.proxy_peak)g->draw.proxy_peak=g->draw.proxy_bytes;
  px->passes=calloc(px->count,sizeof(*px->passes));if(!check(g,px->passes!=NULL,"Depth proxy pass table allocation")){px->count=0;return NULL;}
  WGPUTextureDescriptor td=WGPU_TEXTURE_DESCRIPTOR_INIT;td.dimension=WGPUTextureDimension_2D;td.size=o->td.size;td.mipLevelCount=o->td.mipLevelCount;td.sampleCount=1;td.format=kind==0?WGPUTextureFormat_R32Float:kind==1?WGPUTextureFormat_R8Uint:WGPUTextureFormat_Depth32Float;td.usage=WGPUTextureUsage_TextureBinding|WGPUTextureUsage_RenderAttachment;
  px->texture=wgpuDeviceCreateTexture(g->device,&td);if(!check(g,px->texture&&!atomic_load(&g->failed),"Depth proxy texture allocation"))return NULL;g->draw.proxy_allocations++;
 }
 for(uint32_t m=mip;m<mip+mips;m++)for(uint32_t layer=first;layer<first+layers;layer++){
  ProxyPass *r=px->passes+(uint64_t)m*o->td.size.depthOrArrayLayers+layer;if(r->version==o->version){g->draw.proxy_reuses++;continue;}
  unsigned pipeline_kind=kind==3?2:kind;if(!proxy_pipeline(g,pipeline_kind))return NULL;draw_end_pass(g);if(!g->draw.encoder)g->draw.encoder=encoder(g);if(!g->draw.encoder)return NULL;
  if(!r->group){WGPUTextureViewDescriptor vd=WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;vd.dimension=WGPUTextureViewDimension_2D;vd.baseMipLevel=m;vd.mipLevelCount=1;vd.baseArrayLayer=layer;vd.arrayLayerCount=1;vd.aspect=kind==0?WGPUTextureAspect_DepthOnly:kind==1?WGPUTextureAspect_StencilOnly:WGPUTextureAspect_All;if(kind==3)vd.format=format(srv->meta[3],0);
   WGPUTextureView src=wgpuTextureCreateView(o->texture,&vd);vd.aspect=WGPUTextureAspect_All;vd.format=WGPUTextureFormat_Undefined;r->dst=wgpuTextureCreateView(px->texture,&vd);if(!src||!r->dst){if(src)wgpuTextureViewRelease(src);return check(g,false,"Depth proxy views unavailable"),NULL;}
   WGPUBindGroupEntry be=WGPU_BIND_GROUP_ENTRY_INIT;be.binding=0;be.textureView=src;WGPUBindGroupDescriptor bg=WGPU_BIND_GROUP_DESCRIPTOR_INIT;bg.layout=g->draw.proxy_layouts[pipeline_kind];bg.entryCount=1;bg.entries=&be;r->group=wgpuDeviceCreateBindGroup(g->device,&bg);wgpuTextureViewRelease(src);if(!r->group)return NULL;g->draw.proxy_passes++;
  }
  WGPURenderPassDescriptor pd=WGPU_RENDER_PASS_DESCRIPTOR_INIT;WGPURenderPassColorAttachment color=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;WGPURenderPassDepthStencilAttachment depth=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
  if(kind>=2){depth.view=r->dst;depth.depthLoadOp=WGPULoadOp_Clear;depth.depthStoreOp=WGPUStoreOp_Store;depth.depthClearValue=1;pd.depthStencilAttachment=&depth;}
  else {color.view=r->dst;color.loadOp=WGPULoadOp_Clear;color.storeOp=WGPUStoreOp_Store;pd.colorAttachmentCount=1;pd.colorAttachments=&color;}
  WGPURenderPassEncoder pass=wgpuCommandEncoderBeginRenderPass(g->draw.encoder,&pd);if(!pass)return NULL;wgpuRenderPassEncoderSetPipeline(pass,g->draw.proxy_pipelines[pipeline_kind]);wgpuRenderPassEncoderSetBindGroup(pass,0,r->group,0,NULL);wgpuRenderPassEncoderDraw(pass,3,1,0,0);wgpuRenderPassEncoderEnd(pass);wgpuRenderPassEncoderRelease(pass);
  if(atomic_load(&g->failed))return NULL;r->version=o->version;g->draw.proxy_refreshes++;
 }
 return px->texture;
}
static bool draw_prepare_proxies(NativeGpu *g,DrawProgram *p){
 uint32_t ids[DRAW_BINDINGS]={0};for(unsigned i=0;i<p->count;i++)if(p->bindings[i].kind==4)ids[i]=g->srv[p->stage][p->bindings[i].slot];
 if(p->proxy_scan_valid&&!p->proxy_any&&p->proxy_epoch==g->draw.resource_epoch&&!memcmp(ids,p->proxy_ids,sizeof(ids)))return true;
 bool any=false;for(unsigned i=0;i<p->count;i++){DrawBinding *b=p->bindings+i;if(b->kind!=4)continue;if(b->reference_empty){any=true;if(!shadow_reference_prepare(g))return false;continue;}Object *srv=object(g,ids[i]);if(!srv){if(!check(g,!ids[i],"Nonzero texture SRV handle is missing"))return false;continue;}
  if(!check(g,srv->kind==17&&srv->words==9&&!srv->meta[1],"Texture binding requires a texture SRV"))return false;Object *o=srv_source(g,srv);if(!o)return false;uint32_t mc,lc;if(!draw_srv_range(g,o,srv,b,&mc,&lc)||!srv_native_format(g,o,srv,b))return false;int kind=proxy_kind(o,srv,b);if(kind<0)continue;any=true;if(!draw_proxy(g,o,srv,b,(unsigned)kind))return false;
 }
 memcpy(p->proxy_ids,ids,sizeof(ids));p->proxy_epoch=g->draw.resource_epoch;p->proxy_any=any;p->proxy_scan_valid=true;return !atomic_load(&g->failed);
}
static void draw_dirty_targets(NativeGpu *g,DrawPipeline *pipeline,DrawProgram *ps){
 for(unsigned i=0;i<8;i++)if(ps&&(ps->outputs&(1u<<i))){unsigned q=2+(pipeline->key.blend[1]?i:0)*8;if(pipeline->key.blend[q+7]&15)proxy_dirty(object(g,g->targets[i*4]));}
 Object *ds=object(g,g->targets[32]);bool depth=pipeline->key.depth[0]&&pipeline->key.depth[1]==1&&!(g->targets[36]&1),stencil=ds&&draw_stencil(ds->td.format)&&pipeline->key.depth[3]&&((pipeline->key.depth[4]>>8)&255)&&!(g->targets[36]&2);if(depth||stencil)proxy_dirty(ds);
}
