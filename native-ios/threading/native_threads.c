#include "native_threads.h"
#include "platform/native_paths.h"
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
 _Atomic bool finished,wait_observed;
 bool cancelled;
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
 uint64_t requested_routine,input,audio,audio_capacity,deadline;
 NativeAssets *assets;
 _Atomic unsigned game_started;
 _Atomic bool archive_boundary;
 unsigned generations;
};
static _Thread_local NativeThreads *current;
static _Thread_local NativeWorker *worker;
static void check(struct w2c_env *h,bool yes,const char *why){if(!yes)host_unavailable(h,why);}
static void checked(int error){if(error)abort();}
static void report(struct w2c_env *h,const char *s){if(h->progress)h->progress(h->progress_context,s);}
static uint64_t now_ns(void){struct timespec t;checked(clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
static void signal_change(NativeThreads *r){checked(pthread_mutex_lock(&r->mutex));checked(pthread_cond_broadcast(&r->changed));checked(pthread_mutex_unlock(&r->mutex));}
static double probe_proxy(struct w2c_env *h,w2c_gta *e,uint64_t p,unsigned index,unsigned n,const uint64_t *args){
 for(unsigned i=0;i<n;i++){host_put64(h,p+16*i,1);host_put64(h,p+16*i+8,args[i]);}
 return w2c_gta_0x5Femscripten_run_js_on_main_thread(e,index,0,n*16,p,1);
}
// Called by native_wait.c outside its waiter locks before unwinding a worker.
bool native_wait_cancelled(void){
 if(!current)return false;
 if(worker&&current->main_mode)atomic_store(&worker->wait_observed,true);
 if(now_ns()>current->deadline){atomic_store(&current->stopping,true);}
 if(native_assets_failure(current->assets)){atomic_store(&current->stopping,true);}
 return atomic_load(&current->stopping);
}
w2c_gta *native_current_engine(struct w2c_env *h){return worker?worker->engine:h->engine;}
void native_thread_error(struct w2c_env *h,const char *name){
 if(worker){if(!worker->failure)worker->failure=name;}
 else if(!h->failure)h->failure=name;
}
NativeThreads *native_threads_begin(struct w2c_env *h){
 check(h,!current,"thread runtime already active");NativeThreads *r=calloc(1,sizeof(*r));check(h,r!=NULL,"thread runtime allocation");
 r->host=h;r->owner=pthread_self();r->deadline=now_ns()+90000000000ull;checked(pthread_mutex_init(&r->mutex,NULL));checked(pthread_cond_init(&r->changed,NULL));current=r;return r;
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
 uint64_t sp=w2c_gta_emscripten_stack_get_current(e),p=w2c_gta_0x5Femscripten_stack_alloc(e,80*1024);
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
 uint64_t packed=p+70000;
 double proxied_fd=probe_proxy(h,e,packed,18,4,(uint64_t[]){(uint64_t)(int64_t)-100,p,0,0});
 check(h,proxied_fd>=3&&proxied_fd<HOST_FDS,"proxied file open failed");
 uint64_t descriptor=(uint64_t)proxied_fd;
 check(h,probe_proxy(h,e,packed,7,2,(uint64_t[]){descriptor,p+71000})==0&&host_u64(h,p+71032)==sizeof(expected),"proxied stat failed");
 host_put64(h,p+72000,p+512);host_put64(h,p+72008,sizeof(expected));
 check(h,probe_proxy(h,e,packed,35,4,(uint64_t[]){descriptor,p+72000,1,p+72016})==0&&host_u64(h,p+72016)==sizeof(expected),"proxied read failed");
 host_load(h,actual,p+512,sizeof(actual));check(h,!memcmp(expected,actual,sizeof(actual)),"proxied read bytes differ");
 check(h,probe_proxy(h,e,packed,32,1,(uint64_t[]){descriptor})==0,"proxied close failed");
 check(h,unlinkat(h->save_root,path+10,0)==0,"thread probe file cleanup failed");
 check(h,probe_proxy(h,e,packed,18,4,(uint64_t[]){(uint64_t)(int64_t)-100,p,0,0})==-44,"proxied missing-file errno differs");
 if(r->assets)native_assets_thread_probe(r->assets,e,p,(unsigned)id);
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
  if(r->main_mode)atomic_fetch_add(&r->game_started,1);
  uint64_t result=((uint64_t(*)(w2c_gta*,uint64_t))entry.func)(entry.module_instance,w->arg);
  w2c_gta_0x5Femscripten_thread_exit(w->engine,result);
 }else if(!w->failure){if(atomic_load(&r->stopping))w->cancelled=true;else w->failure="worker engine trap";}
 if(w->failure)atomic_store(&r->stopping,true);
 if(instantiated)wasm2c_gta_free(w->engine);free(w->engine);w->engine=NULL;
 wasm_rt_free_thread();atomic_store(&w->finished,true);signal_change(r);worker=NULL;current=NULL;return NULL;
}
uint32_t native_thread_create(struct w2c_env *h,uint64_t tcb,uint64_t attr,uint64_t routine,uint64_t arg){
 (void)attr;check(h,current!=NULL,"pthread create without runtime");NativeThreads *r=current;
 if(!r->probe_mode&&!r->main_mode)return 6;
 if(atomic_load(&r->stopping))return 6;
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
  check(h,atomic_load(&r->completed)==LIMIT&&atomic_load(&r->proxies)==6*LIMIT,"worker/proxy completion count differs");r->generations++;
 }
 uint32_t err;host_load(h,&err,main_errno,4);check(h,err==777,"main thread TLS overwritten");r->probe_mode=false;
 report(h,"PASS: 8 engine pthreads in two batches; distinct TLS/stacks, shared heap, mailbox/proxy replies, concurrent save reads/writes, queued asset reads and joined cleanup.");
}
static uint64_t proxy_arg(struct w2c_env *h,uint64_t p){
 uint64_t tag=host_u64(h,p),bits=host_u64(h,p+8);if(tag==1)return bits;
 check(h,tag==0,"invalid proxy argument tag");double value;memcpy(&value,&bits,8);check(h,isfinite(value)&&value>=-9007199254740991.0&&value<=9007199254740991.0&&floor(value)==value,"invalid numeric proxy argument");return (uint64_t)(int64_t)value;
}
double native_proxy_receive(struct w2c_env *h,uint32_t index,uint64_t asmaddr,uint64_t caller,uint32_t bytes,uint64_t args,uint64_t ctx,uint64_t ctxargs){
 check(h,current&&!worker&&pthread_equal(current->owner,pthread_self()),"proxy executed outside engine owner");
 check(h,!asmaddr&&((ctx==0)==(ctxargs==0))&&bytes%16==0&&bytes<=16*6&&host_range(h,args,bytes),"unsupported async/ASM proxy or malformed args");
 uint64_t a[6]={0};for(unsigned i=0;i<bytes/16;i++)a[i]=proxy_arg(h,args+16*i);
 check(h,caller!=0,"proxy has no caller");uint32_t result;
 switch(index){
 case 31:check(h,bytes==32,"environment proxy arity");result=host_environment(h,a[0],a[1],true);break;
 case 30:check(h,bytes==32,"environment proxy arity");result=host_environment(h,a[0],a[1],false);break;
 case 2:check(h,bytes==64,"thread proxy arity");result=native_thread_create(h,a[0],a[1],a[2],a[3]);break;
 case 7:check(h,bytes==32,"stat proxy arity");result=host_stat(h,a[0],a[1]);break;
 case 18:check(h,bytes==64,"open proxy arity");result=host_open(h,a[0],a[1],a[2],a[3]);break;
 case 32:check(h,bytes==16,"close proxy arity");result=host_fd_close(h,a[0]);break;
 case 33:case 34:check(h,bytes==80,"positioned IO proxy arity");result=host_io(h,a[0],a[1],a[2],a[4],index==34,true,a[3]);break;
 case 35:case 37:check(h,bytes==64,"IO proxy arity");result=host_io(h,a[0],a[1],a[2],a[3],index==37,false,0);break;
 case 36:check(h,bytes==64,"seek proxy arity");result=host_seek(h,a[0],a[1],a[2],a[3]);break;
 default:host_unavailable(h,"main-thread proxy operation not ported");
 }
 atomic_fetch_add(&current->proxies,1);
 double answer=(index==2||index==7||index==18)?(double)(int32_t)result:(double)result;
 if(ctx){w2c_gta_0x5Femscripten_run_js_on_main_thread_done(h->engine,ctx,ctxargs,answer);return 0;}
 return answer;
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

void native_set_assets(NativeAssets *a){if(!current)abort();check(current->host,!worker,"asset service owner");current->assets=a;}
NativeAssets *native_current_assets(void){return current?current->assets:NULL;}
static void archive_boundary(void *context){NativeThreads *r=context;atomic_store(&r->archive_boundary,true);atomic_store(&r->stopping,true);}
uint32_t native_language(struct w2c_env *h,uint64_t code,uint64_t signature,uint64_t args){
 char sig[8];check(h,code==7980558&&host_string(h,signature,sig,sizeof(sig))&&!strcmp(sig,"pi")&&args%8==0&&host_range(h,args,12),"unsupported embedded host operation");
 uint64_t out=host_u64(h,args);uint32_t cap;host_load(h,&cap,args+8,4);
 const char language[]="en-US";if(cap){size_t n=cap-1;if(n>sizeof(language)-1)n=sizeof(language)-1;check(h,host_range(h,out,n+1),"language buffer invalid");host_store(h,out,language,n);char zero=0;host_store(h,out+n,&zero,1);}return 0;
}
void native_main_checkpoint(struct w2c_env *h){
 NativeThreads *r=current;check(h,r&&!worker&&r->assets,"main checkpoint requires owner and asset service");r->main_mode=true;
 native_assets_arm(r->assets,archive_boundary,r);
 const char *args[]={"native-probe","-rootdir=/game/","-output","-nonetwork","-noSocialClub","-pc:nosocialclub","-nodisplaycalibration","-width=640","-height=360","-forceResolution","-noautoload"};
 uint64_t p=w2c_gta_0x5Femscripten_stack_alloc(h->engine,2048),cursor=p+128;
 for(unsigned i=0;i<sizeof(args)/sizeof(args[0]);i++){host_put64(h,p+8*i,cursor);size_t n=strlen(args[i])+1;host_store(h,cursor,args[i],n);cursor+=n;}
 host_put64(h,p+8*(sizeof(args)/sizeof(args[0])),0);
 w2c_gta_0x5F_main_argc_argv(h->engine,sizeof(args)/sizeof(args[0]),p);host_unavailable(h,"main returned before archive boundary");
}
bool native_main_reached(struct w2c_env *h){
 NativeThreads *r=current;if(!r)return false;
 atomic_store(&r->stopping,true);signal_change(r);
 unsigned waited=0;for(unsigned i=0;i<LIMIT;i++){
  NativeWorker *w=&r->workers[i];if(!w->occupied)continue;
  if(!w->joined){checked(pthread_join(w->thread,NULL));w->joined=true;}
  if(w->failure&&!h->failure)h->failure=w->failure;
  if(atomic_load(&w->wait_observed))waited++;
 }
 const char *io_failure=native_assets_failure(r->assets);if(io_failure&&!h->failure)h->failure=io_failure;
 const char *path=native_assets_boundary(r->assets);
 if(atomic_load(&r->archive_boundary)&&r->input&&r->audio&&!h->failure&&atomic_load(&r->game_started)==2&&waited==2&&path&&!strcmp(path,"x64/audio/audio.rpf")){
  report(h,"PASS: actual main launched two game workers, both executed to native waits; mounted the selected asset index and requested x64/audio/audio.rpf. Stopped deliberately before archive/scene loading; all workers joined.");return true;
 }
 if(!h->failure)h->failure=now_ns()>r->deadline?"startup checkpoint timed out":"startup did not reach verified archive boundary";
 return false;
}
