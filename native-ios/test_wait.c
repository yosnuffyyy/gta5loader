#include "wait.h"
#include "native_wait.h"
#include "wasm-rt-impl.h"
#include "wasm-rt-exceptions.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>
#include <unistd.h>
#include <stdlib.h>

struct w2c_env { wasm_rt_shared_memory_t memory; };
wasm_rt_shared_memory_t *w2c_env_memory(struct w2c_env *env) { return &env->memory; }
typedef struct { struct w2c_env *env; uint32_t address; int wide; _Atomic uint32_t result; } Task;
static void *run_wait(void *arg) {
  Task *t=arg;
  wasm_rt_init_thread();
  w2c_wait instance;
  wasm2c_wait_instantiate(&instance,t->env);
  uint32_t result=t->wide?w2c_wait_wait64(&instance,t->address,0,2000000000ll):w2c_wait_wait32(&instance,t->address,0,2000000000ll);
  atomic_store(&t->result,result);
  wasm2c_wait_free(&instance);
  wasm_rt_free_thread();
  return NULL;
}
static unsigned notify_until(w2c_wait *i,uint32_t address,unsigned count) {
  unsigned result=0;
  for(unsigned n=0;n<2000 && result<count;n++) { result+=w2c_wait_notify(i,address,count-result);usleep(1000); }
  return result;
}
int main(void) {
  wasm_rt_init();
  struct w2c_env env={0},other={0};
  wasm_rt_allocate_memory_shared(&env.memory,1,4,false,65536);
  wasm_rt_allocate_memory_shared(&other.memory,1,4,false,65536);
  w2c_wait instance;wasm2c_wait_instantiate(&instance,&env);
  assert(w2c_wait_wait32(&instance,0,1,0)==1);
  assert(w2c_wait_wait64(&instance,0,0,0)==2);
  assert(w2c_wait_wait32(&instance,0,0,1000000)==2);
  assert(w2c_wait_offsetWait(&instance,0,1,0)==1);
  assert(w2c_wait_notify(&instance,0,100)==0);
  assert(w2c_wait_notify(&instance,0,0)==0);
  wasm_rt_trap_t trap=wasm_rt_impl_try();
  if(!trap){w2c_wait_wait32(&instance,1,0,0);abort();}
  assert(trap==WASM_RT_TRAP_UNALIGNED);
  trap=wasm_rt_impl_try();
  if(!trap){w2c_wait_notify(&instance,65536,1);abort();}
  assert(trap==WASM_RT_TRAP_OOB);
  for(unsigned round=0;round<25;round++) {
    Task tasks[4];pthread_t threads[4];
    for(unsigned n=0;n<4;n++) {
      tasks[n]=(Task){.env=n==3?&other:&env,.address=n==2?8:0,.wide=n==1,.result=99};
      assert(pthread_create(&threads[n],NULL,run_wait,&tasks[n])==0);
    }
    assert(notify_until(&instance,0,1)==1);
    assert(notify_until(&instance,0,1)==1);
    assert(notify_until(&instance,8,1)==1);
    w2c_wait isolated;wasm2c_wait_instantiate(&isolated,&other);
    assert(notify_until(&isolated,0,1)==1);
    for(unsigned n=0;n<4;n++){assert(pthread_join(threads[n],NULL)==0);assert(atomic_load(&tasks[n].result)==0);}
    wasm2c_wait_free(&isolated);
  }
  Task growth={.env=&env,.address=0,.result=99};pthread_t thread;
  assert(pthread_create(&thread,NULL,run_wait,&growth)==0);
  assert(w2c_wait_grow(&instance,1)==1);
  assert(notify_until(&instance,0,1)==1);assert(pthread_join(thread,NULL)==0);assert(atomic_load(&growth.result)==0);
  assert(w2c_wait_wait32(&instance,65536,0,0)==2);
  wasm2c_wait_free(&instance);
  wasm_rt_free_memory_shared(&env.memory);wasm_rt_free_memory_shared(&other.memory);wasm_rt_free();
  puts("PASS: compiled wait/notify, timeout, mismatch, alignment, bounds, mixed widths, address/memory isolation, concurrent waiters and growth.");
}
