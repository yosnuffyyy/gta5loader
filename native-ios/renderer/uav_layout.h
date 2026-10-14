#ifndef NATIVE_UAV_LAYOUT_H
#define NATIVE_UAV_LAYOUT_H
#include <stdint.h>
#include <stdbool.h>
static inline bool uav_hidden_counter(uint32_t kind,uint32_t flags){return kind==1&&(flags&6)!=0;}
static inline bool uav_buffer_range(uint64_t bytes,uint32_t stride,uint32_t first,uint32_t count,uint64_t *offset,uint64_t *size){
 if(!stride||!offset||!size)return false;uint64_t at=(uint64_t)first*stride;if(at>=bytes)return false;
 uint64_t n=!count||count==UINT32_MAX?bytes-at:(uint64_t)count*stride;if(!n||n>bytes-at)return false;*offset=at;*size=n;return true;
}
static inline uint32_t uav_component_mask(uint32_t bits){return bits>=32?UINT32_MAX:(1u<<bits)-1;}
#endif
