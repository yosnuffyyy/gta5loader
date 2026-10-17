#include "drain_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 unsigned checks=0;const uint64_t times[]={0,1,3999999,4000000,4000001,UINT64_MAX};
 for(unsigned n=0;n<12;n++)for(unsigned t=0;t<6;t++)for(unsigned flags=0;flags<4;flags++){
  bool ready=(flags&1)!=0,full=(flags&2)!=0;
  bool expected=n>=1&&n<=3&&t<=2&&full&&ready;
  assert(native_drain_keep(n,times[t],full,ready)==expected);checks++;
 }
 assert(!native_drain_keep(UINT32_MAX,0,true,true));checks++;
 printf("PASS: bounded ready-drain policy: %u controlled-clock cases; full/partial budgets, producer idle, four-drain / 4 ms bounds and overflow; no producer wait.\n",checks);
 return 0;
}
