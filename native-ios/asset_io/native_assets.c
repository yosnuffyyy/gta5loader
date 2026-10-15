#include "native_assets.h"
#include "gta-impl.h"
#include "native_wait.h"
#include "wasm-rt-impl.h"
#include "wasm-rt-exceptions.h"
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "renderer/timing_window.h"

// Pinned original engine functions. No binary patches or extra exports.
#define engine_lookup w2c_gta_f194
#define engine_read w2c_gta_f198
#define SLOT_LIMIT 512
#define CACHE_HANDLES 8
struct NativeAssets {
 struct w2c_env *host;
 NativeAssetIndex index;
 uint64_t table;
 pthread_t thread;
 bool started;
 _Atomic bool stopping,armed;
 _Atomic unsigned requests;
 _Atomic(const char*) failure;
 _Atomic(const char*) boundary;
 void (*on_boundary)(void*);void *context;
 struct {int fd;uint32_t id;uint64_t stamp;} cache[CACHE_HANDLES];
 uint64_t stamp;
 unsigned char scratch[65536];
 uint32_t sample_ids[3];
 TimingWindow service_times;uint64_t cache_hits,cache_misses;
 uint64_t bytes_read,io_errors,size_mismatches,short_reads;unsigned diagnostic_count;char diagnostics[8][384];
};
static void asset_diagnostic(NativeAssets *a,const char *operation,uint32_t id,uint64_t offset,uint32_t length,int error){if(a->diagnostic_count>=8)return;char *out=a->diagnostics[a->diagnostic_count++];snprintf(out,384,"Asset %s: id %u path %.220s, offset %llu, requested %u, errno %d",operation,id,id<a->index.count?a->index.files[id].path:"<invalid>",(unsigned long long)offset,length,error);}
static void check(NativeAssets *a,bool ok,const char *why){if(!ok)host_unavailable(a->host,why);}
static _Atomic uint32_t *word(NativeAssets *a,uint64_t p){return (_Atomic uint32_t*)(a->host->memory.data+p);}
static int cached_file(NativeAssets *a,uint32_t id){
 unsigned slot=0;for(unsigned i=0;i<CACHE_HANDLES;i++){
  if(a->cache[i].fd>=0&&a->cache[i].id==id){a->cache[i].stamp=++a->stamp;a->cache_hits++;return a->cache[i].fd;}
  if(a->cache[i].fd<0||a->cache[i].stamp<a->cache[slot].stamp)slot=i;
 }
 a->cache_misses++;int fd=native_asset_open(a->host->game_root,a->index.files[id].path);if(fd<0)return -1;
 if(a->cache[slot].fd>=0)close(a->cache[slot].fd);
 a->cache[slot].fd=fd;a->cache[slot].id=id;a->cache[slot].stamp=++a->stamp;return fd;
}
static int32_t serve(NativeAssets *a,uint32_t id,uint64_t offset,uint64_t dst,uint32_t length){
 if(id>=a->index.count||length>INT32_MAX||offset>INT64_MAX||!host_range(a->host,dst,length)){a->io_errors++;asset_diagnostic(a,"invalid request",id,offset,length,EINVAL);return -1;}
 int fd=cached_file(a,id);if(fd<0){a->io_errors++;asset_diagnostic(a,"open failure",id,offset,length,errno);return -1;}
 struct stat st;if(fstat(fd,&st)||st.st_size<0){a->io_errors++;asset_diagnostic(a,"stat failure",id,offset,length,errno);return -1;}
 if((uint64_t)st.st_size!=a->index.files[id].size){a->size_mismatches++;asset_diagnostic(a,"manifest/physical size mismatch",id,(uint64_t)st.st_size,(uint32_t)(a->index.files[id].size>UINT32_MAX?UINT32_MAX:a->index.files[id].size),0);}
 uint64_t size=(uint64_t)st.st_size;if(size>a->index.files[id].size)size=a->index.files[id].size;
 if(offset>=size)return 0;
 if(length>size-offset)length=(uint32_t)(size-offset);
 uint32_t done=0;while(done<length&&!atomic_load(&a->stopping)){
  size_t n=length-done;if(n>sizeof(a->scratch))n=sizeof(a->scratch);
  ssize_t got;do{got=pread(fd,a->scratch,n,(off_t)(offset+done));}while(got<0&&errno==EINTR);
  if(got<0){a->io_errors++;asset_diagnostic(a,"read failure",id,offset+done,(uint32_t)n,errno);a->bytes_read+=done;return done?(int32_t)done:-1;}if(!got){a->short_reads++;asset_diagnostic(a,"unexpected physical EOF",id,offset+done,(uint32_t)n,0);break;}
  host_store(a->host,dst+done,a->scratch,(size_t)got);done+=(uint32_t)got;
 }
 a->bytes_read+=done;return (int32_t)done;
}
static void *read_loop(void *context){
 NativeAssets *a=context;wasm_rt_init_thread();
 wasm_rt_trap_t trap=wasm_rt_impl_try();
 if(trap){atomic_store(&a->failure,"asset worker memory/protocol trap");goto done;}
 while(!atomic_load(&a->stopping)){
  uint32_t bell=atomic_load(word(a,a->table)),count=atomic_load(word(a,a->table+4));
  if(count>SLOT_LIMIT){atomic_store(&a->failure,"asset slot count exceeds pinned engine limit");break;}
  for(unsigned i=0;i<count;i++){
   uint64_t p=a->table+64+64*i;uint32_t requested=1;
   if(!atomic_compare_exchange_strong(word(a,p),&requested,3))continue;
   uint32_t id=atomic_load(word(a,p+4)),length=atomic_load(word(a,p+24));
   uint64_t offset=host_u64(a->host,p+8),dst=host_u64(a->host,p+16);
   if(atomic_load(&a->armed)){
    if(id<a->index.count)atomic_store(&a->boundary,a->index.files[id].path);
    else atomic_store(&a->failure,"startup requested invalid archive ID");
    // Deliberate CPU checkpoint: leave the request pending and cancel engine
    // waits. No dummy archive bytes are supplied or parsed as game data.
    a->on_boundary(a->context);goto done;
   }
   struct timespec start,end;clock_gettime(CLOCK_MONOTONIC,&start);int32_t result=serve(a,id,offset,dst,length);clock_gettime(CLOCK_MONOTONIC,&end);timing_add(&a->service_times,((uint64_t)end.tv_sec*1000000000ull+end.tv_nsec)-((uint64_t)start.tv_sec*1000000000ull+start.tv_nsec));
   atomic_fetch_add(&a->requests,1);atomic_store(word(a,p+28),(uint32_t)result);
   atomic_store(word(a,p),2);native_notify(&a->host->memory,p,1);
  }
  // Local files do not need HTTP prefetch. Hints are optional and are left
  // unused, like the reference noHints mode. Sleep on the shared doorbell.
  native_wait32(&a->host->memory,a->table,bell,100000000);
 }
done:wasm_rt_free_thread();return NULL;
}
NativeAssets *native_assets_begin(struct w2c_env *h){
 NativeAssets *a=calloc(1,sizeof(*a));if(!a)host_unavailable(h,"asset service allocation");
 a->host=h;for(unsigned i=0;i<CACHE_HANDLES;i++)a->cache[i].fd=-1;
 char error[256];if(!native_asset_index_load(h->game_root,&a->index,error,sizeof(error))){if(h->progress)h->progress(h->progress_context,error);free(a);host_unavailable(h,"selected folder manifest invalid or unavailable");}
 char line[160];snprintf(line,sizeof(line),"Local asset index: %u files; bounded reader: one 64 KiB buffer, at most 8 open files.",a->index.count);if(h->progress)h->progress(h->progress_context,line);
 return a;
}
void native_assets_stop(NativeAssets *a){
 if(!a)return;atomic_store(&a->stopping,true);
 if(a->started){native_notify(&a->host->memory,a->table,1);if(pthread_join(a->thread,NULL))abort();a->started=false;}
}
void native_assets_end(NativeAssets *a){
 if(!a)return;native_assets_stop(a);
 for(unsigned i=0;i<CACHE_HANDLES;i++)if(a->cache[i].fd>=0)close(a->cache[i].fd);
 native_asset_index_free(&a->index);free(a);
}
uint32_t native_assets_manifest(NativeAssets *a,uint64_t p,uint32_t cap){
 if(!a)return UINT32_MAX;
 if(cap>a->index.bytes)host_store(a->host,p,a->index.json,a->index.bytes+1);
 return (uint32_t)a->index.bytes;
}
void native_assets_start(NativeAssets *a,uint64_t table){
 check(a,!a->started&&!(table%4)&&host_range(a->host,table,64+64*SLOT_LIMIT),"invalid or duplicate asset request table");a->table=table;
 check(a,pthread_create(&a->thread,NULL,read_loop,a)==0,"asset reader thread creation failed");a->started=true;
}
void native_assets_arm(NativeAssets *a,void(*callback)(void*),void *context){a->on_boundary=callback;a->context=context;atomic_store(&a->armed,true);}
const char *native_assets_boundary(NativeAssets *a){return a?atomic_load(&a->boundary):NULL;}
const char *native_assets_failure(NativeAssets *a){return a?atomic_load(&a->failure):NULL;}
uint64_t native_assets_requests(NativeAssets *a){return a?atomic_load(&a->requests):0;}
void native_assets_summary(NativeAssets *a,char *out,size_t size){if(!a){snprintf(out,size,"Asset reader not started");return;}snprintf(out,size,"Asset reads: %u requests, %llu MiB delivered; I/O errors %llu; size mismatches %llu; unexpected short reads %llu. Expected EOF/range clipping is not an error.",atomic_load(&a->requests),(unsigned long long)(a->bytes_read/1048576),(unsigned long long)a->io_errors,(unsigned long long)a->size_mismatches,(unsigned long long)a->short_reads);if(a->host->progress){char text[440];timing_text(&a->service_times,"Asset read service wall (excludes queued wait)",text,sizeof(text));a->host->progress(a->host->progress_context,text);snprintf(text,sizeof(text),"Asset descriptor cache hits/misses %llu/%llu; one 64 KiB buffer and at most eight descriptors preserved.",(unsigned long long)a->cache_hits,(unsigned long long)a->cache_misses);a->host->progress(a->host->progress_context,text);}for(unsigned i=0;i<a->diagnostic_count;i++)if(a->host->progress)a->host->progress(a->host->progress_context,a->diagnostics[i]);}
static void sample(NativeAssets *a,w2c_gta *e,uint64_t scratch,unsigned sample){
 static const char *paths[]={"common/data/levels/gta5/paths.xml","x64/anim/cutscene/cuts_prologue.rpf","x64/levels/gta5/_prologue/plg_01.rpf"};
 char path[256];snprintf(path,sizeof(path),"/game/%s",paths[sample]);host_store(a->host,scratch,path,strlen(path)+1);
 int32_t id;
 if(e==a->host->engine){
  id=(int32_t)engine_lookup(e,scratch,scratch+256,scratch+264);
  check(a,id>=0&&(unsigned)id<a->index.count,"engine HTTPFS lookup failed");
  check(a,host_u64(a->host,scratch+256)==a->index.files[id].size,"engine/native manifest sizes differ");
  a->sample_ids[sample]=(uint32_t)id;
 }else{
  // Diagnostic pthreads have libc TLS, but do not run the game's allocator
  // setup wrapper. Resolve C++ path keys on the initialized engine owner;
  // worker tests exercise the real per-thread queued pread path with IDs.
  id=(int32_t)a->sample_ids[sample];
 }
 check(a,!strcmp(a->index.files[id].path,paths[sample]),"engine/native manifest file IDs differ");
 int fd=native_asset_open(a->host->game_root,paths[sample]);check(a,fd>=0,"asset comparison open failed");
 struct stat st;bool valid=!fstat(fd,&st)&&st.st_size>=65536&&(uint64_t)st.st_size==a->index.files[id].size;
 if(!valid){close(fd);host_unavailable(a->host,"sample file size differs from selected manifest");}
 unsigned char expected[65536],actual[65536];
 // Read direct comparison bytes first and close the OS descriptor before any
 // engine call can trap, so a failed test cannot leak the comparison handle.
 for(unsigned tail=0;tail<2;tail++){
  uint64_t offset=tail?(uint64_t)st.st_size-65536:0;
  ssize_t got=pread(fd,expected,sizeof(expected),(off_t)offset);
  if(got!=sizeof(expected)){close(fd);host_unavailable(a->host,"asset comparison short read");}
  close(fd);fd=-1;
  check(a,engine_read(e,(uint32_t)id,offset,scratch+512,65536)==65536,"engine queued asset read failed");
  host_load(a->host,actual,scratch+512,sizeof(actual));check(a,!memcmp(expected,actual,sizeof(actual)),"engine queued asset bytes differ");
  if(!tail){fd=native_asset_open(a->host->game_root,paths[sample]);check(a,fd>=0,"asset comparison reopen failed");}
 }
 check(a,engine_read(e,(uint32_t)id,(uint64_t)st.st_size-7,scratch+512,32)==7,"asset EOF short read differs");
 check(a,engine_read(e,(uint32_t)id,(uint64_t)st.st_size,scratch+512,32)==0,"asset EOF differs");
 check(a,engine_read(e,UINT32_MAX,0,scratch+512,32)==UINT32_MAX,"invalid asset ID accepted");
 check(a,engine_read(e,(uint32_t)id,UINT64_MAX,scratch+512,32)==UINT32_MAX,"invalid asset offset accepted");
 check(a,engine_read(e,(uint32_t)id,0,UINT64_MAX-8,32)==UINT32_MAX,"invalid asset buffer accepted");
}
void native_assets_probe(NativeAssets *a,uint64_t scratch){
 for(unsigned i=0;i<3;i++)sample(a,a->host->engine,scratch,i);
 if(a->host->progress)a->host->progress(a->host->progress_context,"PASS: real engine HTTPFS lookup and queued reads of three files (head/tail), byte comparison, EOF and invalid request rejection.");
}
void native_assets_thread_probe(NativeAssets *a,w2c_gta *e,uint64_t scratch,unsigned id){sample(a,e,scratch,id%3);}
