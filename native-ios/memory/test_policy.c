#include "native_memory.h"
#include "available_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){unsigned cases=0;for(uint64_t n=0;n<=UINT64_C(134217729);n+=4093){uint64_t cap=native_stream_growth_limit(n,0);assert(cap>=33554432&&cap<=268435456);assert((cap&(cap-1))==0);if(n<=134217728)assert(cap/2>=n);cases++;}assert(native_stream_growth_limit(37314560,0)==134217728);assert(native_stream_growth_limit(UINT64_MAX,0)==268435456);assert(native_stream_growth_limit(16777216,0)==33554432);assert(native_stream_growth_limit(16777217,0)==67108864);for(unsigned slots=0;slots<=33;slots++){uint64_t cap=native_stream_growth_limit(1,slots);assert(cap==(slots<16?33554432:slots<24?67108864:slots<28?134217728:268435456));cases++;}assert(native_stream_available_bytes(33546240,0,536870912,1)==536870912);
assert(native_stream_available_bytes(134209536,67108864,16777216,1)==67100672);
assert(native_stream_available_bytes(134209536,67108864,536870912,32)==67100672);
assert(native_stream_available_bytes(33546240,0,UINT64_MAX,31)==301973504);
assert(native_stream_available_bytes(0,0,UINT64_MAX,0)==UINT64_C(4294967296));
assert(native_stream_available_bytes(0,1,536870912,0)==0);
assert(native_stream_available_bytes(UINT64_MAX,0,UINT64_MAX,0)==0);
for(unsigned i=0;i<40000;i++){uint64_t heap=(uint64_t)i*100003,used=i%3?heap/2:heap,target=i%2?UINT64_MAX:536870912;uint32_t slots=i%34;uint64_t n=native_stream_available_bytes(heap,used,target,slots);assert(n<=UINT64_C(4294967296)-used);if(slots>=32||heap>=target)assert(n==heap-used);cases++;}
puts("PASS: bounded streamer preflight headroom: large loads before growth, over-target saturation, full slots, sentinel rounding, 4 GiB ceiling and 40,000 accounting cases; no allocation or OS-memory promise.");
printf("PASS: request-aware buddy growth policy: %u boundary/overflow checks; bounded segment-growth envelope; growth target unchanged.\n",cases+4);return 0;}
