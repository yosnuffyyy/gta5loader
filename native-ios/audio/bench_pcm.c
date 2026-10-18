#include "native_pcm.h"
#include <time.h>
#include <stdio.h>
#include <assert.h>
static _Alignas(8) _Atomic unsigned char data[64+16384*8];
int main(void){NativePCM p={0};atomic_store((_Atomic uint32_t*)(data+8),16384);assert(na_attach(&p,data,sizeof(data),0,16384));atomic_store(&p.enabled,true);float l[256],r[256];struct timespec a,b;clock_gettime(CLOCK_MONOTONIC,&a);
 for(unsigned i=0;i<10000;i++){atomic_store(na_word(&p,0),(i+1)*256);assert(na_read(&p,l,r,256)==256);}
 clock_gettime(CLOCK_MONOTONIC,&b);double seconds=(b.tv_sec-a.tv_sec)+(b.tv_nsec-a.tv_nsec)/1e9;
 printf("PCM host benchmark: 10000 x 256-frame production callbacks, %.3f ms wall, %.3f us/callback; %.3f ms wall per second of 48-kHz audio. O2 synthetic silent PCM, includes cursor publication; excludes decode, audio framework and iPhone scheduling. Context %zu bytes; original ring reused.\n",seconds*1000,seconds*100,seconds*1000*48000/(10000.0*256),sizeof(p));return 0;}
