#ifndef NATIVE_HOST_H
#define NATIVE_HOST_H
#include "gta.h"
#include "probe.h"
#include "memory/native_memory.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>
#define HOST_FDS 64
typedef struct {int fd; bool writable;} HostFD;
struct w2c_env {
 wasm_rt_shared_memory_t memory;
 w2c_gta *engine;
 HostFD files[HOST_FDS];
 int game_root,save_root;
 uint64_t mailbox;
 uint64_t mono_origin,wall_origin;
 native_progress progress;
 void *progress_context;
 const char *failure;
 unsigned log_lines;
 _Atomic unsigned memory_growth_logs,memory_failure_logs;
 pthread_mutex_t memory_report_lock;unsigned memory_history_count;char memory_history[8][512];
};
struct w2c_wasi__snapshot__preview1 {struct w2c_env *env;};
bool host_range(struct w2c_env*,uint64_t,uint64_t);
void host_load(struct w2c_env*,void*,uint64_t,size_t);
void host_store(struct w2c_env*,uint64_t,const void*,size_t);
uint64_t host_u64(struct w2c_env*,uint64_t);
void host_put64(struct w2c_env*,uint64_t,uint64_t);
void host_put32(struct w2c_env*,uint64_t,uint32_t);
bool host_string(struct w2c_env*,uint64_t,char*,size_t);
void host_init(struct w2c_env*);
void host_close(struct w2c_env*);
void host_log(struct w2c_env*,const char*);
_Noreturn void host_unavailable(struct w2c_env*,const char*);
uint32_t host_open(struct w2c_env*,uint32_t,uint64_t,uint32_t,uint64_t);
uint32_t host_fd_close(struct w2c_env*,uint32_t);
uint32_t host_io(struct w2c_env*,uint32_t,uint64_t,uint64_t,uint64_t,bool,bool,uint64_t);
uint32_t host_seek(struct w2c_env*,uint32_t,uint64_t,uint32_t,uint64_t);
uint32_t host_stat(struct w2c_env*,uint32_t,uint64_t);
uint32_t host_resize(struct w2c_env*,uint64_t);
double host_now(struct w2c_env*,bool);
uint32_t host_clock(struct w2c_env*,uint32_t,uint64_t);
uint32_t host_environment(struct w2c_env*,uint64_t,uint64_t,bool);
#endif
