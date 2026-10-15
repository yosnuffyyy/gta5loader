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
 puts("PASS: pipeline capacity pressure/hysteresis and bounded queue staging thresholds");
}
