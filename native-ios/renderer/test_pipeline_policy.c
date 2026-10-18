#include "pipeline_key.h"
#include "pipeline_policy.h"
#include <assert.h>
#include <time.h>
#include <stdio.h>
static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
static DrawKey keys[256];static uint64_t hashes[256];static volatile unsigned matches;
int main(void){
 assert(!pipeline_cold(9999999999,0,60,0));assert(!pipeline_cold(10000000000,0,59,0));assert(pipeline_cold(10000000000,0,60,0));assert(!pipeline_cold(0,1,60,0));assert(!pipeline_cold(UINT64_MAX,0,0,1));
 for(unsigned i=0;i<256;i++){keys[i].shader[0]=17;keys[i].raster[3]=i;hashes[i]=pipeline_fingerprint(keys+i,sizeof(DrawKey));for(unsigned j=0;j<i;j++)assert(hashes[j]!=hashes[i]);}
 DrawKey collision=keys[0];collision.raster[3]=99;assert(memcmp(&collision,keys,sizeof(collision))); /* even a supplied colliding fingerprint cannot pass exact equality */
 double ms[2];for(unsigned mode=0;mode<2;mode++){uint64_t start=ns();for(unsigned j=0;j<100000;j++){DrawKey k=keys[j%256];uint64_t h=mode?pipeline_fingerprint(&k,sizeof(k)):0;for(unsigned i=0;i<256;i++)if((!mode||hashes[i]==h)&&!memcmp(keys+i,&k,sizeof(k))){matches++;break;}}ms[mode]=(ns()-start)/1e6;}
 assert(matches==200000);printf("PASS: pressure cold-boundaries and exact-key collision safety;100000 same-shader/raster-distinct lookup workload linear %.3fms fingerprint+exact %.3fms;256 keys/%zu bytes each. Synthetic CPU only.\n",ms[0],ms[1],sizeof(DrawKey));return 0;
}
