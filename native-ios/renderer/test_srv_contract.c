#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include "srv_contract.h"
int main(void){
 SrvTexture t={2,4,4,1,1,1};uint32_t w[]={709,0,708,71,4,0,1,0,0};SrvRange r;
 assert(!srv_decode(t,w,&r)&&r.layers==1&&!srv_shader_contract(r,4,false));
 assert(srv_shader_contract(r,9,false)); /* exact active captured mismatch */
 w[4]=9;assert(srv_decode(t,w,&r));t.layers=6;assert(!srv_decode(t,w,&r)&&r.layers==6);
 t.layers=13;w[4]=10;w[7]=1;w[8]=12;assert(!srv_decode(t,w,&r)&&r.first==1&&r.layers==12);
 w[8]=13;assert(srv_decode(t,w,&r));w[8]=UINT32_MAX;assert(!srv_decode(t,w,&r));
 w[7]=2;assert(srv_decode(t,w,&r));w[7]=1;w[8]=0;assert(!srv_decode(t,w,&r));
 t.width=8;assert(srv_decode(t,w,&r));t.width=4;
 w[4]=8;assert(srv_decode(t,w,&r));t.dimension=3;w[7]=0;w[8]=0;assert(!srv_decode(t,w,&r)&&r.layers==1);
 t=(SrvTexture){2,4,4,1,1,4};w[4]=6;w[6]=1;assert(!srv_decode(t,w,&r)&&r.ms);assert(srv_shader_contract(r,4,false));
 w[5]=1;assert(srv_decode(t,w,&r));w[5]=0;w[4]=4;assert(srv_decode(t,w,&r));
 /* Exhaustive small ranges: compare to independent widened arithmetic. */
 unsigned cases=0;for(unsigned total=1;total<=8;total++)for(unsigned layers=1;layers<=13;layers++)for(unsigned mip=0;mip<=total;mip++)for(unsigned first=0;first<=layers;first++)for(unsigned n=0;n<=15;n++){
  t=(SrvTexture){2,128,128,layers,total,1};w[4]=5;w[5]=mip;w[6]=UINT32_MAX;w[7]=first;w[8]=n;
  bool valid=mip<total&&first<layers&&(n==0||(uint64_t)first+n<=layers);
  assert((srv_decode(t,w,&r)==NULL)==valid);if(valid){assert(r.mips==total-mip);assert((uint64_t)r.first+r.layers<=layers);assert(!srv_shader_contract(r,5,false));}cases++;
 }
 const uint32_t edge[]={0,1,6,UINT32_MAX-5,UINT32_MAX};
 for(unsigned a=0;a<5;a++)for(unsigned b=0;b<5;b++){t=(SrvTexture){2,4,4,UINT32_MAX,32,1};w[4]=10;w[5]=edge[a];w[6]=edge[b];w[7]=edge[b];w[8]=edge[a];if(!srv_decode(t,w,&r)){assert((uint64_t)r.mip+r.mips<=32);assert((uint64_t)r.first+r.layers<=UINT32_MAX);assert(r.layers%6==0);}}
 puts("PASS: SRV protocol contracts, captured 708/709 mismatch, cube arrays starting at any face, 3D/MS, sentinels and uint32 overflow boundaries.");printf("Exhaustive range cases: %u\n",cases);return 0;
}
