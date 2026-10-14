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
 assert(!depth_proxy_range(5,6,5,1,0,1,&m,&l));assert(!depth_proxy_range(5,6,4,2,0,1,&m,&l));assert(!depth_proxy_range(5,6,0,1,6,1,&m,&l));assert(!depth_proxy_range(5,6,0,1,4,3,&m,&l));
 puts("PASS: depth-proxy storage budget, mip/layer view ranges and widened arithmetic.");return 0;
}
