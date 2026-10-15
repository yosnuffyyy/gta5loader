#ifndef NATIVE_CACHE_BUDGET_H
#define NATIVE_CACHE_BUDGET_H
#include <stdint.h>
/* Count limits are explicit; opaque driver pipeline allocations are not bytes
   we can estimate. Hysteresis avoids recompiling on small pressure changes. */
enum { PIPELINE_NORMAL_CAPACITY=256, PIPELINE_PRESSURE_CAPACITY=128 };
static inline unsigned pipeline_budget(unsigned current,uint64_t available){
 if(available<UINT64_C(256)*1024*1024)return PIPELINE_PRESSURE_CAPACITY;
 if(!current||available>=UINT64_C(512)*1024*1024)return PIPELINE_NORMAL_CAPACITY;
 return current;
}
/* Queue writes are ordered after prior submissions. These limits bound queued
   uniform staging, rather than incorrectly requiring a wait on slot reuse. */
static inline int uniform_pressure(unsigned batches,uint64_t bytes){
 return batches>=16||bytes>=UINT64_C(4)*1024*1024;
}
#endif
