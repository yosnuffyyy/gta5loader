#include "shadow_reference.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 uint32_t creation[]={708,2,4,4,1,1,1,71,8,1},srv[]={709,0,708,71,4,0,1,0,0};
 uint32_t sampler[]={3511,3,20,3,3,1,0,1,1,0,0,0,0,0,1094713344};
 uint32_t upload[]={708,0,0,0,0,0,4,4,1,8,1,8},prefix[]={0,0};unsigned checks=0;
 #define ELIGIBLE(hash,binding,count,type,version,uploads) shadow_reference_contract(hash,binding,count,type,creation,srv,sampler,version,uploads,upload,prefix)
 #define CHECK(expected,expression) do{assert((expression)==(expected));checks++;}while(0)
 CHECK(true,ELIGIBLE("996bb42f448956f2",46,1,0x40000000,2,1));
 for(unsigned i=0;i<10;i++){creation[i]^=1;CHECK(false,ELIGIBLE("996bb42f448956f2",46,1,0x40000000,2,1));creation[i]^=1;}
 for(unsigned i=0;i<9;i++){srv[i]^=1;CHECK(false,ELIGIBLE("996bb42f448956f2",46,1,0x40000000,2,1));srv[i]^=1;}
 for(unsigned i=1;i<15;i++){sampler[i]^=1;CHECK(false,ELIGIBLE("996bb42f448956f2",46,1,0x40000000,2,1));sampler[i]^=1;}
 for(unsigned i=0;i<12;i++){upload[i]^=1;CHECK(false,ELIGIBLE("996bb42f448956f2",46,1,0x40000000,2,1));upload[i]^=1;}
 for(unsigned i=0;i<2;i++){prefix[i]=1;CHECK(false,ELIGIBLE("996bb42f448956f2",46,1,0x40000000,2,1));prefix[i]=0;}
 const uint32_t selectors[]={0,0x80000000,0x3f800000,0x40400000,0x7f800000,0x7fc00000,0xffffffff};
 for(unsigned i=0;i<sizeof(selectors)/sizeof(*selectors);i++)CHECK(false,ELIGIBLE("996bb42f448956f2",46,1,selectors[i],2,1));
 const uint32_t counts[]={0,0x80000000,0xffffffff};for(unsigned i=0;i<3;i++)CHECK(false,ELIGIBLE("996bb42f448956f2",46,counts[i],0x40000000,2,1));
 const uint32_t positive[]={1,2,3,4,5,6,INT32_MAX};
 for(unsigned i=0;i<7;i++)CHECK(true,ELIGIBLE("996bb42f448956f2",46,positive[i],0x40000000,2,1));
 for(unsigned i=0;i<4;i++)if(i!=2)CHECK(false,ELIGIBLE("996bb42f448956f2",46,1,0x40000000,i,1));
 for(unsigned i=0;i<4;i++)if(i!=1)CHECK(false,ELIGIBLE("996bb42f448956f2",46,1,0x40000000,2,i));
 CHECK(false,ELIGIBLE("996bb42f448956f3",46,1,0x40000000,2,1));
 CHECK(false,ELIGIBLE("996bb42f448956f2",47,1,0x40000000,2,1));
 sampler[2]|=0x80;CHECK(false,ELIGIBLE("996bb42f448956f2",46,1,0x40000000,2,1));sampler[2]&=~0x80;
 sampler[0]=7777;CHECK(true,ELIGIBLE("996bb42f448956f2",46,1,0x40000000,2,1));
 CHECK(false,ELIGIBLE(NULL,46,1,0x40000000,2,1));
 printf("PASS: reference empty-shadow contract: %u checks, every captured descriptor/upload lane, regular/comparison sampler distinction, active/inactive/ambiguous constants, resource mutation and sampler ID reuse.\n",checks);
 return 0;
}
