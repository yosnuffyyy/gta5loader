#ifndef NATIVE_SRV_CONTRACT_H
#define NATIVE_SRV_CONTRACT_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "depth_proxy_layout.h"

/* Values are the original command protocol's D3D SRV dimensions. Cube-array
   counts have ALREADY been multiplied by six in Wasm function 328. */
typedef struct { uint32_t dimension,width,height,layers,mips,samples; } SrvTexture;
typedef struct { uint32_t mip,mips,first,layers,dimension; bool ms; } SrvRange;
static inline const char *srv_decode(SrvTexture t,const uint32_t *w,SrvRange *r){
 if(!w||!r||w[1]||!t.width||!t.height||!t.layers||!t.mips||!t.samples)return "invalid texture/SRV descriptor";
 uint32_t d=w[4];
 /* Null D3D descriptor: producer emits dimension zero and remaining counts.
    The protocol contains no cube misc flag; never infer cube from six layers. */
 if(!d)d=t.dimension==3?8:t.dimension==1?(t.layers==1?2:3):t.samples>1?(t.layers==1?6:7):(t.layers==1?4:5);
 bool ms=d==6||d==7;
 if(d<2||d>10)return "unknown texture SRV dimension";
 if((d==2||d==3?t.dimension!=1:d==8?t.dimension!=3:t.dimension!=2))return "view/resource dimension mismatch";
 if(ms!=(t.samples>1))return "view/resource multisample mismatch";
 if((d==9||d==10)&&t.width!=t.height)return "cube faces are not square";
 uint32_t first=w[7],count=w[8],mip=w[5],mips=w[6];
 if(d==2||d==4||d==6){if(first)return "non-array SRV has a first array layer";count=1;}
 if(d==8){if(first)return "3D SRV has a first array layer";count=1;}
 if(d==9){if(first)return "cube SRV has a first array face";count=6;}
 if(ms&&!w[4]&&!mip&&(!mips||mips==UINT32_MAX))mips=1;
 if(ms&&(mip||mips!=1))return "multisample SRV has mip range";
 uint32_t available=d==8?1:t.layers,mc=0,lc=0;
 if(!depth_proxy_range(t.mips,available,mip,mips,first,count,&mc,&lc))return "range outside texture";
 /* D3D permits a cube array to start at any face; only the count must be
    a multiple of six. Do not impose first % 6 == 0. */
 if(d==10&&lc%6)return "cube-array face count is not a multiple of six";
 *r=(SrvRange){mip,mc,first,lc,d,ms};return NULL;
}
static inline const char *srv_shader_contract(SrvRange r,uint32_t shader_dimension,bool ms){
 /* D3D11.3 22.3.12: Texture2DArray accepts one or more selected layers;
    Texture2D requires exactly one. The SRV supplies the visible range, while
    the shader supplies the native view shape. No layer replication or range
    adjustment is needed. Keep cube/3D and multisample contracts separate. */
 bool dimension=r.dimension==shader_dimension;
 if((r.dimension==4||r.dimension==5)&&(shader_dimension==4||shader_dimension==5))
  dimension=r.layers>=1&&(shader_dimension==5||r.layers==1);
 if(!dimension||r.ms!=ms)return "SRV/shader dimension or multisample mismatch";
 return NULL;
}
#endif
