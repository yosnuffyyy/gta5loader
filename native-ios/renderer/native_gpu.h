#ifndef NATIVE_GPU_SERVICE_H
#define NATIVE_GPU_SERVICE_H
#include "native_host.h"
typedef struct NativeGpu NativeGpu;
NativeGpu *native_gpu_create(struct w2c_env*,void*,void(*)(void*),void*);
void native_gpu_reference_shadow_option(NativeGpu*,bool);
void native_gpu_start(NativeGpu*,uint64_t,uint64_t,uint64_t);
void native_gpu_stop(NativeGpu*);
void native_gpu_end(NativeGpu*);
const char *native_gpu_error(NativeGpu*);
bool native_gpu_ready(NativeGpu*);
void native_gpu_summary(NativeGpu*,char*,size_t);
void native_gpu_snapshot(NativeGpu*,char*,size_t);
bool native_gpu_replay_test(struct w2c_env*,char*,size_t);
#endif
