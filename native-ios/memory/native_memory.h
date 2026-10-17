#ifndef NATIVE_MEMORY_H
#define NATIVE_MEMORY_H
#include <stdint.h>
/* The original buddy segment loses one 8 KiB leaf to its sentinel. Its largest
   initial block is therefore half the segment, not the segment size. Keep the
   32 MiB default for small requests and the original 256 MiB upper bound. This
   selects a segment granularity; it does not change the allocator budget. */
static inline uint64_t native_stream_growth_limit(uint64_t request,uint32_t segments){
 uint64_t block=UINT64_C(16777216);
 while(block<request&&block<UINT64_C(134217728))block*=2;
 uint64_t cap=block*2,reserve=33554432;uint32_t remaining=segments<32?32-segments:1;
 /* Keep a useful capacity reserve as the fixed 32-slot pool fills. Doubling
    at half/quarter/eighth free slots is bounded by the original 256 MiB cap. */
 for(;remaining<=16&&reserve<UINT64_C(268435456);remaining*=2)reserve*=2;
 return cap>reserve?cap:reserve;
}
struct w2c_gta;struct w2c_env;
void native_memory_summary(struct w2c_env*);
uint64_t native_stream_default_limit(struct w2c_gta*,uint64_t);
uint32_t native_stream_grow(struct w2c_gta*,uint64_t,uint64_t);
uint64_t native_stream_growth_context(struct w2c_gta*,uint64_t,uint64_t);
void native_stream_allocation_result(struct w2c_gta*,uint64_t,uint64_t,uint64_t);
#ifdef NATIVE_PROBE_CLI
uint32_t native_stream_grow_legacy(struct w2c_gta*,uint64_t);
uint64_t native_stream_try_legacy(struct w2c_gta*,uint64_t,uint64_t,uint64_t,uint32_t);
#endif
#endif
