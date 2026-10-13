#import <Foundation/Foundation.h>
#include "native_assets.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>

bool native_asset_path_valid(const char *path) {
 if(!path||!*path||strlen(path)>2048)return false;
 const char *part=path;
 for(const char *p=path;;p++){
  unsigned char c=*p;
  if(!c||c=='/'){
   size_t n=p-part;if(!n||(n==1&&part[0]=='.')||(n==2&&part[0]=='.'&&part[1]=='.'))return false;
   if(!c)return true;part=p+1;
  }else if(c<32||c>126||c=='\\'||c=='"'||c==':')return false;
 }
}
int native_asset_open(int root,const char *path) {
 if(root<0||!native_asset_path_valid(path)){errno=EINVAL;return -1;}
 int dir=openat(root,"data",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 if(dir<0)return -1;
 char text[2049];strcpy(text,path);char *part=text;
 for(;;){char *slash=strchr(part,'/');if(slash)*slash=0;
  int next=openat(dir,part,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|(slash?O_DIRECTORY:0));
  int error=errno;close(dir);if(next<0){errno=error;return -1;}
  if(!slash){struct stat st;if(fstat(next,&st)||!S_ISREG(st.st_mode)){close(next);errno=EINVAL;return -1;}return next;}
  dir=next;part=slash+1;
 }
}
void native_asset_index_free(NativeAssetIndex *index){
 for(unsigned i=0;i<index->count;i++)free(index->files[i].path);
 free(index->files);free(index->json);memset(index,0,sizeof(*index));
}
static bool valid_number(id object){
 if(![object isKindOfClass:NSNumber.class]||CFGetTypeID((__bridge CFTypeRef)object)==CFBooleanGetTypeID())return false;
 double value=[object doubleValue];return isfinite(value)&&value>=0&&value<=9007199254740991.0&&floor(value)==value;
}
bool native_asset_index_parse(const void *bytes,size_t length,NativeAssetIndex *index,char *error,size_t capacity){
 memset(index,0,sizeof(*index));
 @autoreleasepool {
  if(!length||length>8*1024*1024){snprintf(error,capacity,"manifest size outside 1..8 MiB");return false;}
  NSError *parseError=nil;
  id object=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytesNoCopy:(void*)bytes length:length freeWhenDone:NO] options:0 error:&parseError];
  if(![object isKindOfClass:NSDictionary.class]||![object[@"mount"] isEqual:@"/game/"]||![object[@"files"] isKindOfClass:NSArray.class]){snprintf(error,capacity,"manifest needs /game/ mount and files array");return false;}
  NSArray *rows=object[@"files"];
  if(!rows.count||rows.count>20000){snprintf(error,capacity,"manifest file count outside 1..20000");return false;}
  index->files=calloc(rows.count,sizeof(*index->files));if(!index->files){snprintf(error,capacity,"manifest allocation failed");return false;}
  NSMutableSet *seen=[NSMutableSet set];NSMutableArray *normalized=[NSMutableArray arrayWithCapacity:rows.count];NSData *json=nil;
  for(id row in rows){
   if(![row isKindOfClass:NSArray.class]||[row count]!=3||![row[0] isKindOfClass:NSString.class]||!valid_number(row[1])||!valid_number(row[2]))goto invalid;
   NSString *path=row[0];const char *name=path.UTF8String;
   if(!name||strlen(name)!=[path lengthOfBytesUsingEncoding:NSUTF8StringEncoding]||!native_asset_path_valid(name)||[seen containsObject:path])goto invalid;
   char *copy=strdup(name);if(!copy)goto invalid;
   index->files[index->count++]=(NativeAsset){copy,[row[1] unsignedLongLongValue]};
   [seen addObject:path];[normalized addObject:@[path,row[1],row[2]]];
  }
  // Emit the exact tuple format the pinned engine parser accepts. Keep index
  // order identical to the native reader; the engine sends numeric file IDs.
  json=[NSJSONSerialization dataWithJSONObject:@{@"mount":@"/game/",@"version":@"native-local",@"files":normalized} options:NSJSONWritingWithoutEscapingSlashes error:nil];
  if(!json||json.length>8*1024*1024)goto invalid;
  index->json=malloc(json.length+1);if(!index->json)goto invalid;
  memcpy(index->json,json.bytes,json.length);index->json[json.length]=0;index->bytes=json.length;return true;
invalid:
  native_asset_index_free(index);snprintf(error,capacity,"manifest has an invalid, duplicate or unsafe file entry");return false;
 }
}
bool native_asset_index_load(int root,NativeAssetIndex *index,char *error,size_t capacity){
 int fd=native_asset_open(root,"manifest.json");
 if(fd<0){snprintf(error,capacity,"cannot open selected web/data/manifest.json");return false;}
 struct stat st;if(fstat(fd,&st)||st.st_size<=0||st.st_size>8*1024*1024){close(fd);snprintf(error,capacity,"invalid manifest size");return false;}
 size_t size=(size_t)st.st_size;char *bytes=malloc(size);if(!bytes){close(fd);snprintf(error,capacity,"manifest allocation failed");return false;}
 size_t done=0;while(done<size){ssize_t n=read(fd,bytes+done,size-done);if(n<0&&errno==EINTR)continue;if(n<=0)break;done+=(size_t)n;}
 close(fd);bool ok=false;if(done==size)ok=native_asset_index_parse(bytes,size,index,error,capacity);else snprintf(error,capacity,"manifest read incomplete; download the folder locally first");
 free(bytes);return ok;
}
