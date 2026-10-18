#include "texture_copy.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(tc_cast(28,29)&&tc_cast(41,42)&&tc_cast(17,71)&&tc_cast(3,77)&&tc_cast(3,98));
 assert(!tc_cast(28,87)&&!tc_cast(42,71)&&!tc_cast(17,77)&&!tc_cast(71,80));
 CopyShape s={2,2,1,1,1,8},d={8,8,1,4,4,8};uint32_t zero[3]={0},end[3]={2,2,1};CopyPlan p;
 assert(!tc_plan(s,d,zero,zero,end,&p)&&p.sw==2&&p.dw==8&&p.rows==2&&p.pitch==256);
 uint32_t bcend[]={8,8,1};assert(!tc_plan(d,s,zero,zero,bcend,&p)&&p.sw==8&&p.dw==2);
 d.width=d.height=1;s.width=s.height=1;end[0]=end[1]=1;assert(!tc_plan(s,d,zero,zero,end,&p)&&p.dw==4);
 uint32_t bad[]={1,0,0};assert(tc_plan(s,d,zero,bad,end,&p));
 d.width=d.height=8;s.width=s.height=3;end[0]=end[1]=3;assert(tc_plan(s,d,zero,zero,end,&p));
 end[0]=4;assert(tc_plan(s,d,zero,zero,end,&p));
 end[0]=0;end[1]=0;assert(!tc_plan(s,d,zero,zero,end,&p)&&!p.blocks);
 s=(CopyShape){16384,16384,1,1,1,16};d=s;end[0]=end[1]=16384;assert(!tc_plan(s,d,zero,zero,end,&p)&&p.pitch==262144&&p.tile_rows==4);
 puts("PASS: texture-copy storage families, BC block scaling, small mips, empty/invalid boxes and 1 MiB tile bounds");return 0;
}
