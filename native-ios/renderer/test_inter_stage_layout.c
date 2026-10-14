#include "inter_stage_layout.h"
#include <assert.h>
#include <stdio.h>
static unsigned bits(uint32_t v){unsigned n=0;while(v){n+=v&1;v>>=1;}return n;}
int main(void){InterStagePlan p;
 assert(inter_stage_plan(0x807f,0x7f,15,&p)&&p.location[15]==7&&p.changed==(1u<<15));
 assert(inter_stage_plan(0x8006,0x8006,15,&p)&&p.location[15]==0);
 assert(inter_stage_plan(0x8006,6,15,&p)&&p.location[15]==0);
 assert(inter_stage_plan(UINT32_MAX,UINT32_MAX,32,&p)&&!p.changed);
 assert(!inter_stage_plan(0xffff,0x7f,15,&p));assert(!inter_stage_plan(1,2,15,&p));assert(!inter_stage_plan(1,1,0,&p));assert(!inter_stage_plan(1,1,33,&p));assert(!inter_stage_plan(1,1,15,NULL));
 uint32_t state=17;unsigned valid=0;
 for(unsigned t=0;t<20000;t++){state=state*1664525u+1013904223u;uint32_t v=state;state=state*1664525u+1013904223u;uint32_t f=v&state;unsigned limit=1+t%32;bool ok=inter_stage_plan(v,f,limit,&p);assert(ok==(bits(v)<=limit));if(!ok)continue;
  uint32_t used=0;for(unsigned i=0;i<32;i++)if(v&(1u<<i)){assert(p.location[i]<limit);uint32_t bit=1u<<p.location[i];assert(!(used&bit));used|=bit;if(i<limit)assert(p.location[i]==i);}assert(bits(used)==bits(v));valid++;
 }
 printf("PASS: inter-stage location plan: exact failing/consumed/unused location15 cases, full 32-slot boundary, impossible/missing interfaces rejected, 20000 randomized masks (%u fitting); collision-free and value-preserving.\n",valid);return 0;}
