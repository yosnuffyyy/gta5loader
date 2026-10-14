#ifndef NATIVE_BUFFER_UPLOAD_H
#define NATIVE_BUFFER_UPLOAD_H
#include <stdint.h>
#include <stdbool.h>
/* Preserve every byte outside a D3D update, including GPU-written neighbors.
   Full DWORDs use queue writes; at most two distinct edge DWORDs use GPU RMW.
   No CPU buffer mirror, readback, or GPU completion wait is required. */
typedef struct { uint32_t word, mask, value; } UploadEdge;
typedef struct { uint32_t offset, bytes, source, count; UploadEdge edge[2]; } UploadPlan;
static bool buffer_upload_plan(uint32_t size,uint32_t offset,uint32_t bytes,const uint8_t *src,UploadPlan *out){
 if(!out||offset>size||bytes>size-offset||(bytes&&!src))return false;
 *out=(UploadPlan){0};if(!bytes)return true;uint32_t consumed=0,at=offset;
 if(bytes&&(at&3)){
  unsigned n=4-(at&3);if(n>bytes)n=bytes;UploadEdge *e=out->edge+out->count++;e->word=at&~3u;
  for(unsigned i=0;i<n;i++){unsigned shift=((at&3)+i)*8;e->mask|=255u<<shift;e->value|=(uint32_t)src[i]<<shift;}
  consumed=n;at+=n;
 }
 out->offset=at&~3u;out->source=consumed;out->bytes=(bytes-consumed)&~3u;
 consumed+=out->bytes;at+=out->bytes;
 if(consumed<bytes){UploadEdge *e=out->edge+out->count++;e->word=at;
  for(unsigned i=0;i<bytes-consumed;i++){e->mask|=255u<<(i*8);e->value|=(uint32_t)src[consumed+i]<<(i*8);}
 }
 return true;
}
#endif
