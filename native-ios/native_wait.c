#include "native_wait.h"
#include "threading/native_pump.h"
#include "wasm-rt-impl.h"
#include "wasm-rt-exceptions.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

#if !WASM_RT_USE_MMAP
#error Shared engine memory must grow without moving its base address.
#endif

typedef struct Waiter {
  struct Waiter *next;
  wasm_rt_shared_memory_t *memory;
  uint64_t address;
  pthread_cond_t condition;
  bool notified;
  pthread_t owner;
  uint64_t pokes;
} Waiter;
typedef struct { pthread_mutex_t mutex; Waiter *head; } Bucket;
static Bucket buckets[128];
__attribute__((weak)) bool native_wait_cancelled(void){return false;}
__attribute__((weak)) void native_wait_service(void){}
typedef struct PollGuard {
  struct PollGuard *previous;
  wasm_rt_jmp_buf checkpoint;
  WASM_RT_UNWIND_TARGET *unwind;
} PollGuard;
static _Thread_local PollGuard *poll_guard;
void native_wait_trap_redirect(wasm_rt_trap_t code){
  if(poll_guard)WASM_RT_LONGJMP(poll_guard->checkpoint,code);
}
// Callouts may reenter the engine, notify this bucket, trap or throw. They run
// without bucket locks; a local checkpoint preserves the outer engine handler.
static wasm_rt_trap_t poll_services(bool *cancelled){
  PollGuard guard={.previous=poll_guard,.unwind=wasm_rt_get_unwind_target()};
  poll_guard=&guard;
  wasm_rt_trap_t trap=WASM_RT_SETJMP(guard.checkpoint);
  if(!trap){
    wasm_rt_set_unwind_target(&guard.checkpoint);
    *cancelled=native_wait_cancelled();
    if(!*cancelled)native_wait_service();
    if(!*cancelled)*cancelled=native_wait_cancelled();
  }
  wasm_rt_set_unwind_target(guard.unwind);poll_guard=guard.previous;
  return trap;
}
static void propagate(wasm_rt_trap_t trap,bool cancelled){
  if(trap==WASM_RT_TRAP_UNCAUGHT_EXCEPTION)wasm_rt_throw();
  if(trap)wasm_rt_trap(trap);
  if(cancelled)wasm_rt_trap(WASM_RT_TRAP_UNREACHABLE);
}
static pthread_once_t initialized=PTHREAD_ONCE_INIT;
static void checked(int status) { if(status) abort(); }
static void initialize(void) {
  for(unsigned i=0;i<128;i++) checked(pthread_mutex_init(&buckets[i].mutex,NULL));
}
static Bucket *bucket_for(wasm_rt_shared_memory_t *memory,uint64_t address) {
  checked(pthread_once(&initialized,initialize));
  return &buckets[(((uintptr_t)memory>>4)^(address>>2))&127];
}
static uint64_t now_ns(void) {
  struct timespec t;
  if(clock_gettime(CLOCK_MONOTONIC,&t)) abort();
  return (uint64_t)t.tv_sec*1000000000ull+(uint64_t)t.tv_nsec;
}
// Caller holds mem_lock. The mmap base is stable across shared memory growth.
static wasm_rt_trap_t invalid(wasm_rt_shared_memory_t *m,uint64_t a,unsigned width) {
  if(a&(width-1)) return WASM_RT_TRAP_UNALIGNED;
  if(a>m->size || width>m->size-a) return WASM_RT_TRAP_OOB;
  return WASM_RT_TRAP_NONE;
}
static uint32_t wait_value(wasm_rt_shared_memory_t *memory,uint64_t address,
                           uint64_t expected,int64_t timeout,unsigned width) {
  Bucket *bucket=bucket_for(memory,address);
  checked(pthread_mutex_lock(&bucket->mutex));
  checked(pthread_mutex_lock(&memory->mem_lock));
  wasm_rt_trap_t error=invalid(memory,address,width);
  uint64_t actual=0;
  if(!error) {
    if(width==4) actual=atomic_load_explicit((_Atomic uint32_t*)(memory->data+address),memory_order_seq_cst);
    else actual=atomic_load_explicit((_Atomic uint64_t*)(memory->data+address),memory_order_seq_cst);
  }
  checked(pthread_mutex_unlock(&memory->mem_lock));
  if(error || actual!=expected || timeout==0) {
    checked(pthread_mutex_unlock(&bucket->mutex));
    if(error) wasm_rt_trap(error);
    bool cancelled=false;wasm_rt_trap_t trap=poll_services(&cancelled);
    propagate(trap,cancelled);
    return actual!=expected ? 1 : 2;
  }
  Waiter waiter={.memory=memory,.address=address,.owner=pthread_self(),.next=bucket->head};
  pthread_condattr_t attr;
  checked(pthread_condattr_init(&attr));
#if !defined(__APPLE__)
  checked(pthread_condattr_setclock(&attr,CLOCK_MONOTONIC));
#endif
  checked(pthread_cond_init(&waiter.condition,&attr));
  checked(pthread_condattr_destroy(&attr));
  bucket->head=&waiter;
  const uint64_t start=now_ns();
  const uint64_t deadline=timeout<0 || (uint64_t)timeout>UINT64_MAX-start
    ?UINT64_MAX:start+(uint64_t)timeout;
  bool cancelled=false;
  wasm_rt_trap_t trap=WASM_RT_TRAP_NONE;
  while(!waiter.notified) {
    uint64_t pokes=waiter.pokes;
    checked(pthread_mutex_unlock(&bucket->mutex));
    trap=poll_services(&cancelled);
    checked(pthread_mutex_lock(&bucket->mutex));
    if(trap||cancelled||waiter.notified)break;
    // Delivery may race with the callout or reacquiring the bucket lock. Do
    // another service pass instead of sleeping through that delivery.
    if(waiter.pokes!=pokes)continue;
    int result;
    {
      uint64_t now=now_ns();
      if(timeout>=0&&now>=deadline) break;
      uint64_t wake=deadline;
      if(wake>now+100000000ull)wake=now+100000000ull;
#if defined(__APPLE__)
      uint64_t remaining=wake-now;
      struct timespec relative={(time_t)(remaining/1000000000ull),(long)(remaining%1000000000ull)};
      result=pthread_cond_timedwait_relative_np(&waiter.condition,&bucket->mutex,&relative);
#else
      struct timespec absolute={(time_t)(wake/1000000000ull),(long)(wake%1000000000ull)};
      result=pthread_cond_timedwait(&waiter.condition,&bucket->mutex,&absolute);
#endif
    }
    if(result!=0 && result!=ETIMEDOUT) abort();
    // A timed wait may race with notify; notification wins if it acquired
    // this mutex first. Spurious wakeups keep waiting until the deadline.
  }
  Waiter **link=&bucket->head;
  while(*link!=&waiter) link=&(*link)->next;
  *link=waiter.next;
  uint32_t result=waiter.notified?0:2;
  checked(pthread_cond_destroy(&waiter.condition));
  checked(pthread_mutex_unlock(&bucket->mutex));
  propagate(trap,cancelled);
  return result;
}
void native_wait_poke(wasm_rt_shared_memory_t *memory,pthread_t owner){
  checked(pthread_once(&initialized,initialize));
  for(unsigned i=0;i<128;i++){
    Bucket *b=&buckets[i];checked(pthread_mutex_lock(&b->mutex));
    for(Waiter *w=b->head;w;w=w->next)if(w->memory==memory&&pthread_equal(w->owner,owner)){
      w->pokes++;checked(pthread_cond_signal(&w->condition));
    }
    checked(pthread_mutex_unlock(&b->mutex));
  }
}
uint32_t native_wait32(wasm_rt_shared_memory_t *m,uint64_t a,uint32_t expected,int64_t timeout) {
  return wait_value(m,a,expected,timeout,4);
}
uint32_t native_wait64(wasm_rt_shared_memory_t *m,uint64_t a,uint64_t expected,int64_t timeout) {
  return wait_value(m,a,expected,timeout,8);
}
uint32_t native_notify(wasm_rt_shared_memory_t *m,uint64_t a,uint32_t count) {
  checked(pthread_mutex_lock(&m->mem_lock));
  wasm_rt_trap_t error=invalid(m,a,4);
  checked(pthread_mutex_unlock(&m->mem_lock));
  if(error) wasm_rt_trap(error);
  Bucket *bucket=bucket_for(m,a);
  checked(pthread_mutex_lock(&bucket->mutex));
  uint32_t woken=0;
  for(Waiter *w=bucket->head;w && woken<count;w=w->next) {
    if(w->memory==m && w->address==a && !w->notified) {
      w->notified=true;woken++;
      checked(pthread_cond_signal(&w->condition));
    }
  }
  checked(pthread_mutex_unlock(&bucket->mutex));
  return woken;
}
