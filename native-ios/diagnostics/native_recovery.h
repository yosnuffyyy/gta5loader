#ifndef NATIVE_RECOVERY_H
#define NATIVE_RECOVERY_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
enum { RECOVERY_KINDS=8,RECOVERY_ROWS=4,RECOVERY_WIDTH=768 };
typedef struct {uint64_t count[RECOVERY_KINDS];char rows[RECOVERY_KINDS][RECOVERY_ROWS][RECOVERY_WIDTH];} NativeRecovery;
/* Caller serializes updates. Preserve each diagnostic category independently
   so repetitive graphics summaries cannot evict the last memory/stream event. */
static inline void recovery_add(NativeRecovery *r,const char *bytes,size_t n){
 char line[RECOVERY_WIDTH];if(n>=sizeof(line)){n=sizeof(line)-1;while(n&&((unsigned char)bytes[n]&0xc0)==0x80)n--;}memcpy(line,bytes,n);line[n]=0;
 int k=-1;if(strstr(line,"memory headroom")||strstr(line,"Native startup: process")||strstr(line,"[wasm] mem:"))k=0;
 else if(strstr(line,"[streaming]")||strstr(line,"Streaming allocation")||strstr(line,"Asset reads:"))k=1;
 else if(strstr(line,"Game pipeline cache")||strstr(line,"Pipeline live/peak")||strstr(line,"Resource census:"))k=2;
 else if(strstr(line,"Live startup:")||strstr(line,"GPU breadcrumb")||strstr(line,"Native GPU:"))k=3;
 else if(strstr(line,"Thermal state:")||strstr(line,"Input:"))k=4;
 else if(strstr(line,"Actual display intervals:")||strstr(line,"User-marked"))k=5;
 else if(strstr(line,"Session transition")||strstr(line,"SCRIPT[")||strstr(line,"frame profile"))k=6;
 else if(strstr(line,"FAIL")||strstr(line,"Fatal")||strstr(line,"fatal")||strstr(line,"trap")||strstr(line,"Exit:"))k=7;
 if(k>=0){unsigned at=(unsigned)(r->count[k]++%RECOVERY_ROWS);memcpy(r->rows[k][at],line,n+1);}
}
static inline int recovery_write_all(int fd,const void *bytes,size_t n){
 const char *p=bytes;while(n){ssize_t v=write(fd,p,n);if(v<0&&errno==EINTR)continue;if(v<=0)return v<0?errno:EIO;p+=v;n-=(size_t)v;}return 0;
}
/* Same-directory atomic replacement: incomplete temp files never replace the
   last complete snapshot. No signal handlers, unbounded files or Foundation. */
static inline int recovery_save(const NativeRecovery *r,const char *path,uint64_t written,uint64_t stamp){
 char temp[4096];int len=snprintf(temp,sizeof(temp),"%s.tmp",path);if(len<0||(size_t)len>=sizeof(temp))return ENAMETOOLONG;
 int fd=open(temp,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return errno;
 char header[320];int n=snprintf(header,sizeof(header),"Native recovery snapshot v1; monotonic ns %llu; full log written prefix %llu bytes. Bounded recent categories; not a termination diagnosis.\n",(unsigned long long)stamp,(unsigned long long)written);
 int error=recovery_write_all(fd,header,(size_t)n);
 for(unsigned k=0;k<RECOVERY_KINDS&&!error;k++){unsigned count=r->count[k]<RECOVERY_ROWS?(unsigned)r->count[k]:RECOVERY_ROWS;for(unsigned i=0;i<count&&!error;i++){const char *s=r->rows[k][(r->count[k]-count+i)%RECOVERY_ROWS];error=recovery_write_all(fd,s,strlen(s));if(!error)error=recovery_write_all(fd,"\n",1);}}
 if(!error&&fsync(fd))error=errno;if(close(fd)&&!error)error=errno;
 if(!error&&rename(temp,path))error=errno;if(error)unlink(temp);return error;
}
#endif
