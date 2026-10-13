#ifndef NATIVE_GPU_RING_H
#define NATIVE_GPU_RING_H
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef bool (*gpu_execute)(void*,uint32_t,const uint8_t*,uint32_t);
typedef void (*gpu_wake)(void*,uint64_t);
typedef struct {
 uint8_t *memory;_Atomic uint64_t *memory_size;uint64_t base;
 uint32_t capacity,tail;uint64_t executed;char error[160];
 void *context;gpu_execute execute;gpu_wake wake;
} GpuRing;
bool gpu_ring_init(GpuRing*,uint8_t*,_Atomic uint64_t*,uint64_t,uint32_t,void*,gpu_execute,gpu_wake);
int gpu_ring_drain(GpuRing*,unsigned);
void gpu_ring_failed(GpuRing*);
uint32_t gpu_word(const uint8_t*,unsigned);
float gpu_float(const uint8_t*,unsigned);
const char *gpu_opcode_name(uint32_t);
#endif
