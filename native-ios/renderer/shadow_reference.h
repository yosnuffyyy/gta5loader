#ifndef NATIVE_SHADOW_REFERENCE_H
#define NATIVE_SHADOW_REFERENCE_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
/* Optional equivalence with the recovered browser's empty-cube + Always
   comparison path. This is NOT a proof that the producer intended no shadow.
   Exact captured contracts only; real comparison samplers and changed resources
   stay on strict validation. IDs alone are never sufficient. */
static inline bool shadow_reference_contract(const char *hash,uint32_t binding,
 uint32_t light_count,uint32_t selector,const uint32_t creation[10],
 const uint32_t srv[9],const uint32_t sampler[15],uint64_t version,
 uint64_t uploads,const uint32_t upload[12],const uint32_t prefix[2]){
 static const uint32_t captured_create[]={708,2,4,4,1,1,1,71,8,1};
 static const uint32_t captured_srv[]={709,0,708,71,4,0,1,0,0};
 static const uint32_t captured_sampler[]={3,20,3,3,1,0,1,1,0,0,0,0,0,1094713344};
 static const uint32_t captured_upload[]={708,0,0,0,0,0,4,4,1,8,1,8};
 return hash&&creation&&srv&&sampler&&upload&&prefix&&
  !strcmp(hash,"996bb42f448956f2")&&binding==46&&light_count==1&&selector==0x40000000u&&
  version==2&&uploads==1&&!prefix[0]&&!prefix[1]&&
  !memcmp(creation,captured_create,sizeof(captured_create))&&
  !memcmp(srv,captured_srv,sizeof(captured_srv))&&
  !memcmp(sampler+1,captured_sampler,sizeof(captured_sampler))&&
  !memcmp(upload,captured_upload,sizeof(captured_upload));
}
#endif
