#ifndef NATIVE_ASSETS_H
#define NATIVE_ASSETS_H
#include "native_host.h"
typedef struct {char *path; uint64_t size;} NativeAsset;
typedef struct {NativeAsset *files; unsigned count; char *json; size_t bytes;} NativeAssetIndex;
typedef struct NativeAssets NativeAssets;
bool native_asset_path_valid(const char*);
int native_asset_open(int,const char*);
bool native_asset_index_load(int,NativeAssetIndex*,char*,size_t);
bool native_asset_index_parse(const void*,size_t,NativeAssetIndex*,char*,size_t);
void native_asset_index_free(NativeAssetIndex*);
NativeAssets *native_assets_begin(struct w2c_env*);
void native_assets_end(NativeAssets*);
void native_assets_stop(NativeAssets*);
uint32_t native_assets_manifest(NativeAssets*,uint64_t,uint32_t);
void native_assets_start(NativeAssets*,uint64_t);
void native_assets_probe(NativeAssets*,uint64_t);
void native_assets_arm(NativeAssets*,void(*)(void*),void*);
const char *native_assets_boundary(NativeAssets*);
const char *native_assets_failure(NativeAssets*);
void native_assets_summary(NativeAssets*,char*,size_t);
void native_assets_thread_probe(NativeAssets*,w2c_gta*,uint64_t,unsigned);
#endif
