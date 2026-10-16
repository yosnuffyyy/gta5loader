#ifndef NATIVE_SHADOW_REFERENCE_H
#define NATIVE_SHADOW_REFERENCE_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
/* Optional equivalence to the recovered empty-cube + Always comparison path.
   The shader must independently certify a direct compare sample and its
   sampler. No shader hash or resource handle is a semantic certificate.
   This remains a reference-behavior option, not proof of producer intent. */
static inline bool shadow_reference_contract(bool comparison_sample,uint32_t selector,
 const uint32_t creation[10],const uint32_t srv[9],const uint32_t sampler[15],
 uint64_t version,uint64_t uploads,const uint32_t upload[12],const uint32_t prefix[2]){
 static const uint32_t shape[]={2,4,4,1,1,1,71,8,1};
 static const uint32_t sample[]={3,20,3,3,1,0,1,1,0,0,0,0,0,1094713344};
 if(!comparison_sample||selector!=0x40000000u||!creation||!srv||!sampler||!upload||!prefix)return false;
 const uint32_t view[]={srv[0],0,creation[0],71,4,0,1,0,0};
 const uint32_t bytes[]={creation[0],0,0,0,0,0,4,4,1,8,1,8};
 return creation[0]&&srv[0]&&version==2&&uploads==1&&!prefix[0]&&!prefix[1]&&
  !memcmp(creation+1,shape,sizeof(shape))&&!memcmp(srv,view,sizeof(view))&&
  !memcmp(sampler+1,sample,sizeof(sample))&&!memcmp(upload,bytes,sizeof(bytes));
}
#endif
