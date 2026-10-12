#include "native_threads.h"
#include "gta-impl.h"
#include "wasm-rt-impl.h"
#include "wasm-rt-exceptions.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif

// Each OS thread owns an engine instance (globals, stack pointer and TLS).
// Instances import the same host and shared heap. No heap copies or JIT.
// These names/signatures are verified against the pinned engine by the generator.
#define engine_create w2c_gta_f90879
#define engine_join w2c_gta_f90884
#define engine_errno w2c_gta_f90695
#define LIMIT 4
typedef struct NativeWorker {
 struct NativeThreads *runtime;
 pthread_t thread;
 w2c_gta *engine;
 uint64_t tcb,routine,arg,errno_address;
 bool occupied,joined,cleanup;
 _Atomic bool finished;
 const char *failure;
} NativeWorker;
struct NativeThreads {
 struct w2c_env *host;
 pthread_t owner;
 pthread_mutex_t mutex;
 pthread_cond_t changed;
 NativeWorker workers[LIMIT];
 _Atomic bool stopping;
 _Atomic unsigned ready,completed,proxies;
 bool probe_mode,main_mode,reached_main_boundary;
 uint64_t requested_routine,input,audio,audio_capacity;
 unsigned generations;
};
static _Thread_local NativeThreads *current;
static _Thread_local NativeWorker *worker;
static void check(struct w2c_env *h,bool yes,const char *why){if(!yes)host_unavailable(h,why);}
static void checked(int error){if(error)abort();}
static void report(struct w2c_env *h,const char *s){if(h->progress)h->progress(h->progress_context,s);}
static uint64_t now_ns(void){struct timespec t;checked(clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
static void signal_change(NativeThreads *r){checked(pthread_mutex_lock(&r->mutex));checked(pthread_cond_broadcast(&r->changed));checked(pthread_mutex_unlock(&r->mutex));}
// Called by native_wait.c outside its waiter locks before unwinding a worker.
bool native_wait_cancelled(void){return current&&atomic_load(&current->stopping);}
w2c_gta *native_current_engine(struct w2c_env *h){return worker?worker->engine:h->engine;}
void native_thread_error(struct w2c_env *h,const char *name){
 if(worker){if(!worker->failure)worker->failure=name;}
 else if(!h->failure)h->failure=name;
}
NativeThreads *native_threads_begin(struct w2c_env *h){
 check(h,!current,"thread runtime already active");NativeThreads *r=calloc(1,sizeof(*r));check(h,r!=NULL,"thread runtime allocation");
 r->host=h;r->owner=pthread_self();checked(pthread_mutex_init(&r->mutex,NULL));checked(pthread_cond_init(&r->changed,NULL));current=r;return r;
}
void native_mailbox_register(struct w2c_env *h,uint64_t tcb){
 check(h,host_range(h,tcb,208)&&(tcb%8)==0,"invalid thread mailbox");
 if(!worker)h->mailbox=tcb;
 // Native condition-variable notification replaces browser waitAsync. Keeping
 // this flag clear makes the engine call the postmessage import on delivery.
 atomic_store((_Atomic uint32_t*)(h->memory.data+tcb+204),0);
}
void native_mailbox_notify(struct w2c_env *h,uint64_t target,uint64_t sender){
 (void)sender;check(h,current!=NULL,"mailbox without native thread runtime");
 NativeThreads *r=current;bool found=target==h->mailbox;
 checked(pthread_mutex_lock(&r->mutex));
 for(unsigned i=0;i<LIMIT;i++)if(r->workers[i].occupied&&r->workers[i].tcb==target)found=true;
 checked(pthread_cond_broadcast(&r->changed));checked(pthread_mutex_unlock(&r->mutex));
 // Emscripten tolerates notifications racing with thread removal.
 (void)found;
}
void native_thread_cleanup(struct w2c_env *h,uint64_t tcb){
 check(h,current!=NULL,"cleanup without native thread runtime");NativeThreads *r=current;
 checked(pthread_mutex_lock(&r->mutex));
 for(unsigned i=0;i<LIMIT;i++)if(r->workers[i].occupied&&r->workers[i].tcb==tcb)r->workers[i].cleanup=true;
 checked(pthread_mutex_unlock(&r->mutex));
 // Never free engine TLS/stack while its native thread can still execute.
}
static uint64_t diagnostic_entry(w2c_gta *e,uint64_t id){
 NativeThreads *r=current;struct w2c_env *h=r->host;
 check(h,r->probe_mode&&worker&&worker->engine==e,"diagnostic dispatch instance mismatch");
 check(h,w2c_gta_pthread_self(e)==worker->tcb,"worker pthread_self differs");
 worker->errno_address=engine_errno(e);
 uint64_t sp=w2c_gta_emscripten_stack_get_current(e),p=w2c_gta_0x5Femscripten_stack_alloc(e,8192);
 check(h,sp<=host_u64(h,worker->tcb+80)&&p>=host_u64(h,worker->tcb+80)-host_u64(h,worker->tcb+88),"worker shadow stack bounds");
 host_put32(h,worker->errno_address,(uint32_t)(100+id));
 atomic_fetch_add(&r->ready,1);signal_change(r);
 uint64_t deadline=now_ns()+10000000000ull;
 while(atomic_load(&r->ready)<LIMIT){check(h,now_ns()<deadline&&!atomic_load(&r->stopping),"worker barrier timeout");struct timespec delay={0,1000000};nanosleep(&delay,NULL);}
 // Genuine engine mailbox/proxy machinery. Each typed proxy argument uses
 // the original 16-byte tag/value encoding from game.js.
 host_put64(h,p,1);host_put64(h,p+8,p+128);host_put64(h,p+16,1);host_put64(h,p+24,p+136);
 double result=w2c_gta_0x5Femscripten_run_js_on_main_thread(e,31,0,32,p,1);
 check(h,result==0&&host_u64(h,p+128)==3,"worker-to-main environment proxy failed");
 uint32_t err=0;host_load(h,&err,worker->errno_address,4);check(h,err==100+id,"thread-local errno was overwritten");
 char path[96];snprintf(path,sizeof(path),"/userdata/.native-thread-%llu",(unsigned long long)id);
 host_store(h,p,path,strlen(path)+1);host_store(h,p+128,"wb",3);
 uint64_t f=w2c_gta_native_probe_fopen(e,p,p+128);check(h,f!=0,"concurrent engine fopen failed");
 unsigned char expected[4096];for(unsigned i=0;i<sizeof(expected);i++)expected[i]=(unsigned char)(i*29+id);
 host_store(h,p+512,expected,sizeof(expected));
 check(h,w2c_gta_native_probe_fwrite(e,p+512,1,sizeof(expected),f)==sizeof(expected),"concurrent engine fwrite failed");w2c_gta_native_probe_fclose(e,f);
 host_store(h,p+128,"rb",3);f=w2c_gta_native_probe_fopen(e,p,p+128);check(h,f!=0,"concurrent engine reopen failed");
 check(h,w2c_gta_native_probe_fread(e,p+512,1,sizeof(expected),f)==sizeof(expected),"concurrent engine fread failed");
 unsigned char actual[4096];host_load(h,actual,p+512,sizeof(actual));check(h,!memcmp(expected,actual,sizeof(actual)),"concurrent file bytes differ");w2c_gta_native_probe_fclose(e,f);
 check(h,unlinkat(h->save_root,path+10,0)==0,"thread probe file cleanup failed");
 w2c_gta_0x5Femscripten_stack_restore(e,sp);atomic_fetch_add(&r->completed,1);return id+123;
}
static void *run_worker(void *arg){
 NativeWorker *w=arg;NativeThreads *r=w->runtime;struct w2c_env *h=r->host;current=r;worker=w;
 wasm_rt_init_thread();
 w->engine=calloc(1,sizeof(*w->engine));struct w2c_wasi__snapshot__preview1 wasi={.env=h};
 volatile bool instantiated=false;
 wasm_rt_trap_t trap=wasm_rt_impl_try();
 if(!trap){
  check(h,w->engine!=NULL,"worker instance allocation");
  wasm2c_gta_instantiate(w->engine,h,&wasi);instantiated=true;
  uint64_t high=host_u64(h,w->tcb+80),size=host_u64(h,w->tcb+88);
  check(h,size>0&&high>=size&&host_range(h,high-size,size),"invalid worker stack descriptor");
  w2c_gta_emscripten_stack_set_limits(w->engine,high,high-size);w2c_gta_0x5Femscripten_stack_restore(w->engine,high);
  w2c_gta_0x5Femscripten_thread_init(w->engine,w->tcb,0,0,1,0,0);w2c_gta_0x5Femscripten_tls_init(w->engine);native_mailbox_register(h,w->tcb);
  wasm_rt_funcref_table_t *table=w2c_gta_0x5F_indirect_function_table(w->engine);
  // Slot zero is null in this fixed engine. Install a diagnostic-only native
  // entry to exercise the unchanged engine pthread_create/exit/join functions.
  if(r->probe_mode&&w->routine==0){check(h,table->data[0].func==NULL,"diagnostic slot is not null");table->data[0]=(wasm_rt_funcref_t){.func_type=wasm2c_gta_get_func_type(1,1,WASM_RT_I64,WASM_RT_I64),.func=(wasm_rt_function_ptr_t)diagnostic_entry,.module_instance=w->engine};}
  check(h,w->routine<table->size,"invalid worker entry index");wasm_rt_funcref_t entry=table->data[w->routine];
  check(h,entry.func&&entry.func_type&&!memcmp(entry.func_type,wasm2c_gta_get_func_type(1,1,WASM_RT_I64,WASM_RT_I64),32),"invalid worker entry signature");
  uint64_t result=((uint64_t(*)(w2c_gta*,uint64_t))entry.func)(entry.module_instance,w->arg);
  w2c_gta_0x5Femscripten_thread_exit(w->engine,result);
 }else if(!w->failure)w->failure=atomic_load(&r->stopping)?"worker cancelled during shutdown":"worker engine trap";
 if(instantiated)wasm2c_gta_free(w->engine);free(w->engine);w->engine=NULL;
 wasm_rt_free_thread();atomic_store(&w->finished,true);signal_change(r);worker=NULL;current=NULL;return NULL;
}
uint32_t native_thread_create(struct w2c_env *h,uint64_t tcb,uint64_t attr,uint64_t routine,uint64_t arg){
 (void)attr;check(h,current!=NULL,"pthread create without runtime");NativeThreads *r=current;
 if(r->main_mode){r->reached_main_boundary=true;r->requested_routine=routine;wasm_rt_trap(WASM_RT_TRAP_UNREACHABLE);}
 if(!r->probe_mode)return 6;
 if(!host_range(h,tcb,208)||(tcb%8)!=0)return 28;
 checked(pthread_mutex_lock(&r->mutex));NativeWorker *w=NULL;
 for(unsigned i=0;i<LIMIT;i++)if(!r->workers[i].occupied){w=&r->workers[i];break;}
 if(!w){checked(pthread_mutex_unlock(&r->mutex));return 6;}
 *w=(NativeWorker){.runtime=r,.tcb=tcb,.routine=routine,.arg=arg,.occupied=true};
 pthread_attr_t a;int error=pthread_attr_init(&a);
 if(!error){error=pthread_attr_setstacksize(&a,4*1024*1024);if(!error)error=pthread_create(&w->thread,&a,run_worker,w);pthread_attr_destroy(&a);}
 if(error)w->occupied=false;checked(pthread_mutex_unlock(&r->mutex));return error?6:0;
}
static void wait_finished(NativeThreads *r){
 uint64_t deadline=now_ns()+15000000000ull;
 bool sampled=false;
 for(;;){
  if(!sampled&&atomic_load(&r->ready)==LIMIT){
   sampled=true;
#ifdef __APPLE__
   task_vm_info_data_t info={0};mach_msg_type_number_t count=TASK_VM_INFO_COUNT;
   if(task_info(mach_task_self(),TASK_VM_INFO,(task_info_t)&info,&count)==KERN_SUCCESS){char line[180];snprintf(line,sizeof(line),"Worker-batch process footprint: %.1f MiB (sample, not peak; workers may already be finishing).",(double)info.phys_footprint/1048576.0);report(r->host,line);}
#endif
  }
  w2c_gta_0x5Femscripten_check_mailbox(r->host->engine);
  bool all=true;for(unsigned i=0;i<LIMIT;i++)if(r->workers[i].occupied&&!atomic_load(&r->workers[i].finished))all=false;
  if(all)return;check(r->host,now_ns()<deadline,"native workers timed out");
  struct timespec until;checked(clock_gettime(CLOCK_REALTIME,&until));until.tv_nsec+=10000000;if(until.tv_nsec>=1000000000){until.tv_sec++;until.tv_nsec-=1000000000;}
  checked(pthread_mutex_lock(&r->mutex));int rc=pthread_cond_timedwait(&r->changed,&r->mutex,&until);checked(pthread_mutex_unlock(&r->mutex));check(r->host,rc==0||rc==ETIMEDOUT,"native worker wait failed");
 }
}
void native_threads_probe(struct w2c_env *h,uint64_t p){
 NativeThreads *r=current;check(h,r&&!worker,"worker probe must run on engine owner");
 uint64_t main_errno=engine_errno(h->engine);host_put32(h,main_errno,777);r->probe_mode=true;
 for(unsigned round=0;round<2;round++){
  atomic_store(&r->ready,0);atomic_store(&r->completed,0);atomic_store(&r->proxies,0);
  for(unsigned i=0;i<LIMIT;i++)check(h,engine_create(h->engine,p+8*i,0,0,1+i+round*LIMIT)==0,"engine pthread_create failed");
  wait_finished(r);
  for(unsigned i=0;i<LIMIT;i++){NativeWorker *w=&r->workers[i];checked(pthread_join(w->thread,NULL));w->joined=true;check(h,!w->failure,w->failure?w->failure:"worker failed");}
  for(unsigned i=0;i<LIMIT;i++){
   NativeWorker *w=&r->workers[i];check(h,w->errno_address!=main_errno,"worker shares main errno");
   for(unsigned j=0;j<i;j++)check(h,w->errno_address!=r->workers[j].errno_address,"workers share TLS");
   check(h,engine_join(h->engine,host_u64(h,p+8*i))==0,"engine pthread_join failed");
  }
  for(unsigned i=0;i<LIMIT;i++){NativeWorker *w=&r->workers[i];check(h,w->cleanup,"engine join omitted worker cleanup");w2c_gta_0x5Femscripten_thread_free_data(h->engine,w->tcb);w->occupied=false;}
  check(h,atomic_load(&r->completed)==LIMIT&&atomic_load(&r->proxies)==LIMIT,"worker/proxy completion count differs");r->generations++;
 }
 uint32_t err;host_load(h,&err,main_errno,4);check(h,err==777,"main thread TLS overwritten");r->probe_mode=false;
 report(h,"PASS: 8 engine pthreads in two batches; distinct TLS/stacks, shared heap, mailbox/proxy replies, concurrent save reads/writes and joined cleanup.");
}
static uint64_t proxy_arg(struct w2c_env *h,uint64_t p){
 uint64_t tag=host_u64(h,p),bits=host_u64(h,p+8);if(tag==1)return bits;
 check(h,tag==0,"invalid proxy argument tag");double value;memcpy(&value,&bits,8);check(h,isfinite(value)&&value>=0&&value<=9007199254740991.0&&floor(value)==value,"invalid numeric proxy argument");return (uint64_t)value;
}
double native_proxy_receive(struct w2c_env *h,uint32_t index,uint64_t asmaddr,uint64_t caller,uint32_t bytes,uint64_t args,uint64_t ctx,uint64_t ctxargs){
 check(h,current&&!worker&&pthread_equal(current->owner,pthread_self()),"proxy executed outside engine owner");
 check(h,!asmaddr&&!ctx&&!ctxargs&&bytes%16==0&&bytes<=16*6&&host_range(h,args,bytes),"unsupported async/ASM proxy or malformed args");
 uint64_t a[6]={0};for(unsigned i=0;i<bytes/16;i++)a[i]=proxy_arg(h,args+16*i);
 check(h,caller!=0,"proxy has no caller");uint32_t result;
 switch(index){
 case 31:check(h,bytes==32,"environment proxy arity");result=host_environment(h,a[0],a[1],true);break;
 case 30:check(h,bytes==32,"environment proxy arity");result=host_environment(h,a[0],a[1],false);break;
 default:host_unavailable(h,"main-thread proxy operation not ported");
 }
 atomic_fetch_add(&current->proxies,1);return result;
}
void native_threads_end(NativeThreads *r){
 if(!r)return;atomic_store(&r->stopping,true);signal_change(r);
 for(unsigned i=0;i<LIMIT;i++)if(r->workers[i].occupied&&!r->workers[i].joined)checked(pthread_join(r->workers[i].thread,NULL));
 checked(pthread_cond_destroy(&r->changed));checked(pthread_mutex_destroy(&r->mutex));current=NULL;worker=NULL;free(r);
}
uint32_t native_calendar(struct w2c_env *h,uint64_t seconds,uint64_t p,bool local){
 (void)local;if(!host_range(h,p,56))return 1;time_t value=(time_t)(int64_t)seconds;struct tm tm;
 if(!gmtime_r(&value,&tm))return 1;int32_t fields[9]={tm.tm_sec,tm.tm_min,tm.tm_hour,tm.tm_mday,tm.tm_mon,tm.tm_year,tm.tm_wday,tm.tm_yday,0};
 host_store(h,p,fields,sizeof(fields));host_put64(h,p+40,0);host_put64(h,p+48,0);return 0;
}
void native_timezone(struct w2c_env *h,uint64_t zone,uint64_t dst,uint64_t stdname,uint64_t dstname){
 // Deterministic UTC for this startup diagnostic; no libc timezone globals.
 host_put64(h,zone,0);host_put32(h,dst,0);host_store(h,stdname,"UTC",4);host_store(h,dstname,"UTC",4);
}
void native_publish(struct w2c_env *h,uint64_t address,uint64_t capacity,bool audio){
 check(h,current&&current->main_mode&&host_range(h,address,audio?capacity*4:4)&&capacity<=1048576,"invalid startup shared state");
 if(audio){current->audio=address;current->audio_capacity=capacity;}else current->input=address;
 // Record handoff only: no audio output or gameplay input claimed here.
}
void native_main_checkpoint(struct w2c_env *h){
 NativeThreads *r=current;check(h,r&&!worker,"main checkpoint requires owner");r->main_mode=true;
 w2c_gta_0x5F_main_argc_argv(h->engine,0,0);host_unavailable(h,"main returned before worker boundary");
}
bool native_main_reached(struct w2c_env *h){
 NativeThreads *r=current;if(!r)return false;
 if(r->reached_main_boundary&&r->input&&r->audio&&!h->failure){
  char line[256];snprintf(line,sizeof(line),"PASS: actual engine main entered; input/audio state published; first game-worker request reached (entry %llu). Stopped deliberately before launching game workers.",(unsigned long long)r->requested_routine);report(h,line);
  return true;
 }return false;
}
