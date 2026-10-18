#include "native_profile.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
static _Atomic bool enabled;
void native_profile_set_enabled(bool value){atomic_store_explicit(&enabled,value,memory_order_release);}
bool native_profile_enabled(void){return atomic_load_explicit(&enabled,memory_order_acquire);}
NativeProfile *native_profile_create(void){return atomic_load_explicit(&enabled,memory_order_acquire)?calloc(1,sizeof(NativeProfile)):NULL;}
uint64_t native_profile_cpu(void){
#if defined(CLOCK_THREAD_CPUTIME_ID)
 struct timespec t;if(!clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t))return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;
#endif
 return UINT64_MAX;
}
static uint64_t elapsed(NativeProfile *p,uint64_t start){uint64_t end=native_profile_cpu();if(start==UINT64_MAX||end==UINT64_MAX||end<start){p->clock_failures++;return 0;}return end-start;}
static void add(NPWindow *w,uint64_t wall,uint64_t cpu){w->wall[w->next]=wall;w->cpu[w->next]=cpu;w->next=(w->next+1)%NP_SAMPLES;if(w->used<NP_SAMPLES)w->used++;w->count++;w->total_wall+=wall;w->total_cpu+=cpu;}
void native_profile_stage(NativeProfile *p,NPStage kind,uint64_t wall,uint64_t start){if(p&&kind<NP_STAGES)add(&p->stage[kind],wall,elapsed(p,start));}
void native_profile_operation(NativeProfile *p,unsigned op,uint64_t wall,uint64_t start,uint64_t bytes,uint64_t submissions,bool flushed){if(!p||op>=NP_OPS)return;uint64_t cpu=elapsed(p,start);NPOperation *o=p->op+op;o->count++;o->wall+=wall;o->cpu+=cpu;if(wall>o->max_wall)o->max_wall=wall;if(cpu>o->max_cpu)o->max_cpu=cpu;o->bytes+=bytes;o->submissions+=submissions;o->flushes+=flushed;p->command_wall+=wall;p->command_cpu+=cpu;p->frame_wall+=wall;p->frame_cpu+=cpu;if(op==7){add(&p->stage[NP_FRAME_HANDLERS],p->frame_wall,p->frame_cpu);p->frame_wall=p->frame_cpu=0;}}
void native_profile_residual(NativeProfile *p,uint64_t wall,uint64_t start,uint64_t before_wall,uint64_t before_cpu){if(!p)return;uint64_t cpu=elapsed(p,start),commands=p->command_wall-before_wall,active=p->command_cpu-before_cpu;if(wall<commands||cpu<active)p->residual_underflows++;add(&p->stage[NP_RING_RESIDUAL],wall>=commands?wall-commands:0,cpu>=active?cpu-active:0);}
static int cmp(const void *a,const void *b){uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b;return (x>y)-(x<y);}
static void describe(char *s,size_t n,const uint64_t *samples,unsigned count){if(!count){snprintf(s,n,"no samples");return;}uint64_t x[NP_SAMPLES],sum=0;memcpy(x,samples,count*sizeof(*x));qsort(x,count,sizeof(*x),cmp);for(unsigned i=0;i<count;i++)sum+=x[i];if(count<20){snprintf(s,n,"n%u mean/max %.6f/%.6f ms; quantiles withheld (<20)",count,sum/(double)count/1e6,x[count-1]/1e6);return;}snprintf(s,n,"n%u mean/p50/p95/p99/max %.6f/%.6f/%.6f/%.6f/%.6f ms",count,sum/(double)count/1e6,x[(count*50+99)/100-1]/1e6,x[(count*95+99)/100-1]/1e6,x[(count*99+99)/100-1]/1e6,x[count-1]/1e6);}
void native_profile_report(const NativeProfile *p,void (*emit)(void *,const char *),void *context){if(!p||!emit)return;char line[1000];snprintf(line,sizeof(line),"Optional CPU profile: %zu fixed bytes, 48 opcode aggregates and latest256 samples/stage; clock failures %llu, residual underflows %llu. Nested categories overlap. CPU includes driver work on this service thread only; GPU execution and engine simulation time UNKNOWN.",sizeof(*p),(unsigned long long)p->clock_failures,(unsigned long long)p->residual_underflows);emit(context,line);
 for(unsigned i=0;i<NP_OPS;i++){const NPOperation *o=p->op+i;if(!o->count)continue;snprintf(line,sizeof(line),"CPU profile opcode %u: calls %llu; wall/active %.6f/%.6f ms; max wall/active %.6f/%.6f ms; protocol upload bytes %llu; nested submissions %llu; pending mutation preflushes %llu. Inclusive wall; active CPU UNKNOWN if clock failures are nonzero.",i,(unsigned long long)o->count,o->wall/1e6,o->cpu/1e6,o->max_wall/1e6,o->max_cpu/1e6,(unsigned long long)o->bytes,(unsigned long long)o->submissions,(unsigned long long)o->flushes);emit(context,line);}
 static const char *names[]={"queue submit API","completion wait","render pipeline API","varying linking","pending mutation preflush","drain flush","ring residual (decode/publication/wake + clock overhead)","command handlers per protocol PRESENT (not displayed frame)"};
 for(unsigned i=0;i<NP_STAGES;i++){const NPWindow *w=p->stage+i;if(!w->count)continue;char wall[220],cpu[220];describe(wall,sizeof(wall),w->wall,w->used);describe(cpu,sizeof(cpu),w->cpu,w->used);snprintf(line,sizeof(line),"CPU profile %s: wall {%s}; active {%s}; lifetime calls %llu wall/active %.6f/%.6f ms. CPU invalid if clock failures nonzero.",names[i],wall,cpu,(unsigned long long)w->count,w->total_wall/1e6,w->total_cpu/1e6);emit(context,line);}
}
