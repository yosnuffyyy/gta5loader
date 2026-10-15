#include "shadow_guard.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){float values[]={0.0f,-0.0f,3.0f,1.0f,2.0f,-1.0f,INFINITY,-INFINITY,NAN,nextafterf(3,4),nextafterf(0,1)};unsigned cases=0;
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++)for(unsigned j=0;j<2;j++)for(unsigned arm=0;arm<2;arm++){ShadowCondition c={6,63,j?3:0,arm!=0};bool expected=arm?!(values[i]==c.equals):(values[i]==c.equals);assert(shadow_condition_runs(c,values[i])==expected);cases++;}
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++){ShadowCondition outer={6,63,0,true},inner={6,63,3,true};bool cube_runs=values[i]!=0&&values[i]!=3;assert((!shadow_condition_runs(outer,values[i])||!shadow_condition_runs(inner,values[i]))==!cube_runs);cases++;}
 printf("PASS: shadow guard %u IEEE f32 branch cases; zero/signed zero, cube/spot modes, adjacent floats, infinities and NaN. No texture data inferred.\n",cases);return 0;}
