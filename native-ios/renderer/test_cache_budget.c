#include <assert.h>
#include <stdio.h>
#include "cache_budget.h"
int main(void){
 const uint64_t mb=1024*1024;
 assert(pipeline_budget(0,600*mb)==256);
 assert(pipeline_budget(256,255*mb)==128);
 assert(pipeline_budget(128,300*mb)==128);
 assert(pipeline_budget(128,511*mb)==128);
 assert(pipeline_budget(128,512*mb)==256);
 assert(pipeline_budget(256,256*mb)==256);
 assert(!uniform_pressure(15,4*mb-1));
 assert(uniform_pressure(16,0));assert(uniform_pressure(1,4*mb));
 assert(!uniform_would_overflow(4*mb-512*1024,512*1024));
 assert(uniform_would_overflow(4*mb-512*1024+1,512*1024));
 assert(uniform_would_overflow(UINT64_MAX,1));
 assert(uniform_would_overflow(0,UINT64_MAX));
 puts("PASS: pipeline capacity pressure/hysteresis and bounded queue staging thresholds");
}
