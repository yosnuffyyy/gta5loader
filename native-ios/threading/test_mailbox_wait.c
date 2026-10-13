#include "native_wait.h"
#include "native_pump.h"
#include "wasm-rt-impl.h"
#include "wasm-rt-exceptions.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
static wasm_rt_shared_memory_t memory;
static _Thread_local unsigned mode,calls;
static _Atomic bool cancelled,pending,processed,entered;
static _Atomic unsigned result;
bool native_wait_cancelled(void){return atomic_load(&cancelled);}
void native_wait_service(void){
 calls++;
 if(mode==1)native_notify(&memory,32,1); // Must not deadlock on the wait bucket.
 if(mode==2)wasm_rt_trap(WASM_RT_TRAP_OOB);
 if(mode==3)wasm_rt_throw();
 if(mode==4){mode=2;native_wait32(&memory,544,0,-1);abort();} // Same bucket; nested trap cleanup.
 if(mode==5){atomic_store(&entered,true);if(atomic_exchange(&pending,false))atomic_store(&processed,true);}
}
static void *poked_wait(void *unused){
 (void)unused;wasm_rt_init_thread();mode=5;
 atomic_store(&result,native_wait32(&memory,32,0,-1));
 wasm_rt_free_thread();return NULL;
}
static void test_trap(unsigned test,unsigned expected,int64_t timeout){
 mode=test;wasm_rt_trap_t trap=wasm_rt_impl_try();
 if(!trap){native_wait32(&memory,32,0,timeout);abort();}
 assert((unsigned)trap==expected);mode=0;
 assert(native_notify(&memory,32,1)==0&&native_notify(&memory,544,1)==0);
 assert(native_wait32(&memory,32,0,0)==2);
}
int main(int argc,char **argv){
 wasm_rt_init();wasm_rt_allocate_memory_shared(&memory,1,1,false,65536);
 mode=1;calls=0;unsigned value=native_wait32(&memory,32,0,250000000);
 if(argc==2&&!strcmp(argv[1],"baseline")){
  assert(value==2&&calls==0);wasm_rt_free_memory_shared(&memory);pthread_mutex_destroy(&memory.mem_lock);wasm_rt_free();puts("EXPECTED BASELINE FAILURE: 0.8 wait timed out without delivering its main-thread mailbox callback.");return 2;
 }
 assert(value==0&&calls>0);mode=0;
 test_trap(2,WASM_RT_TRAP_OOB,-1);
 test_trap(3,WASM_RT_TRAP_UNCAUGHT_EXCEPTION,-1);
 test_trap(4,WASM_RT_TRAP_OOB,-1);
 test_trap(2,WASM_RT_TRAP_OOB,0);
 // Cancellation must also interrupt timeout-zero and mismatch spin loops.
 atomic_store(&cancelled,true);
 wasm_rt_trap_t trap=wasm_rt_impl_try();
 if(!trap){native_wait32(&memory,32,0,0);abort();}
 assert(trap==WASM_RT_TRAP_UNREACHABLE);
 trap=wasm_rt_impl_try();
 if(!trap){native_wait32(&memory,32,123,0);abort();}
 assert(trap==WASM_RT_TRAP_UNREACHABLE);atomic_store(&cancelled,false);
#ifndef NATIVE_WAIT_BASELINE
 for(unsigned pass=0;pass<50;pass++){
  atomic_store(&pending,false);atomic_store(&processed,false);atomic_store(&entered,false);atomic_store(&result,99);
  pthread_t thread;assert(!pthread_create(&thread,NULL,poked_wait,NULL));
  for(unsigned i=0;i<2000&&!atomic_load(&entered);i++)usleep(1000);
  assert(atomic_load(&entered));atomic_store(&pending,true);native_wait_poke(&memory,thread);
  for(unsigned i=0;i<2000&&!atomic_load(&processed);i++)usleep(1000);
  assert(atomic_load(&processed)&&atomic_load(&result)==99);
  assert(native_notify(&memory,32,1)==1);assert(!pthread_join(thread,NULL));assert(atomic_load(&result)==0);
 }
#endif
 assert(native_notify(&memory,32,1)==0);wasm_rt_free_memory_shared(&memory);pthread_mutex_destroy(&memory.mem_lock);wasm_rt_free();
 puts("PASS: mailbox delivery during atomic wait, callback reentry, nested trap/exception cleanup, zero-timeout/mismatch cancellation and 50 spurious mailbox wake races.");
}
