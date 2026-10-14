#include "vertex_step.h"
#include "draw_fixture_sources.inc"
#include "draw_shader.inc"
#include "draw_loader_test.inc"
#include "inter_stage.inc"
#include "draw_pipeline.inc"
static bool draw_uniforms(NativeGpu *g){DrawState *s=&g->draw;if(s->mirror)return true;s->mirror=calloc(1,DRAW_UBO_BYTES);if(!check(g,s->mirror!=NULL,"Uniform mirror allocation"))return false;
 WGPULimits limits=WGPU_LIMITS_INIT;if(!check(g,wgpuDeviceGetLimits(g->device,&limits)==WGPUStatus_Success,"Uniform alignment query failed"))return false;s->alignment=limits.minUniformBufferOffsetAlignment;if(s->alignment<256)s->alignment=256;if(!check(g,s->alignment<=65536&&(s->alignment&(s->alignment-1))==0,"Unsupported uniform alignment"))return false;
 for(unsigned i=0;i<DRAW_UBO_SLOTS;i++){WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=DRAW_UBO_BYTES;d.usage=WGPUBufferUsage_Uniform|WGPUBufferUsage_CopyDst;s->uniforms[i]=wgpuDeviceCreateBuffer(g->device,&d);if(!check(g,s->uniforms[i]!=NULL,"Uniform ring allocation"))return false;}return !atomic_load(&g->failed);
}
static void draw_end_pass(NativeGpu *g){query_end_pass(g);if(g->draw.pass){wgpuRenderPassEncoderEnd(g->draw.pass);wgpuRenderPassEncoderRelease(g->draw.pass);g->draw.pass=NULL;}}
static bool draw_flush(NativeGpu *g){DrawState *s=&g->draw;draw_end_pass(g);if(!s->encoder)return !atomic_load(&g->failed);
 if(s->offset)wgpuQueueWriteBuffer(g->queue,s->uniforms[s->slot],0,s->mirror,s->offset);
 WGPUCommandEncoder e=s->encoder;s->encoder=NULL;if(!submit(g,e)||!query_submitted(g))return false;g->counter_slot=0;s->batches++;s->slot=(s->slot+1)%DRAW_UBO_SLOTS;s->offset=0;
 /* Three fixed uniform slots; complete submitted work before reusing slot 0.
    No resource destruction or per-draw queue wait is needed. */
 if(s->slot==0&&!completed(g))return false;return !atomic_load(&g->failed);
}
#include "draw_instance.inc"
#include "draw_depth_proxy.inc"
#include "native_uav.inc"
#include "uav_clear.inc"
#include "native_mips.inc"
#include "sampler_border.h"
static WGPUAddressMode draw_address(uint32_t n){return n==4?(WGPUAddressMode)WGPUNativeAddressMode_ClampToBorder:n==1?WGPUAddressMode_Repeat:n==2||n==5?WGPUAddressMode_MirrorRepeat:WGPUAddressMode_ClampToEdge;}
static WGPUSampler draw_sampler(NativeGpu *g,uint32_t id,bool compare){Object *o=object(g,id);
 if(o){if(!check(g,o->kind==16&&o->meta[1]==3&&o->words==15,"Invalid sampler descriptor"))return NULL;const uint32_t *w=o->meta+2;
  if(!!(w[0]&0x80)!=compare)o=NULL;else if(!o->sampler){WGPUSamplerDescriptor d=WGPU_SAMPLER_DESCRIPTOR_INIT;WGPUSamplerDescriptorExtras extra={0};if(w[1]==4||w[2]==4||w[3]==4){unsigned color=sampler_border_color(w+7);if(!check(g,g->border_enabled&&color,"Border sampler requires a supported exact color and negotiated feature"))return NULL;extra.chain.sType=(WGPUSType)WGPUSType_SamplerDescriptorExtras;extra.samplerBorderColor=(WGPUSamplerBorderColor)color;d.nextInChain=&extra.chain;}d.addressModeU=draw_address(w[1]);d.addressModeV=draw_address(w[2]);d.addressModeW=draw_address(w[3]);d.minFilter=(w[0]>>4)&1?WGPUFilterMode_Linear:WGPUFilterMode_Nearest;d.magFilter=(w[0]>>2)&1?WGPUFilterMode_Linear:WGPUFilterMode_Nearest;d.mipmapFilter=w[0]&1?WGPUMipmapFilterMode_Linear:WGPUMipmapFilterMode_Nearest;float lo=draw_float(w[11]),hi=draw_float(w[12]);if(!check(g,isfinite(lo)&&isfinite(hi),"Invalid sampler LOD bounds"))return NULL;d.lodMinClamp=fmaxf(0,lo);d.lodMaxClamp=fmaxf(d.lodMinClamp,fminf(32,fmaxf(0,hi)));if(compare)d.compare=draw_compare(w[6]);if((w[0]&0x7f)==0x55){d.minFilter=d.magFilter=WGPUFilterMode_Linear;d.mipmapFilter=WGPUMipmapFilterMode_Linear;d.maxAnisotropy=w[5]<1?1:w[5]>16?16:w[5];}o->sampler=wgpuDeviceCreateSampler(g->device,&d);}
 }
 if(o)return o->sampler;unsigned i=compare?1:0;if(!g->draw.dummy_sampler[i]){WGPUSamplerDescriptor d=WGPU_SAMPLER_DESCRIPTOR_INIT;if(compare)d.compare=WGPUCompareFunction_Always;g->draw.dummy_sampler[i]=wgpuDeviceCreateSampler(g->device,&d);}return g->draw.dummy_sampler[i];
}
static WGPUTextureView draw_srv(NativeGpu *g,DrawBinding *b,uint32_t id){
 if(b->view&&b->view_id==id&&b->view_epoch==g->draw.resource_epoch)return b->view;
 if(b->view){wgpuTextureViewRelease(b->view);b->view=NULL;}Object *srv=object(g,id),*o=NULL;
 if(srv){if(!check(g,srv->kind==17&&srv->words==9&&!srv->meta[1],"Texture binding requires a texture SRV"))return NULL;o=object(g,srv->meta[2]);if(!check(g,o&&o->texture,"SRV texture was destroyed or missing"))return NULL;}
 WGPUTextureViewDescriptor d=WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;d.dimension=b->dim;
 if(o){uint32_t mip=srv->meta[5],count=srv->meta[6],first=srv->meta[7],layers=o->td.size.depthOrArrayLayers;
  if(!check(g,mip<o->td.mipLevelCount,"SRV mip outside texture"))return NULL;d.baseMipLevel=mip;d.mipLevelCount=!count||count==UINT32_MAX?o->td.mipLevelCount-mip:count;
  if(b->dim!=WGPUTextureViewDimension_3D){d.baseArrayLayer=first;d.arrayLayerCount=b->dim==WGPUTextureViewDimension_Cube?6:b->dim==WGPUTextureViewDimension_2D||b->dim==WGPUTextureViewDimension_1D?1:srv->meta[8]?srv->meta[8]:layers-first;if(!check(g,first<layers&&d.arrayLayerCount<=layers-first,"SRV array layers outside texture"))return NULL;}
  int kind=proxy_kind(o,b);WGPUTexture texture=o->texture;
  if(kind>=0){if(!check(g,o->proxies[kind]&&o->proxies[kind]->texture,"Depth proxy was not prepared before binding"))return NULL;texture=o->proxies[kind]->texture;if(kind==2)d.aspect=WGPUTextureAspect_DepthOnly;}
  else if(draw_depth(o->td.format))d.aspect=WGPUTextureAspect_DepthOnly;
  else if(srv->meta[3])d.format=format(srv->meta[3],0);
  b->view=wgpuTextureCreateView(texture,&d);
 }else {if(!b->dummy){WGPUTextureDescriptor t=WGPU_TEXTURE_DESCRIPTOR_INIT;t.dimension=b->dim==WGPUTextureViewDimension_3D?WGPUTextureDimension_3D:b->dim==WGPUTextureViewDimension_1D?WGPUTextureDimension_1D:WGPUTextureDimension_2D;t.size=(WGPUExtent3D){1,1,b->dim==WGPUTextureViewDimension_Cube||b->dim==WGPUTextureViewDimension_CubeArray?6:1};t.sampleCount=b->ms?4:1;t.mipLevelCount=1;t.format=b->sample==WGPUTextureSampleType_Uint?WGPUTextureFormat_RGBA8Uint:b->sample==WGPUTextureSampleType_Sint?WGPUTextureFormat_RGBA8Sint:b->sample==WGPUTextureSampleType_Depth?WGPUTextureFormat_Depth32Float:WGPUTextureFormat_RGBA8Unorm;t.usage=WGPUTextureUsage_TextureBinding|(b->ms?WGPUTextureUsage_RenderAttachment:0);b->dummy=wgpuDeviceCreateTexture(g->device,&t);}if(b->dummy)b->view=wgpuTextureCreateView(b->dummy,&d);}
 b->view_id=id;b->view_epoch=g->draw.resource_epoch;return b->view;
}
static WGPUBindGroup draw_group(NativeGpu *g,DrawProgram *p,unsigned *offset){DrawState *s=&g->draw;*offset=0;
 if(p->pack_size){unsigned at=s->offset;if(!check(g,at<=DRAW_UBO_BYTES&&p->pack_size<=DRAW_UBO_BYTES-at,"Uniform ring overflow"))return NULL;memset(s->mirror+at,0,p->pack_size);
  for(unsigned i=0;i<p->member_count;i++){unsigned slot=p->members[i].slot;uint64_t from=(uint64_t)g->cb[p->stage][slot][1]*16;Object *o=object(g,g->cb[p->stage][slot][0]);if(o){if(!check(g,o->kind==11&&o->shadow,"Constant binding requires a shadow buffer"))return NULL;unsigned bytes=from<o->size?o->size-(unsigned)from:0;if(bytes>p->members[i].size)bytes=p->members[i].size;/* Match browser first-constant ABI; num is not applied by the reference. */if(bytes)memcpy(s->mirror+at+p->members[i].offset,o->shadow+from,bytes);}}
  *offset=at;s->offset=(at+p->pack_size+s->alignment-1)&~(s->alignment-1);
 }
 uint32_t ids[DRAW_BINDINGS]={0};for(unsigned i=0;i<p->count;i++){DrawBinding *b=p->bindings+i;if(b->kind==2||b->kind==3)ids[i]=g->smp[p->stage][b->slot];else if(b->kind==4)ids[i]=g->srv[p->stage][b->slot];else if(b->kind>=5)ids[i]=storage_binding_id(g,p,b);}
 unsigned slot=s->slot;if(p->groups[slot]&&p->group_epochs[slot]==s->resource_epoch&&!memcmp(ids,p->group_ids[slot],sizeof(ids)))return p->groups[slot];
 WGPUBindGroupEntry entries[DRAW_BINDINGS];for(unsigned i=0;i<p->count;i++){DrawBinding *b=p->bindings+i;entries[i]=(WGPUBindGroupEntry)WGPU_BIND_GROUP_ENTRY_INIT;entries[i].binding=b->binding;
  if(b->kind==1){entries[i].buffer=b->binding==15?p->constants:s->uniforms[slot];entries[i].size=b->binding==15?WGPU_WHOLE_SIZE:p->pack_size;if(!check(g,entries[i].buffer!=NULL,"Shader special constant table missing"))return NULL;}
  else if(b->kind==2||b->kind==3){entries[i].sampler=draw_sampler(g,ids[i],b->kind==3);if(!entries[i].sampler)return NULL;}
  else if(b->kind==5||b->kind==6){uint64_t at,bytes;entries[i].buffer=storage_buffer(g,b,ids[i],&at,&bytes);entries[i].offset=at;entries[i].size=bytes;if(!entries[i].buffer)return NULL;}
  else if(b->kind==7){entries[i].textureView=storage_texture(g,b,ids[i]);if(!entries[i].textureView)return NULL;}
  else {entries[i].textureView=draw_srv(g,b,ids[i]);if(!entries[i].textureView)return NULL;}
 }
 WGPUBindGroupDescriptor d=WGPU_BIND_GROUP_DESCRIPTOR_INIT;d.layout=p->layout;d.entryCount=p->count;d.entries=entries;WGPUBindGroup group=wgpuDeviceCreateBindGroup(g->device,&d);if(!group||atomic_load(&g->failed))return NULL;if(p->groups[slot])wgpuBindGroupRelease(p->groups[slot]);p->groups[slot]=group;memcpy(p->group_ids[slot],ids,sizeof(ids));p->group_epochs[slot]=s->resource_epoch;return group;
}
static bool draw_pass(NativeGpu *g){DrawState *s=&g->draw;if(s->pass&&!memcmp(g->targets,s->pass_targets,sizeof(g->targets)))return true;draw_end_pass(g);
 WGPURenderPassColorAttachment colors[8];WGPUTextureView views[9]={0};WGPURenderPassDepthStencilAttachment depth=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;unsigned count=0;s->width=s->height=0;bool ok=false;
 for(unsigned i=0;i<8;i++){colors[i]=(WGPURenderPassColorAttachment)WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;Object *o=object(g,g->targets[i*4]);if(!o)continue;WGPUTextureViewDescriptor d=WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;d.dimension=WGPUTextureViewDimension_2D;d.baseMipLevel=g->targets[i*4+1];d.mipLevelCount=1;d.baseArrayLayer=g->targets[i*4+2];d.arrayLayerCount=1;if(g->targets[i*4+3])d.format=format(g->targets[i*4+3],o->bind);views[i]=wgpuTextureCreateView(o->texture,&d);if(!views[i])goto done;colors[i].view=views[i];colors[i].loadOp=WGPULoadOp_Load;colors[i].storeOp=WGPUStoreOp_Store;count=i+1;unsigned w=mip_extent(o->td.size.width,d.baseMipLevel),h=mip_extent(o->td.size.height,d.baseMipLevel);if(s->width&&!check(g,w==s->width&&h==s->height,"Render target dimensions differ"))goto done;s->width=w;s->height=h;}
 Object *ds=object(g,g->targets[32]);if(ds){views[8]=view(ds,g->targets[33],g->targets[34]);if(!views[8])goto done;depth.view=views[8];depth.depthReadOnly=g->targets[36]&1;if(!depth.depthReadOnly){depth.depthLoadOp=WGPULoadOp_Load;depth.depthStoreOp=WGPUStoreOp_Store;}if(draw_stencil(ds->td.format)){depth.stencilReadOnly=(g->targets[36]&2)!=0;if(!depth.stencilReadOnly){depth.stencilLoadOp=WGPULoadOp_Load;depth.stencilStoreOp=WGPUStoreOp_Store;}}if(!s->width){s->width=mip_extent(ds->td.size.width,g->targets[33]);s->height=mip_extent(ds->td.size.height,g->targets[33]);}}
 if(!check(g,s->width&&s->height,"Draw has no render target"))goto done;if(!s->encoder)s->encoder=encoder(g);WGPURenderPassDescriptor d=WGPU_RENDER_PASS_DESCRIPTOR_INIT;d.colorAttachmentCount=count;d.colorAttachments=colors;if(ds)d.depthStencilAttachment=&depth;if(g->query_active_count)d.occlusionQuerySet=g->query_pages[g->query_current-1].set;s->pass=wgpuCommandEncoderBeginRenderPass(s->encoder,&d);ok=s->pass&&!atomic_load(&g->failed);if(ok){memcpy(s->pass_targets,g->targets,sizeof(g->targets));ok=query_begin_pass(g);}
 done:for(unsigned i=0;i<9;i++)if(views[i])wgpuTextureViewRelease(views[i]);return ok;
}
#include "native_compute.inc"
static bool draw_execute(NativeGpu *g,const uint8_t *payload){@autoreleasepool{
 unsigned kind=gpu_word(payload,0),count=gpu_word(payload,1),instances=gpu_word(payload,2),start=gpu_word(payload,3),first_instance=gpu_word(payload,5);int32_t base=(int32_t)gpu_word(payload,4);if(!check(g,kind<=5,"Unknown draw kind"))return false;if(kind<4&&(!count||!instances))return true;
 Object *vs=object(g,g->shader[0]),*ps=object(g,g->shader[1]);DrawProgram *vp=draw_program(g,vs),*pp=ps?draw_program(g,ps):NULL;if(!vp||(ps&&!pp))return false;vp=storage_program(g,vp);if(pp)pp=storage_program(g,pp);if(!vp||(ps&&!pp))return false;
 DrawPipeline *pipeline=draw_pipeline(g,vs,ps,vp,pp);if(!pipeline)return false;
 if(kind>=4&&(pipeline->manual_instances||vp->vertex_id||vp->instance_id)){uint32_t args[5]={0};if(!draw_indirect_decode(g,payload,kind,args))return false;count=args[0];instances=args[1];start=args[2];base=kind==4?(int32_t)args[3]:0;first_instance=args[kind==4?4:3];kind=kind==4?3:2;if(!count||!instances)return true;}
 if(!draw_prepare_proxies(g,vp)||(pp&&!draw_prepare_proxies(g,pp))||!storage_prepare(g,vp)||(pp&&!storage_prepare(g,pp)))return false;
 if(!query_prepare_draw(g)||!draw_uniforms(g))return false;unsigned needed=((vp->pack_size+g->draw.alignment-1)&~(g->draw.alignment-1))+(pp?((pp->pack_size+g->draw.alignment-1)&~(g->draw.alignment-1)):0);if(g->draw.offset>DRAW_UBO_BYTES-needed&&!draw_flush(g))return false;if(!query_prepare_draw(g))return false;
 unsigned offsets[2];WGPUBindGroup groups[2]={draw_group(g,vp,&offsets[0]),NULL};if(pp)groups[1]=draw_group(g,pp,&offsets[1]);if(!groups[0]||(pp&&!groups[1]))return false;if(!draw_pass(g))return false;WGPURenderPassEncoder pass=g->draw.pass;wgpuRenderPassEncoderSetPipeline(pass,pipeline->pipeline);wgpuRenderPassEncoderSetBindGroup(pass,0,groups[0],vp->pack_size?1:0,offsets);if(pp)wgpuRenderPassEncoderSetBindGroup(pass,1,groups[1],pp->pack_size?1:0,offsets+1);
 bool indexed=kind==4||(kind<4&&(kind&1));unsigned vertex_first=indexed||vp->vertex_id?0:start,instance_first=vp->instance_id?0:first_instance;
 uint32_t manual_mask=0;WGPUBuffer manual_buffers[16];uint64_t manual_offsets[16],manual_sizes[16];
 for(unsigned i=0;i<pipeline->slots;i++){if(!pipeline->last[i])continue;bool manual=pipeline->instance[i]&&pipeline->rate[i]!=1;Object *o=object(g,g->vb[i][0]);WGPUBuffer buffer=o?o->buffer:NULL;uint64_t bytes=o?o->size:4096,offset=o?g->vb[i][2]:0;
  if(!buffer){if(!g->draw.zero_vertex){WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=4096;d.usage=WGPUBufferUsage_Vertex;g->draw.zero_vertex=wgpuDeviceCreateBuffer(g->device,&d);}buffer=g->draw.zero_vertex;}
  uint64_t unsigned_first=pipeline->instance[i]?first_instance:start;if((pipeline->instance[i]?(vp->instance_id||manual):vp->vertex_id)&&(pipeline->instance[i]||!indexed)&&!check(g,!pipeline->stride[i]||unsigned_first<=INT64_MAX/pipeline->stride[i],"Vertex shift overflow"))return false;
  int64_t shift=pipeline->instance[i]?((vp->instance_id||manual)?(int64_t)first_instance*pipeline->stride[i]:0):(vp->vertex_id?(indexed?(int64_t)base:(int64_t)start)*pipeline->stride[i]:0);if(!check(g,shift>=-(int64_t)offset&&offset+(uint64_t)shift<=bytes,"Vertex start/base outside buffer"))return false;offset=(uint64_t)((int64_t)offset+shift);
  uint64_t available=bytes-offset;if(!check(g,available>=pipeline->last[i],"Vertex buffer does not contain an attribute"))return false;uint64_t elements=pipeline->stride[i]?(available-pipeline->last[i])/pipeline->stride[i]+1:UINT64_MAX;
  if(kind<4&&manual){uint32_t limit=pipeline->stride[i]?vertex_step_capacity(elements,pipeline->rate[i]):UINT32_MAX;if(instances>limit)instances=limit;}
  else if(kind<4){uint64_t first=pipeline->instance[i]?instance_first:vertex_first;if(pipeline->instance[i]||!indexed){if(!check(g,first<=elements,"Draw starts outside vertex/instance buffer"))return false;uint64_t max=elements-first;if(pipeline->instance[i]){if(instances>max)instances=(unsigned)max;}else if(count>max)count=(unsigned)max;}}
  wgpuRenderPassEncoderSetVertexBuffer(pass,pipeline->gpu_slot[i],buffer,offset,bytes-offset);
  if(manual&&pipeline->rate[i]>1&&pipeline->stride[i]){manual_mask|=1u<<i;manual_buffers[i]=buffer;manual_offsets[i]=offset;manual_sizes[i]=bytes;}
 }
 if(indexed){Object *o=object(g,g->ib[0]);unsigned size=g->ib[1]==57?2:4;if(!check(g,o&&o->buffer&&(g->ib[1]==57||g->ib[1]==42)&&g->ib[2]<=o->size&&g->ib[2]%size==0,"Invalid index buffer/format/offset"))return false;unsigned max=(o->size-g->ib[2])/size;if(kind<4){if(!check(g,start<=max,"Draw starts past index buffer"))return false;if(count>max-start)count=max-start;}wgpuRenderPassEncoderSetIndexBuffer(pass,o->buffer,size==2?WGPUIndexFormat_Uint16:WGPUIndexFormat_Uint32,g->ib[2],o->size-g->ib[2]);}
 if(g->viewports[0]){float v[6];for(unsigned i=0;i<6;i++)v[i]=draw_float(g->viewports[i+1]);if(!check(g,isfinite(v[0])&&isfinite(v[1])&&isfinite(v[2])&&isfinite(v[3])&&isfinite(v[4])&&isfinite(v[5])&&v[2]>0&&v[3]>0&&v[4]>=0&&v[5]<=1&&v[4]<=v[5],"Invalid viewport"))return false;wgpuRenderPassEncoderSetViewport(pass,v[0],v[1],v[2],v[3],v[4],v[5]);}else wgpuRenderPassEncoderSetViewport(pass,0,0,g->draw.width,g->draw.height,0,1);
 Object *raster=object(g,g->raster);unsigned x=0,y=0,w=g->draw.width,h=g->draw.height;if(raster&&raster->meta[9]&&g->scissors[0]){int32_t left=(int32_t)g->scissors[1],top=(int32_t)g->scissors[2],right=(int32_t)g->scissors[3],bottom=(int32_t)g->scissors[4];x=left<0?0:(unsigned)left>w?w:(unsigned)left;y=top<0?0:(unsigned)top>h?h:(unsigned)top;unsigned x1=right<(int32_t)x?x:(unsigned)right>w?w:(unsigned)right,y1=bottom<(int32_t)y?y:(unsigned)bottom>h?h:(unsigned)bottom;w=x1-x;h=y1-y;}
 wgpuRenderPassEncoderSetScissorRect(pass,x,y,w,h);WGPUColor color={draw_float(g->blend[1]),draw_float(g->blend[2]),draw_float(g->blend[3]),draw_float(g->blend[4])};wgpuRenderPassEncoderSetBlendConstant(pass,&color);wgpuRenderPassEncoderSetStencilReference(pass,g->depth[1]);
 if(kind>=4){Object *args=object(g,gpu_word(payload,1));unsigned offset=gpu_word(payload,2),bytes=kind==4?20:16;if(!check(g,args&&args->buffer&&(args->misc&0x10)&&offset%4==0&&offset<=args->size&&bytes<=args->size-offset,"Invalid indirect draw arguments"))return false;if(kind==4)wgpuRenderPassEncoderDrawIndexedIndirect(pass,args->buffer,offset);else wgpuRenderPassEncoderDrawIndirect(pass,args->buffer,offset);}
 else if(count&&instances){
  if(!check(g,(uint64_t)instance_first+instances<=UINT64_C(4294967296),"Instance draw range exceeds 32 bits"))return false;
  if(pipeline->manual_instances)g->draw.instance_draws++;
  for(uint32_t done=0;done<instances;){uint32_t batch=instances-done;
   if(manual_mask){for(unsigned i=0;i<pipeline->slots;i++)if(manual_mask&(1u<<i)){uint32_t run=vertex_step_run(pipeline->rate[i],done,instances-done);if(run<batch)batch=run;}
   for(unsigned i=0;i<pipeline->slots;i++)if(manual_mask&(1u<<i)){uint64_t offset=manual_offsets[i]+vertex_step_index(0,done,pipeline->rate[i])*pipeline->stride[i];if(!check(g,offset<=manual_sizes[i]&&pipeline->last[i]<=manual_sizes[i]-offset,"Stepped instance record outside vertex buffer"))return false;wgpuRenderPassEncoderSetVertexBuffer(pass,pipeline->gpu_slot[i],manual_buffers[i],offset,manual_sizes[i]-offset);}}
   if(indexed)wgpuRenderPassEncoderDrawIndexed(pass,count,batch,start,vp->vertex_id?0:base,instance_first+done);else wgpuRenderPassEncoderDraw(pass,count,batch,vertex_first,instance_first+done);
   if(pipeline->manual_instances)g->draw.instance_batches++;done+=batch;
  }
 }
 if(!storage_written(g,vp)||(pp&&!storage_written(g,pp)))return false;draw_dirty_targets(g,pipeline,pp);g->draw.draws++;return !atomic_load(&g->failed);
}}
static void draw_cleanup(NativeGpu *g){DrawState *s=&g->draw;if(s->pass){if(g->query_pass){wgpuRenderPassEncoderEndOcclusionQuery(s->pass);g->query_pass=false;}wgpuRenderPassEncoderEnd(s->pass);wgpuRenderPassEncoderRelease(s->pass);}if(s->encoder)wgpuCommandEncoderRelease(s->encoder);for(unsigned i=0;i<DRAW_PIPELINES;i++)if(s->pipelines[i].pipeline)wgpuRenderPipelineRelease(s->pipelines[i].pipeline);for(unsigned i=0;i<3;i++){if(s->proxy_pipelines[i])wgpuRenderPipelineRelease(s->proxy_pipelines[i]);if(s->proxy_layouts[i])wgpuBindGroupLayoutRelease(s->proxy_layouts[i]);}for(unsigned i=0;i<DRAW_UBO_SLOTS;i++)if(s->uniforms[i]){wgpuBufferDestroy(s->uniforms[i]);wgpuBufferRelease(s->uniforms[i]);}for(unsigned i=0;i<2;i++)if(s->dummy_sampler[i])wgpuSamplerRelease(s->dummy_sampler[i]);if(s->zero_vertex){wgpuBufferDestroy(s->zero_vertex);wgpuBufferRelease(s->zero_vertex);}for(unsigned i=0;i<16;i++)if(s->mip_pipelines[i].pipeline)wgpuRenderPipelineRelease(s->mip_pipelines[i].pipeline);if(s->mip_sampler)wgpuSamplerRelease(s->mip_sampler);if(s->index)CFRelease(s->index);free(s->mirror);s->mirror=NULL;s->pass=NULL;s->encoder=NULL;}
