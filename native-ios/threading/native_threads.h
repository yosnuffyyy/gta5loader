#ifndef NATIVE_THREADS_H
#define NATIVE_THREADS_H
#include "native_host.h"
#include "asset_io/native_assets.h"
typedef struct NativeThreads NativeThreads;
NativeThreads *native_threads_begin(struct w2c_env*);
void native_threads_end(NativeThreads*);
void native_threads_probe(struct w2c_env*,uint64_t);
void native_main_checkpoint(struct w2c_env*);
bool native_main_reached(struct w2c_env*);
w2c_gta *native_current_engine(struct w2c_env*);
uint32_t native_thread_create(struct w2c_env*,uint64_t,uint64_t,uint64_t,uint64_t);
void native_thread_cleanup(struct w2c_env*,uint64_t);
void native_mailbox_register(struct w2c_env*,uint64_t);
void native_mailbox_notify(struct w2c_env*,uint64_t,uint64_t);
double native_proxy_receive(struct w2c_env*,uint32_t,uint64_t,uint64_t,uint32_t,uint64_t,uint64_t,uint64_t);
uint32_t native_calendar(struct w2c_env*,uint64_t,uint64_t,bool);
void native_timezone(struct w2c_env*,uint64_t,uint64_t,uint64_t,uint64_t);
void native_publish(struct w2c_env*,uint64_t,uint64_t,bool);
void native_thread_error(struct w2c_env*,const char*);
void native_set_assets(NativeAssets*);
NativeAssets *native_current_assets(void);
uint32_t native_language(struct w2c_env*,uint64_t,uint64_t,uint64_t);
#endif
