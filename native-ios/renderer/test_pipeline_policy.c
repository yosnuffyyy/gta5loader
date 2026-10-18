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
 for(unsigned workload=0;workload<4;workload++){
  for(unsigned i=0;i<256;i++){keys[i].shader[0]=workload==3?17+i:17;hashes[i]=pipeline_fingerprint(keys+i,sizeof(DrawKey));}
  double ms[3];for(unsigned mode=0;mode<3;mode++){uint64_t start=ns();for(unsigned j=0;j<100000;j++){unsigned index=workload==1?0:workload==2?255:j%256;DrawKey k=keys[index];if(mode==2&&!memcmp(keys,&k,sizeof(k))){matches++;continue;}uint64_t h=mode?pipeline_fingerprint(&k,sizeof(k)):0;for(unsigned i=0;i<256;i++)if((!mode||hashes[i]==h)&&!memcmp(keys+i,&k,sizeof(k))){matches++;break;}}ms[mode]=(ns()-start)/1e6;}
  printf("PASS: lookup benchmark workload%u (0 rotating same-shader,1 first-key,2 last-key,3 distinct shaders);100000 lookups linear %.3fms fingerprint+exact %.3fms; first-entry+fingerprint %.3fms;256 keys/%zu bytes each. Synthetic CPU only.\n",workload,ms[0],ms[1],ms[2],sizeof(DrawKey));
 }
 assert(matches==1200000);puts("PASS: pressure cold-boundaries and exact-key collision safety");return 0;
}
