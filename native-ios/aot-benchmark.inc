#include "gta-impl.h"
/* CLI-only controlled original forward-light producer, not a gameplay profile.
   Linked against the same generated partition at O1 or O2, never rewritten. */
static int aot_benchmark(void){
 wasm_rt_init();struct w2c_env *h=calloc(1,sizeof(*h));w2c_gta *e=calloc(1,sizeof(*e));if(!h||!e)return 1;host_init(h);h->engine=e;
 struct w2c_wasi__snapshot__preview1 wasi={.env=h};wasm_rt_allocate_memory_shared(&h->memory,8192,65536,false,65536);
 wasm_rt_trap_t trap=wasm_rt_impl_try();if(trap)return 1;wasm2c_gta_instantiate(e,h,&wasi);
 const uint64_t light=64*1048576;host_put32(h,23074832,0);
 for(unsigned i=0;i<9;i++){host_put32(h,light+i*1024+100,256);host_put64(h,light+i*1024+136,0x123400+i);if(i<8)host_put64(h,23076016+i*304+272,0x123400+i);}
 uint64_t sum=0,wall=0,cpu=0;struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);wall=(uint64_t)t.tv_sec*1000000000+t.tv_nsec;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);cpu=(uint64_t)t.tv_sec*1000000000+t.tv_nsec;
 for(unsigned i=0;i<200000;i++){
  unsigned slot=i&7;uint32_t actual=w2c_gta_f44528(e,light+slot*1024,0);if(actual!=slot)return 1;sum+=actual;
  if(w2c_gta_f44528(e,light+8*1024,0)!=UINT32_MAX)return 1;
 }
 clock_gettime(CLOCK_MONOTONIC,&t);wall=(uint64_t)t.tv_sec*1000000000+t.tv_nsec-wall;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);cpu=(uint64_t)t.tv_sec*1000000000+t.tv_nsec-cpu;
 printf("AOT_COMPILER_BENCH calls=400000 checksum=%llu wall_ms=%.6f cpu_ms=%.6f\n",(unsigned long long)sum,wall/1e6,cpu/1e6);
 wasm2c_gta_free(e);host_close(h);wasm_rt_free_memory_shared(&h->memory);pthread_mutex_destroy(&h->memory.mem_lock);free(e);free(h);wasm_rt_free_thread();return sum!=700000;
}
