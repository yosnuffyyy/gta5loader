#ifndef NATIVE_PIPELINE_POLICY_H
#define NATIVE_PIPELINE_POLICY_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
/* Equality remains an exact DrawKey comparison. The fingerprint only rejects
   nonmatches; neither collisions nor shader specialization can alias keys. */
static inline uint64_t pipeline_fingerprint(const void *bytes,size_t size){
 const unsigned char *p=bytes;uint64_t h=UINT64_C(0x9e3779b97f4a7c15);
 while(size>=8){uint64_t x;memcpy(&x,p,8);h^=x+UINT64_C(0x9e3779b97f4a7c15)+(h<<6)+(h>>2);p+=8;size-=8;}
 while(size--){h^=(uint64_t)(*p++)+UINT64_C(0x9e3779b97f4a7c15)+(h<<6)+(h>>2);}return h;
}
/* Pressure removes only entries unused for ten seconds AND sixty protocol
   presents. Hot working sets keep the existing hard 256-handle bound. Opaque
   driver bytes cannot justify halving every working set at a process threshold. */
static inline bool pipeline_cold(uint64_t now,uint64_t last,uint64_t frame,uint64_t used){
 return now>=last&&now-last>=UINT64_C(10000000000)&&frame>=used&&frame-used>=60;
}
#endif
