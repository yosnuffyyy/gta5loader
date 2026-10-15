#ifndef NATIVE_PIPELINE_KEY_H
#define NATIVE_PIPELINE_KEY_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
typedef struct {
 uint32_t storage_formats[2][8],inactive_masks[2];uint32_t shader[4],layout[226],strides[16],topology,index_format;
 uint32_t blend[66],depth[13],raster[10],formats[9],samples,ro,mask;
} DrawKey;
/* Normalize only fields demonstrably absent from the resulting pipeline.
   Dynamic scissor and actual index binding still come from current draw state.
   Manual instance strides remain when the vertex shader consumes that slot. */
static inline void pipeline_key_normalize(DrawKey *k,uint32_t used_slots,uint32_t outputs,bool stencil){
 if(k->topology!=3&&k->topology!=5)k->index_format=0;
 for(unsigned i=0;i<16;i++)if(!(used_slots&(1u<<i)))k->strides[i]=0;
 k->raster[7]=0; /* ScissorEnable controls SetScissorRect, not the pipeline. */
 k->mask=k->samples>=32?UINT32_MAX:(1u<<k->samples)-1;
 if(!k->formats[8]){memset(k->depth,0,sizeof(k->depth));k->ro=0;k->raster[3]=k->raster[4]=k->raster[5]=0;}
 else {if(!k->depth[0]){k->depth[1]=0;k->depth[2]=8;}if(!stencil||!k->depth[3]){k->depth[3]=0;memset(k->depth+4,0,9*4);k->ro&=1;}}
 for(unsigned i=0;i<8;i++){uint32_t *b=k->blend+2+i*8;
  if(i&&!k->blend[1]){memset(b,0,8*4);continue;}
  if(!b[0])memset(b+1,0,6*4);b[7]&=15;
  if(k->blend[1]&&(!k->formats[i]||!(outputs&(1u<<i))))b[7]=0;
 }
}
#endif
