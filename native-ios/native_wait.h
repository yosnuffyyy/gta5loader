#ifndef GTA_NATIVE_WAIT_H
#define GTA_NATIVE_WAIT_H
#include "wasm-rt.h"
#include <stdint.h>
uint32_t native_wait32(wasm_rt_shared_memory_t*, uint64_t, uint32_t, int64_t);
uint32_t native_wait64(wasm_rt_shared_memory_t*, uint64_t, uint64_t, int64_t);
uint32_t native_notify(wasm_rt_shared_memory_t*, uint64_t, uint32_t);
#endif
