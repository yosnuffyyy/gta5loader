#ifndef NATIVE_VERTEX_STEP_H
#define NATIVE_VERTEX_STEP_H
#include <stdint.h>
/* D3D11.3 8.4.1: StartInstanceLocation selects the first record. The step
   counter starts at rate, independently of that location. Rate 0 never steps. */
static inline uint64_t vertex_step_index(uint32_t first,uint32_t local,uint32_t rate){return (uint64_t)first+(rate?local/rate:0);}
static inline uint32_t vertex_step_run(uint32_t rate,uint32_t local,uint32_t remaining){
 if(rate<=1)return remaining;uint32_t n=rate-local%rate;return n<remaining?n:remaining;
}
static inline uint32_t vertex_step_capacity(uint64_t records,uint32_t rate){
 if(!records)return 0;if(!rate||records>UINT32_MAX/rate)return UINT32_MAX;return (uint32_t)(records*rate);
}
#endif
