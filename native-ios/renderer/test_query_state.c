#include "query_state.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 QueryState q={.type=1};assert(!query_end_state(&q,1));assert(!query_reserve_state(&q));assert(query_begin_state(&q,1));assert(!query_begin_state(&q,2));
 assert(query_reserve_state(&q));assert(query_reserve_state(&q));assert(query_accept_state(&q,1,128)==1);assert(!query_ready_state(&q));assert(query_end_state(&q,9));assert(!query_ready_state(&q));assert(query_accept_state(&q,1,256)==1);assert(query_ready_state(&q));assert(query_value_state(&q)==384);q.published=true;assert(!query_ready_state(&q));assert(query_accept_state(&q,1,7)==-1);
 assert(query_begin_state(&q,2));assert(query_accept_state(&q,1,65536)==0);assert(query_end_state(&q,0));assert(query_ready_state(&q));assert(query_value_state(&q)==0);
 q.type=5;assert(query_begin_state(&q,3));assert(query_reserve_state(&q));assert(query_end_state(&q,10));assert(query_accept_state(&q,3,UINT64_C(0x100000001))==1);assert(query_value_state(&q)==1);
 q.type=1;assert(query_begin_state(&q,4));assert(query_reserve_state(&q));assert(query_reserve_state(&q));assert(query_accept_state(&q,4,UINT64_MAX)==1);assert(query_accept_state(&q,4,1)==-1);
 for(unsigned type=0;type<17;type++){QueryState bad={.type=type};assert(query_begin_state(&bad,123)==(type==1||type==5));}
 for(unsigned i=0;i<10000;i++){QueryState a={.type=1};assert(query_begin_state(&a,i+10));assert(query_reserve_state(&a));assert(query_end_state(&a,i+1));assert(query_begin_state(&a,i+10010));assert(query_accept_state(&a,i+10,123)==0);assert(query_reserve_state(&a));assert(query_end_state(&a,i+2));assert(query_accept_state(&a,i+10010,(uint64_t)i<<32)==1);assert(query_value_state(&a)==((uint64_t)i<<32));assert(query_ready_state(&a));}
 puts("PASS: query scope/order, exact 64-bit count and BOOL conversion, pending completion, empty scopes, sequence zero, overflow and 10000 reuse/stale/ABA-token cases.");return 0;
}
