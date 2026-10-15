/* Actual production bridge with deterministic clocks and inert service
   snapshots. No game execution or fabricated scene pixels. */
#include "native_session.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
typedef struct {atomic_uint_fast64_t commands,frames;} NativeGpu;
typedef struct {atomic_uint_fast64_t requests;} NativeAssets;
typedef struct {atomic_uint lines;} NativeLog;
typedef struct {const char *phase;} NativeWorker;
typedef struct {bool watch_enabled,watch_reported;atomic_bool stopping;_Atomic uint64_t watch_check_at;pthread_mutex_t watch_mutex;NativeSession session;NativeGpu *gpu;NativeAssets *assets;NativeLog log;atomic_uint game_started,proxies;atomic_bool mailbox_pending;} NativeThreads;
static atomic_uint_fast64_t clock_ns;static atomic_bool scene_stop,scene_suspended;
static pthread_mutex_t scene_status_mutex=PTHREAD_MUTEX_INITIALIZER;
static NativeThreads *scene_runtime;static _Thread_local NativeWorker *worker;
static uint64_t now_ns(void){return atomic_load(&clock_ns);}
static void checked(int code){if(code)abort();}
static void native_gpu_progress(NativeGpu *g,uint64_t *commands,uint64_t *frames){*commands=atomic_load(&g->commands);*frames=atomic_load(&g->frames);}
static uint64_t native_assets_requests(NativeAssets *a){return atomic_load(&a->requests);}
static void native_gpu_snapshot(NativeGpu *g,char *out,size_t n){snprintf(out,n,"test atomic GPU snapshot: %llu",(unsigned long long)atomic_load(&g->commands));}
static void native_log_push(NativeLog *log,const char *text){assert(text&&strlen(text)<768);atomic_fetch_add(&log->lines,1);}
#include "native_session_bridge.inc"
#define NS(n) ((uint64_t)(n)*SESSION_SECOND)
static void *hooks(void *r){for(unsigned i=0;i<100000;i++)scene_watch_hook(r);return NULL;}
static void *polls(void *r){for(unsigned i=0;i<100000;i++)scene_watch_poll(r);return NULL;}
int main(void){NativeGpu gpu={0};NativeAssets assets={0};NativeThreads r={.watch_enabled=true,.gpu=&gpu,.assets=&assets};checked(pthread_mutex_init(&r.watch_mutex,NULL));scene_runtime=&r;atomic_store(&clock_ns,NS(100));session_begin(&r.session,now_ns());
 scene_watch_hook(&r);session_log(&r.session,"Animated camera intial frame");atomic_store(&clock_ns,NS(101));scene_watch_hook(&r);atomic_store(&gpu.frames,1);assert(!scene_watch_poll(&r)&&r.session.phase==SESSION_LIVE);assert(atomic_load(&r.log.lines)==1);char summary[512];scene_watch_summary(&r,summary,sizeof(summary));assert(strstr(summary,"Session live"));
 pthread_t a,b,c;checked(pthread_create(&a,NULL,hooks,&r));checked(pthread_create(&b,NULL,polls,&r));checked(pthread_create(&c,NULL,polls,&r));checked(pthread_join(a,NULL));checked(pthread_join(b,NULL));checked(pthread_join(c,NULL));assert(r.session.hooks==100002&&!r.session.fault);
 native_scene_suspend(true);atomic_store(&clock_ns,NS(5000));assert(!scene_watch_poll(&r));native_scene_suspend(false);scene_watch_hook(&r);atomic_store(&gpu.frames,2);assert(!scene_watch_poll(&r));
 atomic_store(&clock_ns,NS(5060));assert(scene_watch_poll(&r));assert(atomic_load(&r.stopping)&&r.session.fault==SESSION_ENGINE_STALL&&r.watch_reported);unsigned lines=atomic_load(&r.log.lines);assert(!scene_watch_poll(&r)&&atomic_load(&r.log.lines)==lines); /* diagnostic once */
 atomic_store(&r.stopping,false);atomic_store(&r.watch_check_at,0);session_begin(&r.session,now_ns());r.watch_reported=false;atomic_store(&scene_stop,false);atomic_store(&clock_ns,NS(9999));checked(pthread_mutex_lock(&r.watch_mutex));checked(pthread_create(&a,NULL,polls,&r));unsigned attempts=0;while(!atomic_load(&r.watch_check_at)&&attempts++<1000){struct timespec delay={0,1000000};nanosleep(&delay,NULL);}assert(attempts<1000);atomic_store(&scene_stop,true);checked(pthread_mutex_unlock(&r.watch_mutex));checked(pthread_join(a,NULL));assert(!r.session.fault&&!atomic_load(&r.stopping));assert(!scene_watch_poll(&r));session_stop(&r.session);assert(!scene_watch_poll(&r));
 checked(pthread_mutex_lock(&scene_status_mutex));scene_runtime=NULL;checked(pthread_mutex_unlock(&scene_status_mutex));native_scene_suspend(true);native_scene_suspend(false);checked(pthread_mutex_destroy(&r.watch_mutex));
 puts("PASS: production session bridge: actual hook/marker/presentation transition,100000 concurrent hooks/200000 polls, suspended clock, fatal latch, bounded report, cancellation and detached runtime lifetime; inert services, no gameplay claim.");return 0;
}
