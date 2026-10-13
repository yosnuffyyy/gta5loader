#include "native_host.h"
#include "threading/native_threads.h"
#include "wasm-rt-impl.h"
#include "wasm-rt-exceptions.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif

static void message(struct w2c_env *h,const char *format,...) {
 char line[512];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);
 if(h->progress)h->progress(h->progress_context,line);
}
static void require(struct w2c_env *h,bool ok,const char *what){if(!ok)host_unavailable(h,what);}
static void putstr(struct w2c_env *h,uint64_t p,const char *s){host_store(h,p,s,strlen(s)+1);}
static uint64_t open_engine(struct w2c_env *h,uint64_t scratch,const char *path,const char *mode) {
 putstr(h,scratch,path);putstr(h,scratch+4096,mode);
 return w2c_gta_native_probe_fopen(h->engine,scratch,scratch+4096);
}
static void expect_denied(struct w2c_env *h,uint64_t scratch,const char *path,const char *mode) {
 uint64_t f=open_engine(h,scratch,path,mode);
 if(f)w2c_gta_native_probe_fclose(h->engine,f);
 require(h,f==0,"file isolation: unexpected successful fopen");
}
static void saves(struct w2c_env *h,uint64_t scratch) {
 const char *path="/userdata/.native-prototype-probe.bin";
 unsigned char bytes[65536],actual[65536];
 for(unsigned i=0;i<sizeof(bytes);i++)bytes[i]=(unsigned char)((i*37u+19u)^(i>>8));
 uint64_t buffer=scratch+8192;
 // A previous invocation leaves this probe file behind. Validate it before
 // writing again so the phone can demonstrate persistence across launches.
 uint64_t old=open_engine(h,scratch,path,"rb");
 if(old){
  uint64_t got=w2c_gta_native_probe_fread(h->engine,buffer,1,sizeof(bytes),old);
  host_load(h,actual,buffer,sizeof(actual));w2c_gta_native_probe_fclose(h->engine,old);
  require(h,got==sizeof(bytes)&&!memcmp(bytes,actual,sizeof(bytes)),"previous persistent probe file differs");
  message(h,"PASS: probe save from the previous run is intact.");
 }
 uint64_t f=open_engine(h,scratch,path,"wb");require(h,f!=0,"engine fopen save failed");
 host_store(h,buffer,bytes,sizeof(bytes));
 require(h,w2c_gta_native_probe_fwrite(h->engine,buffer,1,sizeof(bytes),f)==sizeof(bytes),"engine fwrite failed");
 w2c_gta_native_probe_fclose(h->engine,f);
 require(h,h->failure==NULL,"save sync/close failed");
 f=open_engine(h,scratch,path,"rb");require(h,f!=0,"engine reopen save failed");
 require(h,w2c_gta_native_probe_fseeko(h->engine,f,0,SEEK_END)==0,"engine seek end failed");
 require(h,w2c_gta_native_probe_ftello(h->engine,f)==sizeof(bytes),"engine file size differs");
 require(h,w2c_gta_native_probe_fseeko(h->engine,f,(uint64_t)(int64_t)-128,SEEK_END)==0,"engine signed seek failed");
 require(h,w2c_gta_native_probe_fread(h->engine,buffer,1,256,f)==128,"engine EOF short read differs");
 host_load(h,actual,buffer,128);require(h,!memcmp(actual,bytes+sizeof(bytes)-128,128),"engine tail read differs");
 require(h,w2c_gta_native_probe_fseeko(h->engine,f,0,SEEK_SET)==0,"engine rewind failed");
 require(h,w2c_gta_native_probe_fread(h->engine,buffer,16,4096,f)==4096,"engine fread element count differs");
 host_load(h,actual,buffer,sizeof(actual));require(h,!memcmp(actual,bytes,sizeof(bytes)),"engine saved bytes differ");
 w2c_gta_native_probe_fclose(h->engine,f);
 message(h,"PASS: engine fopen/fwrite/fread/fseeko/ftello/fclose; 64 KiB save, reopen, signed offsets and EOF.");
 expect_denied(h,scratch,"/userdata/../outside.bin","wb");
 expect_denied(h,scratch,"/etc/passwd","rb");
 expect_denied(h,scratch,"/data/.native-must-not-create","wb");
 expect_denied(h,scratch,"/userdata/.definitely-missing-native-probe","rb");
 message(h,"PASS: missing files, path traversal and writes to game data are rejected.");
}
static void abi(struct w2c_env *h,uint64_t p) {
 struct w2c_wasi__snapshot__preview1 *w=h->engine->w2c_wasi__snapshot__preview1_instance;
 // A sparse file tests offsets above 4 GiB without allocating 4 GiB of RAM
 // or disk blocks. It is removed after the check, including failure cleanup.
 putstr(h,p,"/userdata/.native-sparse-probe");
 uint32_t fd=w2c_env_0x5F_syscall_openat(h,(uint32_t)-100,p,2|64|512,0);
 require(h,(int32_t)fd>=3,"sparse probe open failed");
 uint64_t vector=p+512,buffer=p+1024,out=p+2048,pos=4294967296ull+123;
 const char bytes[]="native64";host_store(h,buffer,bytes,sizeof(bytes));
 host_put64(h,vector,buffer);host_put64(h,vector+8,sizeof(bytes));
 require(h,w2c_wasi__snapshot__preview1_fd_pwrite(w,fd,vector,1,pos,out)==0&&host_u64(h,out)==sizeof(bytes),"64-bit pwrite failed");
 require(h,w2c_wasi__snapshot__preview1_fd_seek(w,fd,0,1,out)==0&&host_u64(h,out)==0,"pwrite moved file cursor");
 host_put64(h,vector,buffer+128);
 require(h,w2c_wasi__snapshot__preview1_fd_pread(w,fd,vector,1,pos,out)==0&&host_u64(h,out)==sizeof(bytes),"64-bit pread failed");
 char actual[sizeof(bytes)];host_load(h,actual,buffer+128,sizeof(actual));require(h,!memcmp(bytes,actual,sizeof(bytes)),"positioned read bytes differ");
 require(h,w2c_env_0x5F_syscall_fstat64(h,fd,p+3072)==0&&host_u64(h,p+3072+32)==pos+sizeof(bytes),"64-bit stat ABI differs");
 host_put64(h,vector,UINT64_MAX-2);host_put64(h,vector+8,16);
 require(h,w2c_wasi__snapshot__preview1_fd_read(w,fd,vector,1,out)==21,"invalid iovec not rejected");
 require(h,w2c_wasi__snapshot__preview1_fd_close(w,fd)==0,"sparse probe close failed");
 require(h,w2c_wasi__snapshot__preview1_fd_close(w,fd)==8,"closed descriptor not rejected");
 require(h,unlinkat(h->save_root,".native-sparse-probe",0)==0,"sparse probe cleanup failed");
 require(h,w2c_wasi__snapshot__preview1_clock_time_get(w,1,0,out)==0,"monotonic clock failed");
 uint64_t first=host_u64(h,out);
 require(h,w2c_wasi__snapshot__preview1_clock_time_get(w,1,0,out)==0&&host_u64(h,out)>=first,"monotonic clock moved backward");
 require(h,w2c_wasi__snapshot__preview1_environ_sizes_get(w,p,p+8)==0&&host_u64(h,p)==3,"environment sizes ABI differs");
 require(h,w2c_wasi__snapshot__preview1_environ_get(w,p+32,p+256)==0,"environment pointers failed");
 char value[64];require(h,host_string(h,host_u64(h,p+32),value,sizeof(value))&&!strcmp(value,"USER=native"),"environment pointer width differs");
 message(h,"PASS: 64-bit iovecs, offsets above 4 GiB, stat layout, clocks, environment and invalid-buffer errors.");
}
static void assets(struct w2c_env *h,uint64_t scratch) {
 const char *paths[]={"data/common/data/levels/gta5/paths.xml","data/x64/anim/cutscene/cuts_prologue.rpf","data/x64/levels/gta5/_prologue/plg_01.rpf"};
 unsigned char expected[65536],actual[65536];
 for(unsigned i=0;i<3;i++){
  uint64_t f=open_engine(h,scratch,paths[i],"rb");require(h,f!=0,"selected folder: engine fopen sample failed");
  // Open a second descriptor through the same rooted, no-symlink resolver.
  putstr(h,scratch,paths[i]);uint32_t fd=host_open(h,(uint32_t)-100,scratch,0,0);require(h,(int32_t)fd>=3,"sample comparison open failed");
  struct stat st;require(h,fstat(h->files[fd].fd,&st)==0&&st.st_size>=65536,"sample size/read failed");
  require(h,w2c_gta_native_probe_fseeko(h->engine,f,0,SEEK_END)==0&&w2c_gta_native_probe_ftello(h->engine,f)==(uint64_t)st.st_size,"sample engine file size differs");
  for(unsigned end=0;end<2;end++){
   uint64_t offset=end?(uint64_t)st.st_size-65536:0;
   require(h,pread(h->files[fd].fd,expected,sizeof(expected),(off_t)offset)==sizeof(expected),"sample direct read failed");
   require(h,w2c_gta_native_probe_fseeko(h->engine,f,offset,SEEK_SET)==0,"sample engine seek failed");
   require(h,w2c_gta_native_probe_fread(h->engine,scratch+8192,1,sizeof(actual),f)==sizeof(actual),"sample engine read failed");
   host_load(h,actual,scratch+8192,sizeof(actual));require(h,!memcmp(expected,actual,sizeof(actual)),"sample engine read differs from file");
  }
  require(h,host_fd_close(h,fd)==0,"sample comparison close failed");w2c_gta_native_probe_fclose(h->engine,f);
  message(h,"PASS: engine file reads at beginning and end: %s",paths[i]);
 }
}
int native_runtime_probe(const char *game_folder,const char *save_folder,native_progress progress,void *context,char *report,size_t capacity) {
 wasm_rt_init();
 struct w2c_env *h=calloc(1,sizeof(*h));w2c_gta *engine=calloc(1,sizeof(*engine));
 if(!h||!engine){free(h);free(engine);snprintf(report,capacity,"FAIL: host allocation");return 1;}
 host_init(h);h->engine=engine;h->progress=progress;h->progress_context=context;
 struct w2c_wasi__snapshot__preview1 wasi={.env=h};
 volatile bool allocated=false,instantiated=false;
 NativeThreads *volatile threads=NULL;
 NativeAssets *volatile assets_service=NULL;
 wasm_rt_trap_t trap=wasm_rt_impl_try();
 if(!trap){
  require(h,save_folder&&*save_folder,"missing app-private save folder");
  h->save_root=open(save_folder,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);require(h,h->save_root>=0,"save folder unavailable");
  if(game_folder){h->game_root=open(game_folder,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);require(h,h->game_root>=0,"selected folder permission unavailable");}
  threads=native_threads_begin(h);
  message(h,"Starting real engine constructors and main-thread TLS…");
  wasm_rt_allocate_memory_shared(&h->memory,8192,65536,false,65536);allocated=true;
  wasm2c_gta_instantiate(engine,h,&wasi);instantiated=true;
  w2c_gta_0x5F_wasm_call_ctors(engine);
  require(h,w2c_gta_pthread_self(engine)!=0&&h->mailbox==w2c_gta_pthread_self(engine),"main thread TLS/mailbox not initialized");
  w2c_gta_0x5Femscripten_check_mailbox(engine);
  message(h,"PASS: real engine constructors and main-thread TLS. Heap committed: %.1f MiB.",(double)atomic_load(&h->memory.size)/1048576.0);
#ifdef __APPLE__
  task_vm_info_data_t info={0};mach_msg_type_number_t count=TASK_VM_INFO_COUNT;
  if(task_info(mach_task_self(),TASK_VM_INFO,(task_info_t)&info,&count)==KERN_SUCCESS)
   message(h,"Process footprint with initialized engine alive: %.1f MiB (sample, not peak).",(double)info.phys_footprint/1048576.0);
#endif
  uint64_t stack=w2c_gta_emscripten_stack_get_current(engine);
  uint64_t scratch=w2c_gta_0x5Femscripten_stack_alloc(engine,80*1024);
  saves(h,scratch);abi(h,scratch);
  if(game_folder)assets(h,scratch);else message(h,"SKIP: actual game-file reads; select the extracted web folder to include them.");
  require(h,h->failure==NULL,"native file close reported an error");
  require(h,game_folder!=NULL,"select the extracted web folder for the asset checkpoint");
  assets_service=native_assets_begin(h);native_set_assets(assets_service);
  native_assets_probe(assets_service,scratch);
  native_threads_probe(h,scratch);
  w2c_gta_0x5Femscripten_stack_restore(engine,stack);
  message(h,"Entering CPU startup with real game workers; stopping at the first archive read…");
  native_main_checkpoint(h);
 }else if(native_main_reached(h)){
  snprintf(report,capacity,"PASS: native 0.4 asset/worker checkpoint — real engine queued file reads, concurrent requests, persistent saves and native thread lifecycle.\nPASS: actual main launched two game workers and reached its first archive read; stopped deliberately and joined workers.\nArchive/scene loading, Metal renderer, audio output and gameplay remain pending.");trap=WASM_RT_TRAP_NONE;
 }else snprintf(report,capacity,"FAIL: runtime trap %d at %s.\nThis checkpoint has not reached gameplay.",trap,h->failure?h->failure:"engine instruction / memory allocation");
 native_threads_end(threads);
 native_assets_end(assets_service);
 host_close(h);
 // This is only our disposable sparse probe. The regular probe save persists.
 if(save_folder){int root=open(save_folder,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(root>=0){unlinkat(root,".native-sparse-probe",0);close(root);}}
 if(instantiated)wasm2c_gta_free(engine);
 if(allocated){wasm_rt_free_memory_shared(&h->memory);pthread_mutex_destroy(&h->memory.mem_lock);}
 free(engine);free(h);wasm_rt_free_thread();return trap?1:0;
}
