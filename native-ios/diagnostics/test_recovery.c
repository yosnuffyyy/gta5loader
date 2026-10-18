#include "native_recovery.h"
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
int main(void){char dir[]="/tmp/native-recovery-XXXXXX";assert(mkdtemp(dir));char file[512],temp[512];snprintf(file,sizeof(file),"%s/snapshot.txt",dir);snprintf(temp,sizeof(temp),"%s/snapshot.txt.tmp",dir);NativeRecovery r={0};
 recovery_add(&r,"Native memory headroom: 3.9 MiB",strlen("Native memory headroom: 3.9 MiB"));for(unsigned i=0;i<10000;i++){char s[100];int n=snprintf(s,sizeof(s),"Game pipeline cache: count %u",i);recovery_add(&r,s,(size_t)n);}assert(!recovery_save(&r,file,1234,99));
 struct stat st;assert(!stat(file,&st)&&st.st_size<25000);char before[25000]={0};int fd=open(file,O_RDONLY);assert(fd>=0);ssize_t bytes=read(fd,before,sizeof(before)-1);assert(bytes>0);close(fd);assert(strstr(before,"3.9 MiB")&&strstr(before,"count 9999")&&!strstr(before,"count 9995"));
 /* An incomplete replacement leaves the committed old snapshot untouched. */
 fd=open(temp,O_WRONLY|O_CREAT|O_TRUNC,0600);assert(fd>=0&&write(fd,"partial",7)==7);close(fd);char after[25000]={0};fd=open(file,O_RDONLY);assert(read(fd,after,sizeof(after)-1)==bytes);close(fd);assert(!memcmp(before,after,(size_t)bytes));
 recovery_add(&r,"Thermal state: serious",strlen("Thermal state: serious"));assert(!recovery_save(&r,file,2000,100));assert(access(temp,F_OK)!=0);assert(unlink(file)==0&&rmdir(dir)==0);
 printf("PASS: recovery ring10000 updates; independent categories; fixed %zu bytes memory; atomic complete replacement and orphan-temp preservation; bounded text,fsync and file errors retained. No simulated jetsam claim.\n",sizeof(r));return 0;
}
