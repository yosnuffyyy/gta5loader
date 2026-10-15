#include "srv_contract.h"
/* Source ownership is checked independently of content version. Reusing a
   numeric ID must never make an old view refer to a new allocation. All these
   records are owned by the one GPU consumer thread. */
static Object *srv_source(NativeGpu *g,Object *v){
 if(!check(g,v&&(v->kind==17||v->kind==37)&&v->words==9&&v->meta[1]<=1,"SRV/UAV descriptor missing or invalid"))return NULL;
 Object *o=object(g,v->meta[2]);
 if(!check(g,o&&(v->meta[1]?o->buffer!=NULL:o->texture!=NULL),"SRV/UAV source destroyed, missing or wrong kind"))return NULL;
 if(!check(g,v->source_generation==o->generation,"SRV/UAV source generation changed; recreate the view"))return NULL;
 return o;
}
static uint32_t srv_shader_dimension(DrawBinding *b){
 switch(b->dim){
 case WGPUTextureViewDimension_1D:return 2;
 case WGPUTextureViewDimension_2D:return b->ms?6:4;
 case WGPUTextureViewDimension_2DArray:return b->ms?7:5;
 case WGPUTextureViewDimension_3D:return 8;
 case WGPUTextureViewDimension_Cube:return 9;
 case WGPUTextureViewDimension_CubeArray:return 10;
 default:return 0;
 }
}
static const char *srv_range_contract(Object *o,Object *v,DrawBinding *b,SrvRange *out){
 SrvTexture t={o->td.dimension==WGPUTextureDimension_1D?1:o->td.dimension==WGPUTextureDimension_3D?3:2,o->td.size.width,o->td.size.height,o->td.size.depthOrArrayLayers,o->td.mipLevelCount,o->td.sampleCount};
 const char *why=srv_decode(t,v->meta,out);if(why)return why;
 return srv_shader_contract(*out,srv_shader_dimension(b),b->ms);
}
static bool srv_native_format(NativeGpu *g,Object *o,Object *v,DrawBinding *b){
 WGPUTextureFormat f=v->meta[3]?format(v->meta[3],0):o->td.format;
 bool compatible=f==o->td.format;
 for(unsigned i=0;i<o->td.viewFormatCount;i++)compatible|=f==o->views[i];
 /* D3D's typed depth/stencil SRVs describe an aspect, not a new texture.
    Keep depth-to-float/stencil and color-to-depth adapters, but do not let
    them turn an arbitrary incompatible DXGI view into a valid binding. */
 if(draw_depth(o->td.format)){
  uint32_t dx=v->meta[3];bool stencil=dx==22||dx==47;
  bool aspect=o->td.format==WGPUTextureFormat_Depth32Float?(dx==39||dx==40||dx==41):o->td.format==WGPUTextureFormat_Depth16Unorm?(dx==53||dx==55||dx==56):o->td.format==WGPUTextureFormat_Depth32FloatStencil8?(dx>=19&&dx<=22):(dx>=44&&dx<=47);
  if(!check(g,!dx||aspect,"SRV depth/stencil format belongs to a different resource family"))return false;
  return check(g,stencil?b->sample==WGPUTextureSampleType_Uint:(b->sample==WGPUTextureSampleType_Depth||b->sample==WGPUTextureSampleType_Float||b->sample==WGPUTextureSampleType_UnfilterableFloat),"SRV depth/stencil aspect and shader sample type differ");
 }
 if(!check(g,compatible,"SRV format is not compatible with its backing texture"))return false;
 WGPUTextureSampleType sample=WGPUTextureSampleType_Float;
 switch(f){
 #define CLASS(bits,channels) case WGPUTextureFormat_##channels##bits##Uint:sample=WGPUTextureSampleType_Uint;break;case WGPUTextureFormat_##channels##bits##Sint:sample=WGPUTextureSampleType_Sint;break;
 CLASS(8,R) CLASS(8,RG) CLASS(8,RGBA) CLASS(16,R) CLASS(16,RG) CLASS(16,RGBA) CLASS(32,R) CLASS(32,RG) CLASS(32,RGBA)
 #undef CLASS
 default:break;
 }
 return check(g,sample==WGPUTextureSampleType_Float?(b->sample==WGPUTextureSampleType_Float||b->sample==WGPUTextureSampleType_UnfilterableFloat||b->sample==WGPUTextureSampleType_Depth):b->sample==sample,"SRV numeric format and shader sample type differ");
}
