#include "shadow_trace.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 ShadowTrace t={0};ShadowTraceEntry e={.binding=46,.srv=709,.source=708,.conditions=3,.inactive=1,.srv_generation=2,.source_generation=1,.source_version=2,.first_command=10,.last_command=10};
 memcpy(e.hash,"996bb42f448956f2",17);e.uniform[0]=(ShadowTraceUniform){1,0,0x40000000,3};e.uniform[1]=e.uniform[0];e.uniform[2]=(ShadowTraceUniform){2,0,0,4};
 assert(shadow_trace_push(&t,&e)&&t.count==1);e.last_command=20;assert(shadow_trace_push(&t,&e)&&t.count==1&&t.entries[0].repeats==1&&t.entries[0].first_command==10&&t.entries[0].last_command==20);
 e.inactive=0;e.uniform[2].bits=1;assert(shadow_trace_push(&t,&e)&&t.count==2);
 e.uniform[0].bits=e.uniform[1].bits=0x7fc00000;assert(shadow_trace_push(&t,&e)&&t.count==3);
 e.uniform[0].first=2;assert(shadow_trace_push(&t,&e)&&t.count==4);
 e.uniform[0].generation++;assert(shadow_trace_push(&t,&e)&&t.count==5);
 e.source_generation++;assert(shadow_trace_push(&t,&e)&&t.count==6);
 e.srv_generation++;assert(shadow_trace_push(&t,&e)&&t.count==7);
 e.source_version++;assert(shadow_trace_push(&t,&e)&&t.count==8);
 for(unsigned i=0;i<40;i++){e.last_command=100+i;e.uniform[2].bits=i;assert(shadow_trace_push(&t,&e));}
 assert(t.count==16&&t.overwritten==32);for(unsigned i=0;i<16;i++)assert(t.entries[(t.next+i)%16].last_command==124+i);
 e.conditions=SHADOW_CONDITIONS+1;assert(!shadow_trace_push(&t,&e)&&t.count==16&&t.overwritten==32);
 printf("PASS: bounded shadow transitions: active/inactive/ambiguous raw states, CB offsets/generations, source/view reuse, coalescing and 32 overwrites; storage %zu bytes. No rendering decisions depend on this trace.\n",sizeof(t));return 0;
}
