#include "buffer_upload.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
int main(void){
 unsigned cases=0;uint8_t source[64];for(unsigned i=0;i<64;i++)source[i]=(uint8_t)(i*37+19);
 for(unsigned size=0;size<=64;size++)for(unsigned at=0;at<=size;at++)for(unsigned n=0;n<=size-at;n++){
  uint8_t actual[72],expected[72];memset(actual,0xa5,sizeof(actual));memcpy(expected,actual,sizeof(actual));memcpy(expected+at,source,n);
  UploadPlan p;assert(buffer_upload_plan(size,at,n,source,&p));assert(p.count<=2&&!(p.offset&3)&&!(p.bytes&3));
  if(p.bytes)memcpy(actual+p.offset,source+p.source,p.bytes);
  for(unsigned i=0;i<p.count;i++){uint32_t word;assert(!(p.edge[i].word&3)&&p.edge[i].word<size);memcpy(&word,actual+p.edge[i].word,4);word=(word&~p.edge[i].mask)|p.edge[i].value;memcpy(actual+p.edge[i].word,&word,4);}
  if(p.count==2)assert(p.edge[0].word!=p.edge[1].word);assert(!memcmp(actual,expected,sizeof(actual)));cases++;
 }
 UploadPlan p;assert(!buffer_upload_plan(4,5,0,source,&p));assert(!buffer_upload_plan(4,3,2,source,&p));assert(!buffer_upload_plan(4,0,1,NULL,&p));assert(!buffer_upload_plan(4,0,0,NULL,NULL));assert(buffer_upload_plan(UINT32_MAX,UINT32_MAX,0,NULL,&p));assert(!buffer_upload_plan(UINT32_MAX,UINT32_MAX,1,source,&p));
 /* Reproduce 0.18's destructive padded tail, independently of the new plan. */
 uint8_t legacy[8]={1,2,3,4,5,6,7,8};uint32_t tail=0;memcpy(&tail,source,1);memcpy(legacy,&tail,4);assert(legacy[1]==0&&legacy[2]==0&&legacy[3]==0);
 printf("PASS: buffer upload edge layout: %u exhaustive ranges, surrounding-byte guards, empty/overflow rejection; old padded-tail corruption reproduced.\n",cases);return 0;
}
