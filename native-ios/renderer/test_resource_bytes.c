#include "resource_bytes.h"
#include <assert.h>
#include <stdio.h>
int main(void){uint64_t n;
 assert(resource_texture_bytes(8,8,3,4,1,false,1,1,4,&n)&&n==1020);
 assert(resource_texture_bytes(4,4,4,3,1,true,1,1,4,&n)&&n==292);
 assert(resource_texture_bytes(4,4,2,3,1,false,4,4,8,&n)&&n==48);
 assert(resource_texture_bytes(4,4,1,1,4,false,1,1,4,&n)&&n==256);
 assert(resource_texture_bytes(7,5,1,1,1,false,4,4,16,&n)&&n==64);
 assert(!resource_texture_bytes(0,8,3,4,1,false,1,1,4,&n));assert(!resource_texture_bytes(1,1,1,33,1,false,1,1,4,&n));assert(!resource_texture_bytes(UINT32_MAX,UINT32_MAX,UINT32_MAX,1,4,true,1,1,16,&n));assert(!resource_texture_bytes(1,1,1,1,1,false,0,1,4,&n));assert(!resource_texture_bytes(1,1,1,1,1,false,1,1,4,NULL));
 puts("PASS: logical GPU resource accounting: array and 3D mip chains, MSAA, compressed edge blocks, invalid dimensions and 64-bit overflow guards.");return 0;}
