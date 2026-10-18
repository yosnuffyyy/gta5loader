#include "completion_serial.h"
#include <assert.h>
#include <stdio.h>

static uint64_t random_state=0x672687ac;
static unsigned random32(void){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return (unsigned)random_state;}
static void invariant(const CompletionSerial *s){
 bool replies[COMPLETION_CAPACITY]={0},batches[COMPLETION_CAPACITY]={0};unsigned used=0,total=0;
 for(unsigned b=0;b<COMPLETION_CAPACITY;b++)if(s->batches[b].busy){
  batches[b]=true;unsigned tail=COMPLETION_NONE;
  for(unsigned r=s->batches[b].head;r!=COMPLETION_NONE;r=s->replies[r].next){assert(r<COMPLETION_CAPACITY&&!replies[r]);replies[r]=true;used++;tail=r;}
  assert(tail==s->batches[b].tail&&tail!=COMPLETION_NONE);
 }
 assert(used==s->pending&&s->pending<=COMPLETION_CAPACITY);total=used;
 for(unsigned r=s->free_reply;r!=COMPLETION_NONE;r=s->replies[r].next){assert(r<COMPLETION_CAPACITY&&!replies[r]);replies[r]=true;total++;}
 assert(total==COMPLETION_CAPACITY);
 for(unsigned b=s->free_batch;b!=COMPLETION_NONE;b=s->batches[b].next){assert(b<COMPLETION_CAPACITY&&!batches[b]);batches[b]=true;}
 for(unsigned i=0;i<COMPLETION_CAPACITY;i++)assert(batches[i]);
}
int main(void){
 CompletionSerial s;completion_init(&s);unsigned b=0,first=0;
 CompletionReply r={.id=709,.generation=4,.token=99,.sequence=42,.value=UINT64_C(0x123456789abcdef0)};
 for(unsigned i=0;i<128;i++){CompletionAction a=completion_add(&s,r,&b);assert(a==(i?COMPLETION_JOIN:COMPLETION_REGISTER));if(!i)first=b;assert(b==first);invariant(&s);}
 CompletionSerial before=s;assert(completion_add(&s,r,&b)==COMPLETION_FULL);assert(!memcmp(&s,&before,sizeof(s)));
 assert(completion_finish(&s,first,true));assert(!completion_finish(&s,first,true));assert(completion_add(&s,r,&b)==COMPLETION_READY);invariant(&s);
 /* Writes as well as submissions advance identity; completed work cannot
    certify a later write. Cancel/failure never manufactures completion. */
 assert(completion_advance(&s));assert(completion_add(&s,r,&b)==COMPLETION_REGISTER);assert(completion_finish(&s,b,false));assert(completion_add(&s,r,&first)==COMPLETION_REGISTER);
 assert(completion_advance(&s));assert(completion_add(&s,r,&b)==COMPLETION_REGISTER);assert(completion_finish(&s,b,true));assert(completion_finish(&s,first,true));assert(s.completed==2);invariant(&s);
 /* Deterministic bounded sequence fuzzing: delayed/reordered completion,
    saturation, success/failure, repeated state, and wrap-safe exhaustion. */
 for(unsigned i=0;i<100000;i++){
  unsigned action=random32()%5;
  if(action==0)assert(completion_advance(&s));
  else if(action<3){unsigned slot=random32()%COMPLETION_CAPACITY;if(s.batches[slot].busy)assert(completion_finish(&s,slot,action==1));}
  else{CompletionAction a=completion_add(&s,r,&b);if(a==COMPLETION_REGISTER||a==COMPLETION_JOIN)assert(s.replies[s.batches[b].tail].value==r.value);}
  invariant(&s);
 }
 for(unsigned i=0;i<COMPLETION_CAPACITY;i++)if(s.batches[i].busy)assert(completion_finish(&s,i,false));
 assert(!s.pending);s.serial=UINT64_MAX;assert(!completion_advance(&s));invariant(&s);
 printf("PASS: completion serial: 100000 deterministic lifecycle/failure/reordering sequences, exact 64-bit replies, 128-entry saturation, no fake completion; state %zu bytes\n",sizeof(s));return 0;
}
