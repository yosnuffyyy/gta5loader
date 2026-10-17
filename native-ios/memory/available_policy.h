#ifndef NATIVE_AVAILABLE_POLICY_H
#define NATIVE_AVAILABLE_POLICY_H
#include <stdint.h>
/* Logical streamer preflight capacity, never a committed/free-OS-memory claim.
   Keep actual retained free and bound any growable part by segment slots and
   the absolute native linear address limit. Arithmetic cannot wrap. */
static inline uint64_t native_stream_available_bytes(uint64_t heap,uint64_t used,uint64_t target,uint32_t segments){
 const uint64_t limit=UINT64_C(4294967296);if(heap>limit||used>heap)return 0;uint64_t free_bytes=heap-used;
 if(segments>=32||target<16777216||heap>=target)return free_bytes;
 uint64_t segment=(target<268435456?target:268435456)&~UINT64_C(8191),potential=(32-segments)*(segment-8192),headroom=target-heap;
 if(headroom>potential)headroom=potential;if(headroom>limit-heap)headroom=limit-heap;return free_bytes+headroom;
}
#endif
