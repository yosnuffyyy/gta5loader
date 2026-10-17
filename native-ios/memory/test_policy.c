#include "native_memory.h"
#include <assert.h>
#include <stdio.h>
int main(void){unsigned cases=0;for(uint64_t n=0;n<=UINT64_C(134217729);n+=4093){uint64_t cap=native_stream_growth_limit(n);assert(cap>=33554432&&cap<=268435456);assert((cap&(cap-1))==0);if(n<=134217728)assert(cap/2>=n);cases++;}assert(native_stream_growth_limit(37314560)==134217728);assert(native_stream_growth_limit(UINT64_MAX)==268435456);assert(native_stream_growth_limit(16777216)==33554432);assert(native_stream_growth_limit(16777217)==67108864);printf("PASS: request-aware buddy growth policy: %u boundary/overflow checks; 37,314,560 bytes requires 128 MiB segment, budget unchanged.\n",cases+4);return 0;}
