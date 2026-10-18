#ifndef NATIVE_PCM_H
#define NATIVE_PCM_H
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
/* Original engine ABI: stereo float32, 48 kHz, 64-byte header, frame cursors.
   One producer, one callback consumer. Attach/detach only while quiescent.
   Memory reservation must outlive detach and every callback. */
typedef struct {
 _Atomic unsigned char *base;uint32_t capacity;uint64_t address;
 _Atomic bool enabled;_Atomic unsigned readers;
 _Atomic uint64_t calls,frames,underrun_frames,invalid,nonfinite,nonzero;
 _Atomic uint32_t peak,available_max;
} NativePCM;
static inline _Atomic uint32_t *na_word(NativePCM *p,unsigned n){return (_Atomic uint32_t*)(p->base+p->address+4*n);}
static inline bool na_attach(NativePCM *p,_Atomic unsigned char *base,uint64_t size,uint64_t address,uint64_t capacity){
 if(!base||address%8||capacity<256||capacity>1048576||(capacity&(capacity-1))||address>size||64+capacity*8>size-address||atomic_load(&p->enabled)||atomic_load(&p->readers))return false;
 p->base=base;p->address=address;p->capacity=(uint32_t)capacity;
 if(atomic_load_explicit(na_word(p,2),memory_order_acquire)!=capacity){p->base=NULL;return false;}
 return true;
}
static inline void na_max(_Atomic uint32_t *p,uint32_t v){uint32_t old=atomic_load_explicit(p,memory_order_relaxed);if(v>old)atomic_store_explicit(p,v,memory_order_relaxed);}
static inline float na_sample(NativePCM *p,uint64_t at){uint32_t u=0;unsigned char *b=(unsigned char*)&u;for(unsigned i=0;i<4;i++)b[i]=atomic_load_explicit(p->base+at+i,memory_order_relaxed);float f;memcpy(&f,&u,4);return f;}
static inline uint32_t na_read(NativePCM *p,float *left,float *right,uint32_t n){
 /* No allocation, locks, logging, syscalls, waits, or archive access. */
 memset(left,0,(size_t)n*4);memset(right,0,(size_t)n*4);
 atomic_fetch_add_explicit(&p->readers,1,memory_order_seq_cst);
 uint32_t take=0;
 if(atomic_load_explicit(&p->enabled,memory_order_seq_cst)){
  uint32_t read=atomic_load_explicit(na_word(p,1),memory_order_relaxed),write=atomic_load_explicit(na_word(p,0),memory_order_acquire),available=write-read;
  if(available>p->capacity){atomic_fetch_add(&p->invalid,1);goto done;}
  take=available<n?available:n;float peak=0;uint64_t nonzero=0,bad=0;
  for(uint32_t i=0;i<take;i++){uint64_t at=p->address+64+8*((read+i)&(p->capacity-1));float l=na_sample(p,at),r=na_sample(p,at+4);if(!isfinite(l)){l=0;bad++;}if(!isfinite(r)){r=0;bad++;}left[i]=l;right[i]=r;float v=fmaxf(fabsf(l),fabsf(r));if(v>peak)peak=v;if(v>0)nonzero++;}
  atomic_store_explicit(na_word(p,1),read+take,memory_order_release);
  atomic_fetch_add_explicit(na_word(p,3),1,memory_order_relaxed);
  uint32_t scaled=(uint32_t)(fminf(peak,1000.0f)*1000000.0f);na_max(&p->peak,scaled);na_max(na_word(p,4),scaled);na_max(&p->available_max,available);
  atomic_fetch_add_explicit(&p->frames,take,memory_order_relaxed);atomic_fetch_add_explicit(&p->underrun_frames,n-take,memory_order_relaxed);atomic_fetch_add_explicit(&p->nonfinite,bad,memory_order_relaxed);atomic_fetch_add_explicit(&p->nonzero,nonzero,memory_order_relaxed);atomic_fetch_add_explicit(&p->calls,1,memory_order_relaxed);
 }
done:atomic_fetch_sub_explicit(&p->readers,1,memory_order_seq_cst);return take;
}
#endif
