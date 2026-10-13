#include "native_host.h"
#include "platform/native_paths.h"
#include "wasm-rt-impl.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
static uint32_t open_path(struct w2c_env *h,const char *path,unsigned flags){host_store(h,64,path,strlen(path)+1);return host_open(h,(uint32_t)-100,64,flags,0);}
int main(void){
 wasm_rt_init();struct w2c_env h={0};host_init(&h);
 wasm_rt_allocate_memory_shared(&h.memory,2,4,false,65536);
 h.game_root=open("native-build/fixtures/web",O_RDONLY|O_DIRECTORY);
 h.save_root=open("native-build/fixtures/saves",O_RDONLY|O_DIRECTORY);
 assert(h.game_root>=0&&h.save_root>=0);
 host_store(&h,64,"/userdata",10);assert(host_path_stat(&h,64,8192)==0);
 host_store(&h,64,"/userdata/path-test",20);
 assert(host_mkdir(&h,(uint32_t)-100,64,0777)==0);
 assert(host_path_stat(&h,64,8192)==0);
 assert(host_mkdir(&h,(uint32_t)-100,64,0777)==(uint32_t)-20);
 assert(unlinkat(h.save_root,"path-test",AT_REMOVEDIR)==0);
 host_store(&h,64,"/userdata/../escape",20);assert(host_mkdir(&h,(uint32_t)-100,64,0777)==(uint32_t)-63);
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
