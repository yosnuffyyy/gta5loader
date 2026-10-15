#include "pipeline_key.h"
#include <assert.h>
#include <stdio.h>
static DrawKey key(unsigned n){DrawKey k={0};k.topology=4;k.index_format=n&1?57:42;k.samples=1;k.mask=n&2?UINT32_MAX:1;k.raster[7]=(n>>2)&1;k.formats[0]=1;k.strides[0]=24;k.strides[15]=n;k.blend[9]=15;
 for(unsigned i=0;i<6;i++)k.blend[3+i]=1+((n>>(i+2))&1);return k;}
static void model(bool normalized,unsigned *creates,unsigned *evictions){DrawKey cache[128]={0};bool occupied[128]={0};uint64_t ages[128]={0},age=0;*creates=*evictions=0;
 for(unsigned step=0;step<512;step++){DrawKey k=key(step%256);if(normalized)pipeline_key_normalize(&k,1,1,false);unsigned slot=0;bool hit=false;for(unsigned i=0;i<128;i++){if(occupied[i]&&!memcmp(cache+i,&k,sizeof(k))){slot=i;hit=true;break;}if(!occupied[i]){slot=i;break;}if(ages[i]<ages[slot])slot=i;}if(!hit){(*creates)++;if(occupied[slot])(*evictions)++;cache[slot]=k;occupied[slot]=true;}ages[slot]=++age;}}
int main(void){DrawKey expected=key(0);pipeline_key_normalize(&expected,1,1,false);for(unsigned n=0;n<256;n++){DrawKey k=key(n);pipeline_key_normalize(&k,1,1,false);assert(!memcmp(&expected,&k,sizeof(k)));}
 for(unsigned test=0;test<6;test++){DrawKey k=expected;if(test==0)k.strides[0]++;if(test==1)k.blend[2]=1;if(test==2)k.raster[1]++;if(test==3)k.formats[0]++;if(test==4)k.inactive_masks[1]++;if(test==5)k.blend[9]=7;pipeline_key_normalize(&k,1,1,false);assert(memcmp(&expected,&k,sizeof(k)));}
 DrawKey a=key(0),b=a;a.topology=b.topology=5;a.index_format=57;b.index_format=42;pipeline_key_normalize(&a,1,1,false);pipeline_key_normalize(&b,1,1,false);assert(memcmp(&a,&b,sizeof(a)));
 a=key(0);a.formats[8]=7;a.depth[0]=1;a.depth[1]=1;a.depth[2]=2;a.depth[3]=1;a.depth[4]=0xffff;a.ro=3;b=a;b.depth[6]=2;pipeline_key_normalize(&a,1,1,true);pipeline_key_normalize(&b,1,1,true);assert(memcmp(&a,&b,sizeof(a)));
 unsigned old,oe,now,ne;model(false,&old,&oe);model(true,&now,&ne);assert(old==512&&oe==384&&now==1&&ne==0);printf("PASS: semantic pipeline keys: 256 equivalent descriptors, 8 required distinctions; 128-entry synthetic LRU creates/evictions %u/%u -> %u/%u; metadata key %zu bytes. CPU model, no FPS claim.\n",old,oe,now,ne,sizeof(DrawKey));return 0;}
