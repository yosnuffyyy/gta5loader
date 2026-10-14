#ifndef NATIVE_RESOURCE_BYTES_H
#define NATIVE_RESOURCE_BYTES_H
#include <stdint.h>
#include <stdbool.h>
/* Logical texel/block payload only: driver alignment, residency, Metal heaps,
   internal compression and native proxy/uniform/staging resources are excluded. */
static bool resource_texture_bytes(uint32_t w,uint32_t h,uint32_t layers,uint32_t mips,uint32_t samples,bool volume,uint32_t bw,uint32_t bh,uint32_t block,uint64_t *bytes){
 if(!w||!h||!layers||!mips||mips>32||!samples||!bw||!bh||!block||!bytes)return false;
 uint64_t total=0;
 for(unsigned i=0;i<mips;i++){
  uint64_t n=((uint64_t)w+bw-1)/bw,rows=((uint64_t)h+bh-1)/bh;
  const uint64_t factors[]={rows,layers,block,samples};for(unsigned j=0;j<4;j++){if(n>UINT64_MAX/factors[j])return false;n*=factors[j];}
  if(n>UINT64_MAX-total)return false;total+=n;w=w>1?w>>1:1;h=h>1?h>>1:1;if(volume)layers=layers>1?layers>>1:1;
 }
 *bytes=total;return true;
}
#endif
