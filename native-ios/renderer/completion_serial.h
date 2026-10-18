#ifndef NATIVE_COMPLETION_SERIAL_H
#define NATIVE_COMPLETION_SERIAL_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Service-thread-owned, bounded completion fan-out. A serial identifies ALL
   queued work at a fence, including writes not yet submitted. No hashing,
   allocation, polling, API calls or engine pointers live in this module. */
enum { COMPLETION_CAPACITY=128, COMPLETION_NONE=128 };
typedef struct {
 uint32_t id,sequence;
 uint64_t generation,token,value,started;
 unsigned next;
} CompletionReply;
typedef struct {uint64_t serial;unsigned head,tail,next;bool busy;} CompletionBatch;
typedef struct {
 CompletionReply replies[COMPLETION_CAPACITY];
 CompletionReply deferred[COMPLETION_CAPACITY];unsigned deferred_count;
 CompletionBatch batches[COMPLETION_CAPACITY];
 uint64_t serial,completed;
 unsigned free_reply,free_batch,last_batch,pending,peak;
 bool completed_valid;
} CompletionSerial;
typedef enum { COMPLETION_FULL,COMPLETION_REGISTER,COMPLETION_JOIN,COMPLETION_READY } CompletionAction;
static inline void completion_init(CompletionSerial *s){
 memset(s,0,sizeof(*s));s->last_batch=COMPLETION_NONE;
 for(unsigned i=0;i<COMPLETION_CAPACITY;i++){s->replies[i].next=i+1;s->batches[i].next=i+1;}
}
/* Combined deferred/in-flight budget stays 128. The service must submit on
   idle, pressure, a true dependency, or the finite between-command deadline. */
static inline bool completion_defer(CompletionSerial *s,CompletionReply reply){
 if(s->pending+s->deferred_count>=COMPLETION_CAPACITY)return false;
 s->deferred[s->deferred_count++]=reply;return true;
}
static inline bool completion_due(const CompletionSerial *s,uint64_t now){
 return s->deferred_count&&(s->deferred_count>=32||now<s->deferred[0].started||now-s->deferred[0].started>=1000000);
}
static inline bool completion_advance(CompletionSerial *s){
 if(s->serial==UINT64_MAX)return false;s->serial++;return true;
}
static inline CompletionAction completion_add(CompletionSerial *s,CompletionReply reply,unsigned *batch){
 *batch=COMPLETION_NONE;
 if(s->completed_valid&&s->serial<=s->completed)return COMPLETION_READY;
 if(s->free_reply==COMPLETION_NONE||s->pending+s->deferred_count>=COMPLETION_CAPACITY)return COMPLETION_FULL;
 unsigned b=s->last_batch;
 bool join=b<COMPLETION_CAPACITY&&s->batches[b].busy&&s->batches[b].serial==s->serial;
 if(!join){
  if(s->free_batch==COMPLETION_NONE)return COMPLETION_FULL;
  b=s->free_batch;s->free_batch=s->batches[b].next;
  s->batches[b]=(CompletionBatch){.serial=s->serial,.head=COMPLETION_NONE,.tail=COMPLETION_NONE,.busy=true};s->last_batch=b;
 }
 unsigned r=s->free_reply;s->free_reply=s->replies[r].next;
 reply.next=COMPLETION_NONE;s->replies[r]=reply;
 if(s->batches[b].tail==COMPLETION_NONE)s->batches[b].head=r;
 else s->replies[s->batches[b].tail].next=r;
 s->batches[b].tail=r;s->pending++;if(s->pending>s->peak)s->peak=s->pending;
 *batch=b;return join?COMPLETION_JOIN:COMPLETION_REGISTER;
}
/* Marking complete is legal ONLY after successful backend completion. A newer
   completed fence proves older queue work complete, even if callbacks arrive
   out of order. Failed/cancelled callbacks must pass success=false. */
static inline bool completion_finish(CompletionSerial *s,unsigned b,bool success){
 if(b>=COMPLETION_CAPACITY||!s->batches[b].busy)return false;
 if(success&&(!s->completed_valid||s->batches[b].serial>s->completed)){
  s->completed=s->batches[b].serial;s->completed_valid=true;
 }
 unsigned r=s->batches[b].head;
 while(r<COMPLETION_CAPACITY){unsigned next=s->replies[r].next;s->replies[r].next=s->free_reply;s->free_reply=r;s->pending--;r=next;}
 s->batches[b].busy=false;s->batches[b].next=s->free_batch;s->free_batch=b;
 if(s->last_batch==b)s->last_batch=COMPLETION_NONE;
 return true;
}
#endif
