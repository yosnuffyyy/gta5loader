#include "vertex_step.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 for(uint32_t first=0;first<7;first++)for(uint32_t rate=0;rate<9;rate++){
  uint64_t record=first;uint32_t counter=rate;
  for(uint32_t local=0;local<100;local++){
   assert(vertex_step_index(first,local,rate)==record);
   if(counter){counter--;if(!counter){record++;counter=rate;}}
  }
 }
 assert(vertex_step_index(3,3,2)==4); /* NOT floor((first+local)/rate). */
 assert(vertex_step_index(UINT32_MAX,UINT32_MAX,1)==UINT64_C(8589934590));
 assert(vertex_step_capacity(0,0)==0&&vertex_step_capacity(1,0)==UINT32_MAX);
 assert(vertex_step_capacity(2,2)==4&&vertex_step_capacity(UINT64_MAX,2)==UINT32_MAX);
 assert(vertex_step_capacity(UINT32_MAX,UINT32_MAX)==UINT32_MAX);
 uint32_t at=0,runs=0;while(at<6){uint32_t n=vertex_step_run(2,at,6-at),other=vertex_step_run(3,at,6-at);if(other<n)n=other;
  for(uint32_t i=0;i<n;i++){assert(vertex_step_index(1,at+i,2)==vertex_step_index(1,at,2));assert(vertex_step_index(1,at+i,3)==vertex_step_index(1,at,3));}at+=n;runs++;
 }assert(runs==4);assert(vertex_step_run(0,UINT32_MAX,7)==7);assert(vertex_step_run(UINT32_MAX,UINT32_MAX-1,9)==1);
 puts("PASS: vertex instance stepping: D3D counter semantics, rate0/1/N, nonzero start, mixed divisors, bounds and widened arithmetic.");return 0;
}
