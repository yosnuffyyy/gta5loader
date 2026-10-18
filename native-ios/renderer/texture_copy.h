#ifndef NATIVE_TEXTURE_COPY_H
#define NATIVE_TEXTURE_COPY_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* D3D copies reinterpret bits; equal byte widths alone are not compatibility.
   Typeless families and the documented block-compression cast groups only. */
static inline unsigned tc_family(unsigned f){
 const unsigned ranges[][2]={{1,4},{5,8},{9,14},{15,18},{19,22},{23,25},{27,32},{33,38},{39,43},{44,47},{48,52},{53,59},{60,64},{70,72},{73,75},{76,78},{79,81},{82,84},{94,96},{97,99}};
 for(unsigned i=0;i<sizeof(ranges)/sizeof(*ranges);i++)if(f>=ranges[i][0]&&f<=ranges[i][1])return ranges[i][0];
 if(f==87||f==90||f==91)return 87;if(f==88||f==92||f==93)return 88;return f;
}
static inline bool tc_cast(unsigned a,unsigned b){
 if(a==b||tc_family(a)==tc_family(b))return true;
 unsigned bc=tc_family(a),raw=tc_family(b);if(bc<70||bc>99){bc=tc_family(b);raw=tc_family(a);}
 if(bc==70||bc==79)return raw==9||raw==15;
 if(bc==73||bc==76||bc==82||bc==94||bc==97)return raw==1;
 return (tc_family(a)==39&&b==67)||(tc_family(b)==39&&a==67);
}
typedef struct {uint32_t width,height,depth,bw,bh,bytes;} CopyShape;
typedef struct {uint32_t blocks,rows,depth,sw,sh,dw,dh,pitch,tile_rows;} CopyPlan;
static inline const char *tc_plan(CopyShape s,CopyShape d,const uint32_t so[3],const uint32_t to[3],const uint32_t end[3],CopyPlan *p){
 if(!s.bw||!s.bh||!d.bw||!d.bh||!s.bytes||s.bytes!=d.bytes)return "Copy formats have incompatible storage blocks";
 if(so[0]>end[0]||so[1]>end[1]||so[2]>end[2]||end[0]>s.width||end[1]>s.height||end[2]>s.depth)return "Copy source box outside subresource";
 if(to[0]>d.width||to[1]>d.height||to[2]>d.depth)return "Copy destination outside subresource";
 if(so[0]%s.bw||so[1]%s.bh||to[0]%d.bw||to[1]%d.bh||(end[0]%s.bw&&end[0]!=s.width)||(end[1]%s.bh&&end[1]!=s.height))return "Copy splits a compressed storage block";
 uint64_t cols=((uint64_t)end[0]-so[0]+s.bw-1)/s.bw,rows=((uint64_t)end[1]-so[1]+s.bh-1)/s.bh,depth=end[2]-so[2];
 uint64_t physicalW=((uint64_t)d.width+d.bw-1)/d.bw*d.bw,physicalH=((uint64_t)d.height+d.bh-1)/d.bh*d.bh;
 if(cols*d.bw>physicalW-to[0]||rows*d.bh>physicalH-to[1]||depth>d.depth-to[2])return "Copy destination blocks outside subresource";
 uint64_t pitch=(cols*s.bytes+255)&~UINT64_C(255);if(pitch>1024*1024)return "Copy row exceeds bounded scratch";
 *p=(CopyPlan){(uint32_t)cols,(uint32_t)rows,(uint32_t)depth,(uint32_t)(cols*s.bw),(uint32_t)(rows*s.bh),(uint32_t)(cols*d.bw),(uint32_t)(rows*d.bh),(uint32_t)pitch,pitch?(uint32_t)(1024*1024/pitch):0};return NULL;
}
#endif
