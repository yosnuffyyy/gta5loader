#include "gpu_ring.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
typedef struct {GpuRing ring;_Atomic uint64_t size;_Alignas(8) uint8_t mem[1024];unsigned received;_Atomic bool bad;} Test;
static _Atomic uint32_t *w(Test *t,unsigned i){return (_Atomic uint32_t*)(t->mem+4*i);}
static bool run(void *arg,uint32_t op,const uint8_t *p,uint32_t n){Test *t=arg;if(op!=10||n!=2||gpu_word(p,0)!=t->received||gpu_word(p,1)!=(t->received^0xabcdef01)){atomic_store(&t->bad,true);return false;}t->received++;return true;}
static void init(Test *t){memset(t,0,sizeof(*t));atomic_init(&t->size,sizeof(t->mem));atomic_store(w(t,0),64);assert(gpu_ring_init(&t->ring,t->mem,&t->size,0,sizeof(t->mem),t,run,NULL));}
static void cmd(Test *t,unsigned at,unsigned value){uint32_t a[]={10,2,value,value^0xabcdef01};memcpy(t->mem+at,a,sizeof(a));}
static void *consume(void *arg){Test *t=arg;while(t->received<100000&&!atomic_load(&t->bad)){if(gpu_ring_drain(&t->ring,64)<0)break;sched_yield();}return NULL;}
int main(void){
 Test t;init(&t);cmd(&t,64,0);atomic_store(w(&t,0),80);assert(gpu_ring_drain(&t.ring,8)==1&&t.received==1&&atomic_load(w(&t,1))==80);
 init(&t);cmd(&t,64,0);atomic_store(w(&t,0),76);assert(gpu_ring_drain(&t.ring,8)==-1&&t.received==0&&atomic_load(w(&t,1))==64&&atomic_load(w(&t,2))==2);
 init(&t);uint32_t bad[]={999,0};memcpy(t.mem+64,bad,8);atomic_store(w(&t,0),72);assert(gpu_ring_drain(&t.ring,8)==-1&&t.received==0);
 init(&t);bad[0]=10;bad[1]=UINT32_MAX;memcpy(t.mem+64,bad,8);atomic_store(w(&t,0),72);assert(gpu_ring_drain(&t.ring,8)==-1);
 init(&t);atomic_store(w(&t,0),65);assert(gpu_ring_drain(&t.ring,8)==-1);
 init(&t);assert(!gpu_ring_init(&t.ring,t.mem,&t.size,4,1024,&t,run,NULL));assert(!gpu_ring_init(&t.ring,t.mem,&t.size,1,128,&t,run,NULL));
 init(&t);t.ring.tail=1000;atomic_store(w(&t,1),1000);uint32_t wrap[]={1,0};memcpy(t.mem+1000,wrap,8);cmd(&t,64,0);atomic_store(w(&t,0),80);assert(gpu_ring_drain(&t.ring,4)==1&&t.received==1&&t.ring.tail==80);
 init(&t);pthread_t thread;assert(!pthread_create(&thread,NULL,consume,&t));unsigned head=64;
 for(unsigned i=0;i<100000;i++){
  unsigned next=head+16;bool wrap=next>1008;
  if(wrap){while(atomic_load_explicit(w(&t,1),memory_order_acquire)!=head&&!atomic_load(&t.bad))sched_yield();uint32_t a[]={1,0};memcpy(t.mem+head,a,8);head=64;next=80;}
  while(next>=atomic_load_explicit(w(&t,1),memory_order_acquire)&&head<atomic_load_explicit(w(&t,1),memory_order_acquire)&&!atomic_load(&t.bad))sched_yield();
  cmd(&t,head,i);head=next;atomic_store_explicit(w(&t,0),head,memory_order_release);
 }
 assert(!pthread_join(thread,NULL));assert(!atomic_load(&t.bad)&&t.received==100000);
 puts("PASS: GPU ring publication, 100,000 concurrent commands, wrap, bounds, unknown opcode and truncated-payload rejection.");return 0;
}
