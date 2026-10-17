#ifndef NATIVE_MEMORY_H
#define NATIVE_MEMORY_H
#include <stdint.h>
/* The original buddy segment loses one 8 KiB leaf to its sentinel. Its largest
   initial block is therefore half the segment, not the segment size. Keep the
   32 MiB default for small requests and the original 256 MiB upper bound. This
   selects a segment granularity; it does not change the allocator budget. */
static inline uint64_t native_stream_growth_limit(uint64_t request){
 uint64_t block=UINT64_C(16777216);
 while(block<request&&block<UINT64_C(134217728))block*=2;
 return block*2;
}
struct w2c_gta;
uint32_t native_stream_grow(struct w2c_gta*,uint64_t,uint64_t);
uint64_t native_stream_growth_context(struct w2c_gta*,uint64_t,uint64_t);
void native_stream_allocation_result(struct w2c_gta*,uint64_t,uint64_t,uint64_t);
#ifdef NATIVE_PROBE_CLI
uint64_t native_stream_try_legacy(struct w2c_gta*,uint64_t,uint64_t,uint64_t,uint32_t);
#endif
#endif
