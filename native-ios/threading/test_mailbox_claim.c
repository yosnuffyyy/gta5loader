#include "native_mailbox.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
typedef struct {_Atomic bool pending;_Atomic unsigned consumed;bool core;} Mail;
static void *sender(void *context){Mail *m=context;for(unsigned i=1;i<=20000;i++){while(atomic_load_explicit(&m->consumed,memory_order_acquire)!=i-1)sched_yield();atomic_store_explicit(&m->pending,true,memory_order_release);}return NULL;}
static uint64_t cpu(void){struct timespec t;assert(!clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t));return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"bench")){for(unsigned pair=0;pair<8;pair++)for(unsigned order=0;order<2;order++){bool core=(pair+order)&1;_Atomic bool pending=false;unsigned claims=0;uint64_t start=cpu();for(unsigned i=0;i<10000000;i++)claims+=native_mailbox_claim(&pending,core);uint64_t elapsed=cpu()-start;assert(!claims);printf("MAILBOX_BENCH pair=%u order=%u core=%u calls=10000000 cpu_ms=%.6f\n",pair,order,core,elapsed/1e6);}return 0;}
 for(unsigned core=0;core<2;core++){
  Mail m={.core=core};pthread_t thread;assert(!pthread_create(&thread,NULL,sender,&m));
  for(unsigned i=1;i<=20000;i++){while(!native_mailbox_claim(&m.pending,m.core))sched_yield();atomic_store_explicit(&m.consumed,i,memory_order_release);}
  assert(!pthread_join(thread,NULL));assert(!native_mailbox_claim(&m.pending,m.core));assert(atomic_load(&m.consumed)==20000);
 }
 puts("PASS: original/V2 mailbox claims: 40000 cross-thread publication/consumption handoffs, no lost notification or duplicate claim.");
}
