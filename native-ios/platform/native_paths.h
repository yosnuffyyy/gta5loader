#ifndef NATIVE_PATHS_H
#define NATIVE_PATHS_H
#include "native_host.h"
uint32_t host_path_stat(struct w2c_env*,uint64_t,uint64_t);
uint32_t host_statfs(struct w2c_env*,uint64_t,uint64_t,uint64_t);
void native_storage_probe(struct w2c_env*,w2c_gta*,uint64_t);
uint32_t host_mkdir(struct w2c_env*,uint32_t,uint64_t,uint32_t);
#endif
