#include "depth_proxy_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void){uint64_t bytes=0;uint32_t m=0,l=0;
 assert(depth_proxy_size(640,360,1,1,4,&bytes)&&bytes==921600);
 assert(depth_proxy_size(16,8,5,6,4,&bytes)&&bytes==4104);
 assert(depth_proxy_size(16,8,5,6,1,&bytes)&&bytes==1026);
 assert(!depth_proxy_size(UINT32_MAX,16,1,1,4,&bytes));assert(!depth_proxy_size(16,16,6,1,4,&bytes));assert(!depth_proxy_size(16,16,1,UINT32_MAX,4,&bytes));
 assert(!depth_proxy_size(16384,16384,1,1,4,&bytes));assert(!depth_proxy_size(4,4,2,2049,1,&bytes));assert(!depth_proxy_size(4,4,3,2048,1,&bytes));assert(!depth_proxy_size(16,16,1,1,3,&bytes));
 assert(depth_proxy_range(5,6,2,0,1,0,&m,&l)&&m==3&&l==5);
 assert(depth_proxy_range(5,6,2,UINT32_MAX,3,1,&m,&l)&&m==3&&l==1);
 assert(depth_proxy_range(5,6,2,UINT32_MAX,3,UINT32_MAX,&m,&l)&&m==3&&l==3);
 unsigned cases=0;for(uint32_t total=1;total<=15;total++)for(uint32_t layers=1;layers<=12;layers++)for(uint32_t mip=0;mip<total;mip++)for(uint32_t first=0;first<layers;first++){
  assert(depth_proxy_range(total,layers,mip,UINT32_MAX,first,UINT32_MAX,&m,&l)&&m==total-mip&&l==layers-first);
  assert(depth_proxy_range(total,layers,mip,0,first,0,&m,&l)&&m==total-mip&&l==layers-first);
  m=123;l=456;assert(!depth_proxy_range(total,layers,mip,total-mip+1,first,1,&m,&l)&&m==123&&l==456);
  assert(!depth_proxy_range(total,layers,mip,1,first,layers-first+1,&m,&l)&&m==123&&l==456);cases++;
 }
 assert(!depth_proxy_range(5,6,5,1,0,1,&m,&l));assert(!depth_proxy_range(5,6,4,2,0,1,&m,&l));assert(!depth_proxy_range(5,6,0,1,6,1,&m,&l));assert(!depth_proxy_range(5,6,0,1,4,3,&m,&l));
 printf("PASS: depth-proxy storage budget, mip/layer view ranges and widened arithmetic; %u exhaustive remaining-range/invalid boundaries.\n",cases);return 0;
}
