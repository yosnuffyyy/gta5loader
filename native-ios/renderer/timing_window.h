#ifndef NATIVE_TIMING_WINDOW_H
#define NATIVE_TIMING_WINDOW_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
/* Single-writer storage; external synchronization is required for readers.
   No allocation or sorting on the per-frame path. Quantiles are nearest rank
   over the latest 256 samples; lifetime counters are reported separately. */
enum { TIMING_WINDOW=256 };
typedef struct {uint64_t samples[TIMING_WINDOW],count,next,worst;double total_ns;} TimingWindow;
typedef struct {unsigned n;double mean_ms,median_ms,p95_ms,p99_ms,worst_ms;} TimingSummary;
static inline void timing_add(TimingWindow *w,uint64_t ns){w->samples[w->next++%TIMING_WINDOW]=ns;w->count++;w->total_ns+=(double)ns;if(ns>w->worst)w->worst=ns;}
static inline int timing_order(const void *a,const void *b){uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b;return (x>y)-(x<y);}
static inline double timing_recent_ms(const TimingWindow *w,unsigned limit){uint64_t n=w->count<TIMING_WINDOW?w->count:TIMING_WINDOW;if(n>limit)n=limit;if(!n)return 0;double total=0;for(uint64_t i=0;i<n;i++)total+=(double)w->samples[(w->next-1-i)%TIMING_WINDOW];return total/(double)n/1e6;}
static inline TimingSummary timing_summary(const TimingWindow *w){TimingSummary s={0};s.n=w->count<TIMING_WINDOW?(unsigned)w->count:TIMING_WINDOW;if(!s.n)return s;
 uint64_t sorted[TIMING_WINDOW];double total=0;for(unsigned i=0;i<s.n;i++){sorted[i]=w->samples[i];total+=(double)sorted[i];}qsort(sorted,s.n,sizeof(*sorted),timing_order);
 s.mean_ms=total/s.n/1e6;s.median_ms=sorted[(s.n+1)/2-1]/1e6;s.p95_ms=sorted[(s.n*95+99)/100-1]/1e6;s.p99_ms=sorted[(s.n*99+99)/100-1]/1e6;s.worst_ms=sorted[s.n-1]/1e6;return s;
}
static inline void timing_text(const TimingWindow *w,const char *label,char *text,size_t size){TimingSummary s=timing_summary(w);if(!s.n){snprintf(text,size,"%s: no samples",label);return;}
 if(s.n<20){snprintf(text,size,"%s: latest %u, mean %.3f ms, max %.3f ms; quantiles withheld (<20 samples); lifetime %llu calls, %.3f ms, max %.3f ms",label,s.n,s.mean_ms,s.worst_ms,(unsigned long long)w->count,w->total_ns/1e6,w->worst/1e6);return;}
 snprintf(text,size,"%s: latest %u ms mean/median/p95/p99/max %.3f/%.3f/%.3f/%.3f/%.3f; lifetime %llu calls %.3f ms max %.3f",label,s.n,s.mean_ms,s.median_ms,s.p95_ms,s.p99_ms,s.worst_ms,(unsigned long long)w->count,w->total_ns/1e6,w->worst/1e6);
}
#endif
