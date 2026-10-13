#ifndef NATIVE_PUMP_H
#define NATIVE_PUMP_H
#include "wasm-rt.h"
#include <pthread.h>
struct w2c_env;
// A mailbox wake is spurious for WebAssembly wait/notify: it must never report
// an atomic notification, or alter the address's value.
void native_wait_poke(wasm_rt_shared_memory_t *,pthread_t);
void native_thread_tick(struct w2c_env *);
#endif
