#include "shadow_guard.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){float values[]={0.0f,-0.0f,3.0f,1.0f,2.0f,-1.0f,INFINITY,-INFINITY,NAN,nextafterf(3,4),nextafterf(0,1)};unsigned cases=0;
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++)for(unsigned j=0;j<2;j++)for(unsigned arm=0;arm<2;arm++){ShadowCondition c={6,63,j?3:0,arm!=0,SHADOW_FLOAT_NE};bool expected=arm?!(values[i]==c.equals):(values[i]==c.equals);assert(shadow_condition_runs(c,values[i])==expected);cases++;}
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++){ShadowCondition outer={6,63,0,true,SHADOW_FLOAT_NE},inner={6,63,3,true,SHADOW_FLOAT_NE};bool cube_runs=values[i]!=0&&values[i]!=3;assert((!shadow_condition_runs(outer,values[i])||!shadow_condition_runs(inner,values[i]))==!cube_runs);cases++;}
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++)for(unsigned j=0;j<2;j++)for(unsigned arm=0;arm<2;arm++){ShadowCondition c={6,63,j?3:0,arm!=0,SHADOW_FLOAT_NE};uint32_t bits;memcpy(&bits,values+i,4);bool equal=j?bits==0x40400000u:(bits&0x7fffffffu)==0;assert(shadow_proven_inactive(c,bits)==(arm&&equal));cases++;}
 const uint32_t edge_bits[]={1,0x80000001u,0x007fffffu,0x807fffffu,0x7f800000u,0xff800000u};for(unsigned i=0;i<6;i++)for(unsigned j=0;j<2;j++){ShadowCondition c={6,63,j?3:0,true,SHADOW_FLOAT_NE};assert(!shadow_proven_inactive(c,edge_bits[i]));cases++;}
 printf("PASS: shadow guard %u IEEE f32 branch cases; zero/signed zero, cube/spot modes, adjacent floats, infinities and NaN. No texture data inferred.\n",cases);
 const int32_t counts[]={INT32_MIN,-1,0,1,2,3,4,5,6,7,8,INT32_MAX};unsigned integerCases=0;
 for(unsigned i=0;i<12;i++)for(unsigned n=0;n<8;n++)for(unsigned pred=SHADOW_INT_LE;pred<=SHADOW_INT_GT;pred++)for(unsigned arm=0;arm<2;arm++){ShadowCondition c={3,8,(float)n,arm!=0,pred};uint32_t bits;memcpy(&bits,counts+i,4);bool low_arm=(pred==SHADOW_INT_LE&&!arm)||(pred==SHADOW_INT_GT&&arm);bool proven=low_arm&&counts[i]>=0&&(uint32_t)counts[i]<=n;assert(shadow_proven_inactive(c,bits)==proven);integerCases++;}
 printf("PASS: %u signed light-count branch proofs, 0..7 thresholds, both lexical arms, INT32 boundaries; only low-count reachability certified, negative/unknown/high counts rejected; robust to subnormal flushing.\n",integerCases);return 0;}
