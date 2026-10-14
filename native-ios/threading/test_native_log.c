#include "native_log.h"
#include <assert.h>
typedef struct {NativeLog *log;unsigned lines;bool last,error,truncated,reenter,parser,late;} Sink;
static void receive(void *context,const char *text){
 Sink *s=context;s->lines++;s->last|=!strcmp(text,"line 999");s->error|=!strcmp(text,"Error: original cause");s->truncated|=strstr(text,"[truncated]")!=NULL;
 s->parser|=!strcmp(text,"Error: [Parser] missing metadata class");s->late|=!strcmp(text,"Error: late 39");
 assert(strlen(text)<NATIVE_LOG_BYTES);
 if(s->reenter){s->reenter=false;native_log_push(s->log,"callback reentry");}
}
static void *producer(void *arg){NativeLog *l=arg;for(unsigned i=0;i<30000;i++)native_log_push(l,"concurrent engine message");return NULL;}
int main(void){
 NativeLog l;native_log_init(&l);assert(sizeof(l)<32768);
 native_log_push(&l,"Error: original cause");for(unsigned i=0;i<1000;i++){char text[32];snprintf(text,sizeof(text),"line %u",i);native_log_push(&l,text);}
 Sink s={.log=&l,.reenter=true};native_log_flush(&l,receive,&s);assert(s.last&&s.lines<=33);native_log_diagnostics(&l,receive,&s);assert(s.error);
 native_log_flush(&l,receive,&s);assert(l.count==0);
 for(unsigned i=0;i<10000;i++)native_log_push(&l,"repeated");assert(l.count==1&&l.lines[l.start].repeats==10000);native_log_flush(&l,receive,&s);
 char longline[4096];memset(longline,'x',sizeof(longline));longline[sizeof(longline)-1]=0;native_log_push(&l,longline);native_log_flush(&l,receive,&s);assert(s.truncated);
 pthread_t threads[4];for(unsigned i=0;i<4;i++)assert(!pthread_create(threads+i,NULL,producer,&l));
 for(unsigned i=0;i<200;i++)native_log_flush(&l,receive,&s);
 for(unsigned i=0;i<4;i++)assert(!pthread_join(threads[i],NULL));native_log_flush(&l,receive,&s);assert(l.count==0);
 native_log_push(&l,"Error: [Parser] missing metadata class");for(unsigned i=0;i<40;i++){char text[40];snprintf(text,sizeof(text),"Error: late %u",i);native_log_push(&l,text);}for(unsigned i=0;i<50;i++)native_log_push(&l,"NEW_LOAD_SCENE_START_SPHERE(1,2,3,20)");native_log_push(&l,"Animated camera intial frame: Position = (1,2,3)");native_log_diagnostics(&l,receive,&s);assert(s.error&&s.parser&&s.late&&l.scene_requests==50&&l.camera_starts==1);
 native_log_destroy(&l);
 puts("PASS: bounded native startup logs retain messages after line 80, first errors, recent tail, repeat counts, truncation and concurrent producers; callbacks execute outside locks; storage below 32 KiB.");
}
