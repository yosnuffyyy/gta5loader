#ifndef NATIVE_INTER_STAGE_LAYOUT_H
#define NATIVE_INTER_STAGE_LAYOUT_H
#include <stdint.h>
#include <stdbool.h>
/* Location numbers are register labels, not an obligation to leave holes.
   Keep low locations and move high ones into free locations on both sides.
   Preserve all outputs, including those not consumed by this fragment stage. */
typedef struct {uint32_t changed;uint8_t location[32];} InterStagePlan;
static bool inter_stage_plan(uint32_t vertex,uint32_t fragment,unsigned limit,InterStagePlan *p){
 if(!p||!limit||limit>32||(fragment&~vertex))return false;
 *p=(InterStagePlan){0};for(unsigned i=0;i<32;i++)p->location[i]=(uint8_t)i;
 uint32_t low=limit==32?UINT32_MAX:(1u<<limit)-1,used=(vertex|fragment)&low;
 for(unsigned i=limit;i<32;i++)if(vertex&(1u<<i)){
  unsigned target=0;while(target<limit&&(used&(1u<<target)))target++;if(target==limit)return false;
  used|=1u<<target;p->changed|=1u<<i;p->location[i]=(uint8_t)target;
 }
 return true;
}
#endif
