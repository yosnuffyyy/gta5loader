#include "native_host.h"
#include "platform/native_paths.h"
#include "wasm-rt-impl.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/statvfs.h>
static uint32_t open_path(struct w2c_env *h,const char *path,unsigned flags){host_store(h,64,path,strlen(path)+1);return host_open(h,(uint32_t)-100,64,flags,0);}
static void capacity_tests(struct w2c_env *h){
 unsigned char guard[136],actual[136];memset(guard,0xa5,sizeof(guard));
 struct statvfs expected;assert(fstatvfs(h->save_root,&expected)==0);
 const char *roots[]={"/userdata","userdata/","/",".","/.","/userdata/.native-statfs"};
 uint32_t fd=open_path(h,roots[5],2|64|512);assert((int32_t)fd>=3);assert(host_fd_close(h,fd)==0);
 for(unsigned repeat=0;repeat<100;repeat++)for(unsigned i=0;i<6;i++){
  host_store(h,64,roots[i],strlen(roots[i])+1);host_store(h,8192,guard,sizeof(guard));
  assert(host_statfs(h,64,120,8200)==0);host_load(h,actual,8192,sizeof(actual));
  assert(!memcmp(guard,actual,8)&&!memcmp(guard+128,actual+128,8));
  assert(host_u64(h,8208)==expected.f_bsize&&host_u64(h,8216)==expected.f_blocks);
  assert(host_u64(h,8272)==(expected.f_frsize?expected.f_frsize:expected.f_bsize));
  assert(host_u64(h,8264)==expected.f_namemax);
  for(unsigned j=88;j<120;j++)assert(actual[8+j]==0);
 }
 // A root query does not require opening a descriptor slot. The named-file
 // loop also catches descriptor leaks long before reaching the 64-slot bound.
 for(unsigned i=3;i<HOST_FDS;i++)assert(h->files[i].fd==-1);
 const char *data[]={"/data","/data/","data/common/data/levels/gta5/paths.xml"};
 for(unsigned i=0;i<3;i++){
  host_store(h,64,data[i],strlen(data[i])+1);assert(host_statfs(h,64,120,8200)==0);assert(host_u64(h,8280)&1);
 }
 const char *bad[]={"/userdata/../web","/userdata/link-file","/data/escape/anything","/etc","","/userdata/absent","/userdata/.native-statfs/"};
 for(unsigned i=0;i<7;i++){
  host_store(h,64,bad[i],strlen(bad[i])+1);host_store(h,8192,guard,sizeof(guard));
  assert((int32_t)host_statfs(h,64,120,8200)<0);host_load(h,actual,8192,sizeof(actual));assert(!memcmp(guard,actual,sizeof(guard)));
 }
 host_store(h,64,"/userdata",10);host_store(h,8192,guard,sizeof(guard));
 assert(host_statfs(h,64,119,8200)==(uint32_t)-28);
 assert(host_statfs(h,64,121,8200)==(uint32_t)-28);
 assert(host_statfs(h,64,UINT64_MAX,8200)==(uint32_t)-28);
 assert(host_statfs(h,UINT64_MAX,120,8200)==(uint32_t)-21);
 assert(host_statfs(h,0,120,8200)==(uint32_t)-21);
 assert(host_statfs(h,64,120,0)==(uint32_t)-21);
 assert(host_statfs(h,64,120,UINT64_MAX-60)==(uint32_t)-21);
 assert(host_statfs(h,64,120,atomic_load(&h->memory.size)-119)==(uint32_t)-21);
 host_load(h,actual,8192,sizeof(actual));assert(!memcmp(guard,actual,sizeof(guard)));
 assert(unlinkat(h->save_root,".native-statfs",0)==0);
 puts("PASS: native filesystem capacity ABI, live OS values, guarded output, size/pointer errors, rooted paths, symlink refusal and descriptor cleanup.");
}
int main(void){
 wasm_rt_init();struct w2c_env h={0};host_init(&h);
 wasm_rt_allocate_memory_shared(&h.memory,2,4,false,65536);
 h.game_root=open("native-build/fixtures/web",O_RDONLY|O_DIRECTORY);
 h.save_root=open("native-build/fixtures/saves",O_RDONLY|O_DIRECTORY);
 assert(h.game_root>=0&&h.save_root>=0);
 capacity_tests(&h);
 // Exercise the same 52-byte seed read as the real engine. These checks
 // establish bridge behavior; they are not a statistical proof of entropy.
 for(unsigned i=0;i<2;i++){
  uint32_t rng=open_path(&h,i?"/dev/random":"/dev/urandom",0);assert((int32_t)rng>=3);
  host_put64(&h,512,4096);host_put64(&h,520,52);
  assert(host_io(&h,rng,512,1,2048,false,false,0)==0&&host_u64(&h,2048)==52);
  assert(host_io(&h,rng,512,1,2048,true,false,0)==8);
  assert(host_fd_close(&h,rng)==0&&host_fd_close(&h,rng)==8);
 }
 assert((int32_t)open_path(&h,"/dev/urandom",1)==-69);
 assert((int32_t)open_path(&h,"/dev/random",64)==-69);
 assert((int32_t)open_path(&h,"/dev/urandom/../other",0)==-44);
 assert((int32_t)open_path(&h,"/dev/zero",0)==-44);
 puts("PASS: read-only native entropy device seed reads, denied writes, exact device paths and close cleanup.");
 host_store(&h,64,"/userdata",10);assert(host_path_stat(&h,64,8192)==0);
 host_store(&h,64,"/userdata/path-test",20);
 assert(host_mkdir(&h,(uint32_t)-100,64,0777)==0);
 assert(host_path_stat(&h,64,8192)==0);
 assert(host_mkdir(&h,(uint32_t)-100,64,0777)==(uint32_t)-20);
 assert(unlinkat(h.save_root,"path-test",AT_REMOVEDIR)==0);
 host_store(&h,64,"/userdata/../escape",sizeof("/userdata/../escape"));assert(host_mkdir(&h,(uint32_t)-100,64,0777)==(uint32_t)-63);
 host_store(&h,64,"/data/escape",13);assert(host_mkdir(&h,(uint32_t)-100,64,0777)==(uint32_t)-69);
 assert((int32_t)open_path(&h,"/data/new-file",64|1)==-69);
 assert((int32_t)open_path(&h,"/userdata/../web/evil",64|1)==-63);
 assert((int32_t)open_path(&h,"/userdata/link-file",0)<0);
 assert((int32_t)open_path(&h,"/data/escape/anything",0)<0);
 assert((int32_t)open_path(&h,"/etc/passwd",0)==-44);
 assert((int32_t)open_path(&h,"/userdata/unit-test",0x80000000)==-28);
 uint32_t fd=open_path(&h,"/userdata/unit-test",2|64|512);assert((int32_t)fd>=3);
 host_store(&h,1024,"hello world",12);host_put64(&h,512,1024);host_put64(&h,520,5);host_put64(&h,528,1029);host_put64(&h,536,7);
 assert(host_io(&h,fd,512,2,2048,true,false,0)==0&&host_u64(&h,2048)==12);
 assert(host_seek(&h,fd,0,0,2048)==0);
 host_put64(&h,512,4096);host_put64(&h,520,100);
 assert(host_io(&h,fd,512,1,2048,false,false,0)==0&&host_u64(&h,2048)==12);
 char text[12];host_load(&h,text,4096,12);assert(!memcmp(text,"hello world",12));
 assert(host_stat(&h,fd,8192)==0&&host_u64(&h,8192+32)==12);
 host_put64(&h,512,UINT64_MAX);assert(host_io(&h,fd,512,1,2048,false,false,0)==21);
 assert(host_seek(&h,fd,0,1,2048)==0&&host_u64(&h,2048)==12);
 assert(host_fd_close(&h,fd)==0&&host_fd_close(&h,fd)==8);
 assert(host_environment(&h,512,520,true)==0&&host_u64(&h,512)==3);
 assert(host_environment(&h,1024,2048,false)==0&&host_u64(&h,1024)==2048);
 assert(host_clock(&h,1,4096)==0);uint64_t before=host_u64(&h,4096);assert(host_clock(&h,1,4096)==0&&host_u64(&h,4096)>=before);
 assert(host_clock(&h,99,4096)==28&&host_clock(&h,1,UINT64_MAX)==21);
 assert(unlinkat(h.save_root,"unit-test",0)==0);
 host_close(&h);wasm_rt_free_memory_shared(&h.memory);pthread_mutex_destroy(&h.memory.mem_lock);wasm_rt_free();
 puts("PASS: native host errors, rooted paths, symlink refusal, vectored I/O, file positions, stat, clocks and environment.");
}
