#include "native_host.h"
#ifndef NATIVE_HOST_UNIT
#include "diagnostics/native_journal.h"
#endif
#include "platform/native_paths.h"
#include "wasm-rt-impl.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <limits.h>
#include <stdlib.h>

// Shared descriptors are serialized. Never hold these locks across engine calls.
static pthread_mutex_t file_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t grow_mutex=PTHREAD_MUTEX_INITIALIZER;
static void lock(pthread_mutex_t *m){if(pthread_mutex_lock(m))abort();}
static void unlock(pthread_mutex_t *m){if(pthread_mutex_unlock(m))abort();}
#ifndef NATIVE_HOST_UNIT
#include "threading/native_pump.h"
void native_thread_error(struct w2c_env*,const char*);
#endif
static uint32_t wasi_errno(int e) {
 switch(e){
 case 0:return 0;case EACCES:return 2;case EAGAIN:return 6;case EBADF:return 8;
 case EEXIST:return 20;case EFAULT:return 21;case EFBIG:return 22;case EINTR:return 27;
 case EINVAL:return 28;case EIO:return 29;case EISDIR:return 31;case ELOOP:return 32;
 case EMFILE:return 33;case ENAMETOOLONG:return 37;case ENOENT:return 44;
 case ENOMEM:return 48;case ENOSPC:return 51;case ENOSYS:return 52;
 case ENOTDIR:return 54;case ENOTEMPTY:return 55;case ENOTSUP:return 58;
 case EPERM:return 63;case EROFS:return 69;case ESPIPE:return 70;case EXDEV:return 75;
 default:return 29;
 }
}
bool host_range(struct w2c_env *h,uint64_t p,uint64_t n) {
 uint64_t size=atomic_load_explicit(&h->memory.size,memory_order_acquire);
 return p<=size&&n<=size-p;
}
void host_load(struct w2c_env *h,void *out,uint64_t p,size_t n) {
 if(!host_range(h,p,n))wasm_rt_trap(WASM_RT_TRAP_OOB);
 unsigned char *b=out;
 for(size_t i=0;i<n;i++)b[i]=atomic_load_explicit(h->memory.data+p+i,memory_order_relaxed);
}
void host_store(struct w2c_env *h,uint64_t p,const void *in,size_t n) {
 if(!host_range(h,p,n))wasm_rt_trap(WASM_RT_TRAP_OOB);
 const unsigned char *b=in;
 for(size_t i=0;i<n;i++)atomic_store_explicit(h->memory.data+p+i,b[i],memory_order_relaxed);
}
uint64_t host_u64(struct w2c_env *h,uint64_t p){uint64_t v;host_load(h,&v,p,8);return v;}
void host_put64(struct w2c_env *h,uint64_t p,uint64_t v){host_store(h,p,&v,8);}
void host_put32(struct w2c_env *h,uint64_t p,uint32_t v){host_store(h,p,&v,4);}
bool host_string(struct w2c_env *h,uint64_t p,char *s,size_t cap) {
 for(size_t i=0;i<cap;i++){
  if(p>UINT64_MAX-i||!host_range(h,p+i,1))return false;
  s[i]=atomic_load_explicit(h->memory.data+p+i,memory_order_relaxed);
  if(!s[i])return true;
 }
 return false;
}
static uint64_t ns(clockid_t clock){struct timespec t;if(clock_gettime(clock,&t))return 0;return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
void host_init(struct w2c_env *h) {
 if(pthread_mutex_init(&h->memory_report_lock,NULL))abort();
 for(unsigned i=0;i<HOST_FDS;i++)h->files[i].fd=-1;
 h->game_root=h->save_root=-1;
 h->mono_origin=ns(CLOCK_MONOTONIC);h->wall_origin=ns(CLOCK_REALTIME);
}
void host_close(struct w2c_env *h) {
 if(pthread_mutex_destroy(&h->memory_report_lock))abort();
 for(unsigned i=3;i<HOST_FDS;i++)if(h->files[i].fd>=0)close(h->files[i].fd);
 if(h->game_root>=0)close(h->game_root);
 if(h->save_root>=0)close(h->save_root);
}
static void host_log_preview(struct w2c_env *h,const char *line) {
#ifndef NATIVE_HOST_UNIT
 if(native_thread_log(h,line))return;
#endif
 // Constructor/checkpoint output is brief; real startup uses the bounded tail.
 lock(&log_mutex); if(h->progress)h->progress(h->progress_context,line); unlock(&log_mutex);
}
void host_log(struct w2c_env *h,const char *line) {
#ifndef NATIVE_HOST_UNIT
 native_journal_capture("engine",line,strlen(line));
#endif
 host_log_preview(h,line);
}
_Noreturn void host_unavailable(struct w2c_env *h,const char *name) {
 #ifdef NATIVE_HOST_UNIT
 if(h)h->failure=name;
#else
 if(h)native_thread_error(h,name);
#endif
 wasm_rt_trap(WASM_RT_TRAP_UNREACHABLE);
}
static HostFD *file(struct w2c_env *h,uint32_t fd){return fd>=3&&fd<HOST_FDS&&h->files[fd].fd>=0?&h->files[fd]:NULL;}
// Walk each component relative to a granted root. Never follow symlinks or
// expose arbitrary app/OS paths. Linux open flags are translated, not reused.
static uint32_t unlocked_open_text(struct w2c_env *h,uint32_t dirfd,char *text,uint32_t flags) {
 if((int32_t)dirfd!=-100)return (uint32_t)-58;
 char *p=text;if(*p=='/')p++;
 // The fixed engine's wc_InitRng reads a 52-byte seed from these exact
 // virtual devices. Bind both to the OS cryptographic source, read-only;
 // this does not permit arbitrary native paths under /dev (or elsewhere).
 if(!strcmp(p,"dev/urandom")||!strcmp(p,"dev/random")){
  if(flags&(3u|64u|128u|512u|1024u))return (uint32_t)-69;
  if(flags&~(32768u|524288u))return (uint32_t)-28;
  unsigned slot=3;while(slot<HOST_FDS&&h->files[slot].fd>=0)slot++;
  if(slot==HOST_FDS)return (uint32_t)-33;
  int fd=open("/dev/urandom",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
  if(fd<0)return (uint32_t)-wasi_errno(errno);
  h->files[slot]=(HostFD){fd,false};return slot;
 }
 int root;bool readonly;
 bool root_only=!strcmp(p,"userdata");
 if(!strncmp(p,"data/",5)){root=h->game_root;readonly=true;}
 else if(root_only){root=h->save_root;readonly=false;p+=8;}
 else if(!strncmp(p,"userdata/",9)){root=h->save_root;readonly=false;p+=9;}
 else return (uint32_t)-44;
 if(root<0)return (uint32_t)-44;
 const uint32_t supported=3u|64u|128u|512u|1024u|32768u|65536u|131072u|524288u;
 if((flags&~supported)||(flags&3u)==3)return (uint32_t)-28;
 bool writes=(flags&3u)!=0||(flags&(64|512|1024));
 if(readonly&&writes)return (uint32_t)-69;
 unsigned slot=3;while(slot<HOST_FDS&&h->files[slot].fd>=0)slot++;
 if(slot==HOST_FDS)return (uint32_t)-33;
 if(root_only){
  if(writes)return (uint32_t)-31;
  int fd=dup(root);if(fd<0)return (uint32_t)-wasi_errno(errno);
  h->files[slot]=(HostFD){fd,false};return slot;
 }
 int parent=dup(root);if(parent<0)return (uint32_t)-wasi_errno(errno);
 uint32_t result=0;
 for(;;){
  char *slash=strchr(p,'/');if(slash)*slash=0;
  if(!*p||!strcmp(p,".")||!strcmp(p,"..")){result=(uint32_t)-63;break;}
  if(slash){
   int next=openat(parent,p,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
   if(next<0){result=(uint32_t)-wasi_errno(errno);break;}
   close(parent);parent=next;p=slash+1;continue;
  }
  int native=(flags&3)==0?O_RDONLY:(flags&3)==1?O_WRONLY:O_RDWR;
  if(flags&64)native|=O_CREAT;if(flags&128)native|=O_EXCL;
  if(flags&512)native|=O_TRUNC;if(flags&1024)native|=O_APPEND;
  if(flags&65536)native|=O_DIRECTORY;
  int fd=openat(parent,p,native|O_NOFOLLOW|O_CLOEXEC,0600);
  if(fd<0)result=(uint32_t)-wasi_errno(errno);
  else{h->files[slot]=(HostFD){fd,!readonly&&(flags&3)!=0};result=slot;}
  break;
 }
 close(parent);return result;
}
static uint32_t unlocked_open(struct w2c_env *h,uint32_t dirfd,uint64_t path,uint32_t flags,uint64_t modeptr) {
 (void)modeptr;
 char text[4096];if(!host_string(h,path,text,sizeof(text)))return (uint32_t)-21;
 return unlocked_open_text(h,dirfd,text,flags);
}
static uint32_t unlocked_fd_close(struct w2c_env *h,uint32_t fd) {
 HostFD *f=file(h,fd);if(!f)return 8;
 int native=f->fd;f->fd=-1;
 // Save data is synced before closing so a successful fclose is meaningful.
 int sync_error=0;if(f->writable&&fsync(native))sync_error=errno;
 int rc=close(native);if(sync_error||rc)h->failure="native file sync/close failed";return wasi_errno(sync_error?sync_error:rc?errno:0);
}
static uint32_t unlocked_io(struct w2c_env *h,uint32_t fd,uint64_t iov,uint64_t count,uint64_t out,bool write_op,bool positional,uint64_t offset) {
 HostFD *f=file(h,fd);bool log=write_op&&!positional&&(fd==1||fd==2);
 if(!f&&!log)return 8;
 if(f&&write_op&&!f->writable)return 8;
 if(count>1024||!host_range(h,iov,count*16)||!host_range(h,out,8))return 21;
 if(positional&&offset>INT64_MAX)return 28;
 // Validate the whole vector before an operation changes any file position.
 for(uint64_t i=0;i<count;i++)if(!host_range(h,host_u64(h,iov+16*i),host_u64(h,iov+16*i+8)))return 21;
 unsigned char scratch[65536];uint64_t total=0;uint32_t error=0;
 for(uint64_t i=0;i<count;i++){
  uint64_t p=host_u64(h,iov+16*i),remaining=host_u64(h,iov+16*i+8);
  while(remaining){
   size_t n=remaining>sizeof(scratch)?sizeof(scratch):(size_t)remaining;
   if(positional&&(total>INT64_MAX-offset||n>(uint64_t)INT64_MAX-offset-total)){error=28;goto done;}
   if(write_op)host_load(h,scratch,p,n);
   ssize_t got;
   if(log){
#ifndef NATIVE_HOST_UNIT
    native_journal_capture(NULL,scratch,n); /* Preserve every original stdout byte. */
#endif
    char line[513];size_t take=n<512?n:512;memcpy(line,scratch,take);line[take]=0;host_log_preview(h,line);got=(ssize_t)n;}
   else do{
    got=write_op?(positional?pwrite(f->fd,scratch,n,(off_t)(offset+total)):write(f->fd,scratch,n)):
                  (positional?pread(f->fd,scratch,n,(off_t)(offset+total)):read(f->fd,scratch,n));
   }while(got<0&&errno==EINTR);
   if(got<0){error=wasi_errno(errno);goto done;}
   if(!write_op&&got)host_store(h,p,scratch,(size_t)got);
   p+=(uint64_t)got;remaining-=(uint64_t)got;total+=(uint64_t)got;
   if((size_t)got<n)goto done;
  }
 }
done:host_put64(h,out,total);return total?0:error;
}
static uint32_t unlocked_seek(struct w2c_env *h,uint32_t fd,uint64_t offset,uint32_t whence,uint64_t out) {
 HostFD *f=file(h,fd);if(!f)return 8;if(whence>2)return 28;if(!host_range(h,out,8))return 21;
 off_t p=lseek(f->fd,(off_t)(int64_t)offset,whence==0?SEEK_SET:whence==1?SEEK_CUR:SEEK_END);
 if(p<0)return wasi_errno(errno);host_put64(h,out,(uint64_t)p);return 0;
}
static uint32_t unlocked_stat(struct w2c_env *h,uint32_t fd,uint64_t p) {
 HostFD *f=file(h,fd);if(!f)return (uint32_t)-8;if(!host_range(h,p,104))return (uint32_t)-21;
 struct stat s;if(fstat(f->fd,&s))return (uint32_t)-wasi_errno(errno);
 unsigned char zero[104]={0};host_store(h,p,zero,sizeof(zero));
 host_put32(h,p,(uint32_t)s.st_dev);
 uint32_t mode=(S_ISDIR(s.st_mode)?0040000u:S_ISCHR(s.st_mode)?0020000u:0100000u)|(s.st_mode&0777u);
 host_put32(h,p+4,mode);host_put64(h,p+8,s.st_nlink);
 host_put32(h,p+16,s.st_uid);host_put32(h,p+20,s.st_gid);host_put32(h,p+24,(uint32_t)s.st_rdev);
 host_put64(h,p+32,(uint64_t)s.st_size);host_put32(h,p+40,(uint32_t)s.st_blksize);host_put32(h,p+44,(uint32_t)s.st_blocks);
#if defined(__APPLE__)
 const struct timespec times[3]={s.st_atimespec,s.st_mtimespec,s.st_ctimespec};
#else
 const struct timespec times[3]={s.st_atim,s.st_mtim,s.st_ctim};
#endif
 for(unsigned i=0;i<3;i++){host_put64(h,p+48+i*16,times[i].tv_sec);host_put64(h,p+56+i*16,times[i].tv_nsec);}
 host_put64(h,p+96,s.st_ino);return 0;
}
static uint32_t unlocked_resize(struct w2c_env *h,uint64_t size) {
 if(size>4294967296ull)return 0;
 uint64_t wanted=(size+65535)/65536,current=atomic_load(&h->memory.pages);
 if(wanted<=current)return 1;
 return wasm_rt_grow_memory_shared(&h->memory,wanted-current)!=UINT64_MAX;
}
double host_now(struct w2c_env *h,bool wall) {
#ifndef NATIVE_HOST_UNIT
 native_thread_tick(h);
#endif
 return (double)(wall?ns(CLOCK_REALTIME):h->wall_origin+(ns(CLOCK_MONOTONIC)-h->mono_origin))/1000000.0;
}
uint32_t host_clock(struct w2c_env *h,uint32_t id,uint64_t p) {
 if(id>3)return 28;if(!host_range(h,p,8))return 21;
 clockid_t clock=id==0?CLOCK_REALTIME:id==1?CLOCK_MONOTONIC:id==2?CLOCK_PROCESS_CPUTIME_ID:CLOCK_THREAD_CPUTIME_ID;
 struct timespec t;if(clock_gettime(clock,&t))return wasi_errno(errno);
 host_put64(h,p,(uint64_t)t.tv_sec*1000000000ull+t.tv_nsec);return 0;
}
uint32_t host_environment(struct w2c_env *h,uint64_t a,uint64_t b,bool sizes) {
 // Deliberately fixed values: do not expose the native process environment.
 static const char values[]="USER=native\0HOME=/userdata\0LANG=en_US.UTF-8\0";
 const uint64_t count=3,bytes=sizeof(values)-1;
 if(sizes){if(!host_range(h,a,8)||!host_range(h,b,8))return 21;host_put64(h,a,count);host_put64(h,b,bytes);}
 else{if(!host_range(h,a,count*8)||!host_range(h,b,bytes))return 21;host_store(h,b,values,bytes);uint64_t p=b;for(unsigned i=0;i<count;i++){host_put64(h,a+8*i,p);p+=strlen(values+(p-b))+1;}}
 return 0;
}

uint32_t host_open(struct w2c_env *h,uint32_t d,uint64_t p,uint32_t f,uint64_t m){lock(&file_mutex);uint32_t r=unlocked_open(h,d,p,f,m);unlock(&file_mutex);return r;}
uint32_t host_fd_close(struct w2c_env *h,uint32_t fd){lock(&file_mutex);uint32_t r=unlocked_fd_close(h,fd);unlock(&file_mutex);return r;}
uint32_t host_io(struct w2c_env *h,uint32_t fd,uint64_t v,uint64_t n,uint64_t out,bool wr,bool pos,uint64_t off){lock(&file_mutex);uint32_t r=unlocked_io(h,fd,v,n,out,wr,pos,off);unlock(&file_mutex);return r;}
uint32_t host_seek(struct w2c_env *h,uint32_t fd,uint64_t off,uint32_t whence,uint64_t out){lock(&file_mutex);uint32_t r=unlocked_seek(h,fd,off,whence,out);unlock(&file_mutex);return r;}
uint32_t host_stat(struct w2c_env *h,uint32_t fd,uint64_t p){lock(&file_mutex);uint32_t r=unlocked_stat(h,fd,p);unlock(&file_mutex);return r;}
uint32_t host_resize(struct w2c_env *h,uint64_t size){lock(&grow_mutex);uint32_t r=unlocked_resize(h,size);unlock(&grow_mutex);return r;}

uint32_t host_path_stat(struct w2c_env *h,uint64_t path,uint64_t out){
 if(!host_range(h,out,104))return (uint32_t)-21;
 lock(&file_mutex);uint32_t fd=unlocked_open(h,(uint32_t)-100,path,0,0),result=fd;
 if((int32_t)fd>=0){result=unlocked_stat(h,fd,out);unlocked_fd_close(h,fd);}
 unlock(&file_mutex);return result;
}
uint32_t host_statfs(struct w2c_env *h,uint64_t path,uint64_t size,uint64_t out){
 // This engine's memory64 libc passes a 120-byte statfs, not Apple's struct.
 // Validate before I/O and leave the caller's bytes unchanged on failure.
 if(size!=120)return (uint32_t)-28;
 if(!path||!out||!host_range(h,out,120))return (uint32_t)-21;
 char text[4096];if(!host_string(h,path,text,sizeof(text)))return (uint32_t)-21;
 size_t length=strlen(text);bool directory=length&&text[length-1]=='/';
 while(length>1&&text[length-1]=='/')text[--length]=0;
 const char *relative=text[0]=='/'?text+1:text;
 if(!strncmp(relative,"dev/",4))return (uint32_t)-44; // entropy devices are not game/save volumes
 bool readonly=!strcmp(relative,"data")||!strncmp(relative,"data/",5);
 struct statvfs stats;uint32_t result=0,slot=0;int fd=-1;bool close_fd=false;
 lock(&file_mutex);
 // The virtual root/cwd reports the app's save volume. No engine string is
 // interpreted as an absolute native path; data remains the granted folder.
 if(!strcmp(text,"/")||!strcmp(text,".")||!strcmp(text,"/.")||!strcmp(relative,"userdata")){
  fd=h->save_root;if(fd<0)result=(uint32_t)-44;
 }else if(!strcmp(relative,"data")){
  if(h->game_root<0)result=(uint32_t)-44;
  else{fd=openat(h->game_root,"data",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close_fd=fd>=0;if(fd<0)result=(uint32_t)-wasi_errno(errno);}
 }else{
  slot=unlocked_open_text(h,(uint32_t)-100,text,directory?65536:0);
  if((int32_t)slot<0)result=slot;else fd=h->files[slot].fd;
 }
 if(!result&&fstatvfs(fd,&stats))result=(uint32_t)-wasi_errno(errno);
 if(slot>=3&&slot<HOST_FDS)unlocked_fd_close(h,slot);
 if(close_fd)close(fd);
 unlock(&file_mutex);
 if(result)return result;
 // Pack every byte explicitly, including zeroed padding/reserved fields.
 // Block counts use f_frsize; preserve it separately from optimal I/O size.
 uint64_t words[15]={0};
 words[1]=stats.f_bsize;words[2]=stats.f_blocks;words[3]=stats.f_bfree;
 words[4]=stats.f_bavail;words[5]=stats.f_files;words[6]=stats.f_ffree;
 words[7]=(uint64_t)stats.f_fsid;words[8]=stats.f_namemax;
 words[9]=stats.f_frsize?stats.f_frsize:stats.f_bsize;
 words[10]=(readonly||(stats.f_flag&ST_RDONLY)?1u:0u)|(stats.f_flag&ST_NOSUID?2u:0u);
 host_store(h,out,words,sizeof(words));return 0;
}
uint32_t host_mkdir(struct w2c_env *h,uint32_t dirfd,uint64_t path,uint32_t mode){
 (void)mode;char text[4096];if(!host_string(h,path,text,sizeof(text)))return (uint32_t)-21;
 if((int32_t)dirfd!=-100)return (uint32_t)-58;
 char *part=text;if(*part=='/')part++;
 if(!strcmp(part,"userdata"))return h->save_root>=0?(uint32_t)-20:(uint32_t)-44;
 if(strncmp(part,"userdata/",9))return (uint32_t)-69;
 part+=9;int dir=dup(h->save_root);if(dir<0)return (uint32_t)-wasi_errno(errno);
 uint32_t result=0;
 for(;;){char *slash=strchr(part,'/');if(slash)*slash=0;
  if(!*part||!strcmp(part,".")||!strcmp(part,"..")){result=(uint32_t)-63;break;}
  if(!slash){if(mkdirat(dir,part,0700))result=(uint32_t)-wasi_errno(errno);break;}
  int next=openat(dir,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
  if(next<0){result=(uint32_t)-wasi_errno(errno);break;}close(dir);dir=next;part=slash+1;
 }
 close(dir);return result;
}
