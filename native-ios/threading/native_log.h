#ifndef NATIVE_LOG_H
#define NATIVE_LOG_H
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Fixed storage, no allocation or UI work on the engine's logging path.
   UI timer/final shutdown drains it. The first eight diagnostics survive a
   flood of routine messages; the recent tail retains the latest context. */
#define NATIVE_LOG_SLOTS 22
#define NATIVE_LOG_BYTES 768
typedef void (*NativeLogSink)(void*,const char*);
typedef struct {char text[NATIVE_LOG_BYTES];uint64_t repeats;} NativeLogLine;
typedef struct {
 pthread_mutex_t mutex;
 NativeLogLine lines[NATIVE_LOG_SLOTS];
 char important[8][NATIVE_LOG_BYTES];
 char recent[6][NATIVE_LOG_BYTES],categories[4][NATIVE_LOG_BYTES],camera[192];
 unsigned recent_start,recent_count;uint64_t diagnostic_count,scene_requests,camera_starts;
 unsigned start,count,important_count;
 uint64_t omitted;
} NativeLog;
static void native_log_lock(NativeLog *l){if(pthread_mutex_lock(&l->mutex))abort();}
static void native_log_unlock(NativeLog *l){if(pthread_mutex_unlock(&l->mutex))abort();}
static void native_log_init(NativeLog *l){memset(l,0,sizeof(*l));if(pthread_mutex_init(&l->mutex,NULL))abort();}
static void native_log_destroy(NativeLog *l){if(pthread_mutex_destroy(&l->mutex))abort();}
static void native_log_copy(char *out,const char *text){
 size_t n=strnlen(text,NATIVE_LOG_BYTES);if(n<NATIVE_LOG_BYTES){memcpy(out,text,n+1);return;}
 memcpy(out,text,NATIVE_LOG_BYTES-16);memcpy(out+NATIVE_LOG_BYTES-16,"... [truncated]",16);
}
static bool native_log_important(const char *s){return strstr(s,"Error")||strstr(s,"ERROR")||strstr(s,"Assert")||strstr(s,"assert")||strstr(s,"Fatal")||strstr(s,"FATAL")||strstr(s,"FAILED")||strstr(s,"FAIL:");}
static void native_log_push(NativeLog *l,const char *text){
 native_log_lock(l);
 if(strstr(text,"NEW_LOAD_SCENE_START_SPHERE")&&l->scene_requests!=UINT64_MAX)l->scene_requests++;
 if(strstr(text,"Animated camera intial frame")){if(l->camera_starts!=UINT64_MAX)l->camera_starts++;snprintf(l->camera,sizeof(l->camera),"%.191s",text);}
 if(native_log_important(text)){
  if(l->diagnostic_count!=UINT64_MAX)l->diagnostic_count++;
  unsigned category=strstr(text,"[Parser]")?0:(strstr(text,"[Audio")||strstr(text,"[RAGE Audio]"))?1:(strstr(text,"[file")||strstr(text,"pgStreamer"))?2:strstr(text,"[Graphics]")?3:4;
  if(category<4&&!l->categories[category][0])native_log_copy(l->categories[category],text);
  bool same=false;for(unsigned i=0;i<l->recent_count;i++)if(!strcmp(l->recent[(l->recent_start+i)%6],text))same=true;
  if(!same){if(l->recent_count==6){l->recent_start=(l->recent_start+1)%6;l->recent_count--;}native_log_copy(l->recent[(l->recent_start+l->recent_count++)%6],text);}
 }
 if(l->important_count<8&&native_log_important(text)){
  bool duplicate=false;for(unsigned i=0;i<l->important_count;i++)if(!strcmp(l->important[i],text))duplicate=true;
  if(!duplicate)native_log_copy(l->important[l->important_count++],text);
 }
 if(l->count){NativeLogLine *last=&l->lines[(l->start+l->count-1)%NATIVE_LOG_SLOTS];
  /* Full equality only: different long lines must not be coalesced. */
  if(!strcmp(last->text,text)){if(last->repeats!=UINT64_MAX)last->repeats++;native_log_unlock(l);return;}}
 if(l->count==NATIVE_LOG_SLOTS){l->start=(l->start+1)%NATIVE_LOG_SLOTS;l->count--;if(l->omitted!=UINT64_MAX)l->omitted++;}
 NativeLogLine *line=&l->lines[(l->start+l->count++)%NATIVE_LOG_SLOTS];native_log_copy(line->text,text);line->repeats=1;
 native_log_unlock(l);
}
static void native_log_flush(NativeLog *l,NativeLogSink sink,void *context){
 if(!sink)return;
 /* Only a single record goes on the C stack. Callouts never hold the lock. */
 native_log_lock(l);uint64_t omitted=l->omitted;l->omitted=0;unsigned limit=l->count;native_log_unlock(l);
 if(omitted){char message[128];snprintf(message,sizeof(message),"Native log: %llu earlier message groups omitted; latest context follows.",(unsigned long long)omitted);sink(context,message);}
 for(unsigned i=0;i<limit;i++){
  NativeLogLine line;native_log_lock(l);if(!l->count){native_log_unlock(l);break;}
  line=l->lines[l->start];l->start=(l->start+1)%NATIVE_LOG_SLOTS;l->count--;native_log_unlock(l);
  sink(context,line.text);if(line.repeats>1){char message[96];snprintf(message,sizeof(message),"Previous message repeated %llu times total.",(unsigned long long)line.repeats);sink(context,message);}
 }
}
static void native_log_diagnostics(NativeLog *l,NativeLogSink sink,void *context){
 if(!sink)return;native_log_lock(l);unsigned count=l->important_count;native_log_unlock(l);
 if(count)sink(context,"First engine diagnostics (may include recoverable warnings):");
 for(unsigned i=0;i<count;i++){char text[NATIVE_LOG_BYTES];native_log_lock(l);memcpy(text,l->important[i],sizeof(text));native_log_unlock(l);sink(context,text);}
 sink(context,"Recent/category diagnostics and initialization progress (may be recoverable):");
 for(unsigned i=0;i<4;i++){char text[NATIVE_LOG_BYTES];native_log_lock(l);memcpy(text,l->categories[i],sizeof(text));native_log_unlock(l);if(text[0])sink(context,text);}
 native_log_lock(l);unsigned recent=l->recent_count;native_log_unlock(l);for(unsigned i=0;i<recent;i++){char text[NATIVE_LOG_BYTES];native_log_lock(l);memcpy(text,l->recent[(l->recent_start+i)%6],sizeof(text));native_log_unlock(l);sink(context,text);}
 char progress[512];native_log_lock(l);snprintf(progress,sizeof(progress),"Initialization diagnostics: %llu error/assert messages; scene-sphere requests %llu; animated camera starts %llu. Last camera: %s",(unsigned long long)l->diagnostic_count,(unsigned long long)l->scene_requests,(unsigned long long)l->camera_starts,l->camera[0]?l->camera:"none");native_log_unlock(l);sink(context,progress);
}
#endif
