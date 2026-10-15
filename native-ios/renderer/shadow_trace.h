#ifndef NATIVE_SHADOW_TRACE_H
#define NATIVE_SHADOW_TRACE_H
#include "shadow_guard.h"
enum { SHADOW_TRACE_CAPACITY=16 };
typedef struct { uint32_t id,first,bits; uint64_t generation; } ShadowTraceUniform;
typedef struct {
 char hash[17];
 uint32_t binding,srv,source,conditions,inactive;
 uint64_t srv_generation,source_generation,source_version,first_command,last_command,present,repeats;
 ShadowTraceUniform uniform[SHADOW_CONDITIONS];
} ShadowTraceEntry;
typedef struct { ShadowTraceEntry entries[SHADOW_TRACE_CAPACITY]; uint32_t next,count; uint64_t overwritten; } ShadowTrace;
/* Only consecutive equal decisions coalesce. IDs alone are insufficient:
   bindings at different offsets or reused resource IDs are distinct states.
   This is diagnostic storage, never an inactivity or pipeline cache. */
static inline bool shadow_trace_same(const ShadowTraceEntry *a,const ShadowTraceEntry *b){
 if(memcmp(a->hash,b->hash,17)||a->binding!=b->binding||a->srv!=b->srv||a->source!=b->source||a->conditions!=b->conditions||a->inactive!=b->inactive||a->srv_generation!=b->srv_generation||a->source_generation!=b->source_generation||a->source_version!=b->source_version)return false;
 for(unsigned i=0;i<a->conditions;i++)if(a->uniform[i].id!=b->uniform[i].id||a->uniform[i].first!=b->uniform[i].first||a->uniform[i].bits!=b->uniform[i].bits||a->uniform[i].generation!=b->uniform[i].generation)return false;
 return true;
}
static inline bool shadow_trace_push(ShadowTrace *t,const ShadowTraceEntry *e){
 if(e->conditions>SHADOW_CONDITIONS)return false;
 if(t->count){ShadowTraceEntry *last=&t->entries[(t->next+SHADOW_TRACE_CAPACITY-1)%SHADOW_TRACE_CAPACITY];if(shadow_trace_same(last,e)){last->last_command=e->last_command;last->present=e->present;if(last->repeats!=UINT64_MAX)last->repeats++;return true;}}
 t->entries[t->next]=*e;t->next=(t->next+1)%SHADOW_TRACE_CAPACITY;
 if(t->count<SHADOW_TRACE_CAPACITY)t->count++;else if(t->overwritten!=UINT64_MAX)t->overwritten++;
 return true;
}
#endif
