#include "native_assets.h"
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
int main(void){
 NativeAssetIndex index;char error[256];
 const char *invalid[]={
  "{}",
  "{\"mount\":\"/game/\",\"files\":[[\"../outside\",1,0]]}",
  "{\"mount\":\"/game/\",\"files\":[[\"/absolute\",1,0]]}",
  "{\"mount\":\"/game/\",\"files\":[[\"a//b\",1,0]]}",
  "{\"mount\":\"/game/\",\"files\":[[\"a\",-1,0]]}",
  "{\"mount\":\"/game/\",\"files\":[[\"a\",true,0]]}",
  "{\"mount\":\"/game/\",\"files\":[[\"a\",1.5,0]]}",
  "{\"mount\":\"/game/\",\"files\":[[\"a\",1,0],[\"a\",2,0]]}",
  "{\"mount\":\"/game/\",\"files\":[[\"a\\u0000b\",1,0]]}"
 };
 for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)assert(!native_asset_index_parse(invalid[i],strlen(invalid[i]),&index,error,sizeof(error)));
 const char *valid="{\"mount\":\"/game/\",\"files\":[[\"z/file.rpf\",4294967419,7],[\"a file.xml\",0,8]]}";
 assert(native_asset_index_parse(valid,strlen(valid),&index,error,sizeof(error)));
 assert(index.count==2&&index.files[0].size==4294967419ull&&!strcmp(index.files[0].path,"z/file.rpf"));
 NativeAssetIndex roundtrip;assert(native_asset_index_parse(index.json,index.bytes,&roundtrip,error,sizeof(error)));
 assert(roundtrip.count==2&&!strcmp(roundtrip.files[1].path,"a file.xml"));native_asset_index_free(&roundtrip);native_asset_index_free(&index);
 int root=open("native-build/fixtures/web",O_RDONLY|O_DIRECTORY);assert(root>=0);
 assert(native_asset_index_load(root,&index,error,sizeof(error))&&index.count==4);
 assert(native_asset_open(root,"escape/link-file")<0);
 assert(native_asset_open(root,"../saves/link-file")<0);
 int fd=native_asset_open(root,index.files[0].path);assert(fd>=0);close(fd);close(root);native_asset_index_free(&index);
 puts("PASS: local manifest format/order, 64-bit sizes, invalid entries, traversal and symlink rejection.");return 0;
}
