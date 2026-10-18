#include "native_pcm.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>
static _Alignas(8) _Atomic unsigned char memory[64+16384*8];static NativePCM p;
static void put(unsigned at,float f){unsigned char b[4];memcpy(b,&f,4);for(unsigned i=0;i<4;i++)atomic_store_explicit(memory+at+i,b[i],memory_order_relaxed);}
static void *producer(void *unused){(void)unused;for(unsigned k=0;k<20000;k++){uint32_t w=atomic_load(na_word(&p,0));while(w-atomic_load_explicit(na_word(&p,1),memory_order_acquire)>=p.capacity)sched_yield();unsigned at=64+(w&(p.capacity-1))*8;put(at,(float)k/32768);put(at+4,-(float)k/32768);atomic_store_explicit(na_word(&p,0),w+1,memory_order_release);}return NULL;}
int main(void){
 _Static_assert(ATOMIC_LLONG_LOCK_FREE==2&&ATOMIC_INT_LOCK_FREE==2,"Realtime counters must be lock-free");
 atomic_store((_Atomic uint32_t*)(memory+8),16384);
 assert(!na_attach(&p,memory,sizeof(memory)-1,0,16384));assert(!na_attach(&p,memory,sizeof(memory),1,16384));assert(!na_attach(&p,memory,sizeof(memory),0,16383));assert(na_attach(&p,memory,sizeof(memory),0,16384));
 float l[512],r[512];atomic_store(&p.enabled,true);assert(!na_read(&p,l,r,512));for(unsigned i=0;i<512;i++)assert(l[i]==0&&r[i]==0);
 uint32_t read=UINT32_MAX-2;atomic_store(na_word(&p,0),read);atomic_store(na_word(&p,1),read);
 for(unsigned i=0;i<5;i++){unsigned at=64+((read+i)&16383)*8;put(at,.25f*(i+1));put(at+4,-.125f*(i+1));}atomic_store(na_word(&p,0),read+5);
 assert(na_read(&p,l,r,8)==5);for(unsigned i=0;i<5;i++){assert(l[i]==.25f*(i+1));assert(r[i]==-.125f*(i+1));}for(unsigned i=5;i<8;i++)assert(l[i]==0&&r[i]==0);assert(atomic_load(na_word(&p,1))==read+5);
 read=atomic_load(na_word(&p,1));atomic_store(na_word(&p,0),read+16385);assert(!na_read(&p,l,r,2));assert(atomic_load(na_word(&p,1))==read&&atomic_load(&p.invalid)==1);atomic_store(na_word(&p,0),read);
 atomic_store(&p.enabled,false);assert(!na_read(&p,l,r,8));assert(atomic_load(na_word(&p,1))==read);assert(!atomic_load(&p.readers));
 atomic_store(na_word(&p,0),UINT32_MAX-500);atomic_store(na_word(&p,1),UINT32_MAX-500);atomic_store(&p.enabled,true);
 pthread_t t;assert(!pthread_create(&t,NULL,producer,NULL));unsigned total=0;
 while(total<20000){unsigned take=na_read(&p,l,r,127);for(unsigned i=0;i<take;i++,total++){assert(l[i]==(float)total/32768);assert(r[i]==-(float)total/32768);}if(!take)sched_yield();}assert(!pthread_join(t,NULL));
 atomic_store(&p.enabled,false);assert(!atomic_load(&p.readers));p.base=NULL;
 assert(!na_read(&p,l,r,32));
 printf("PASS: original stereo ring contract, full extent, invalid capacity/occupancy, wrap, L/R bytes, partial underrun, disabled callback, detach, 20000 concurrent ordered PCM frames. No game bank or speaker claim. Context %zu bytes.\n",sizeof(p));
}
