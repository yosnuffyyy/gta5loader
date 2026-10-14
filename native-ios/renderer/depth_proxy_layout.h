#ifndef NATIVE_DEPTH_PROXY_LAYOUT_H
#define NATIVE_DEPTH_PROXY_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>
enum { DEPTH_PROXY_SUBRESOURCES=4096, DEPTH_PROXY_BUDGET=256*1024*1024 };
static inline bool depth_proxy_size(uint32_t width,uint32_t height,uint32_t mips,uint32_t layers,uint32_t bytes,uint64_t *out){
 if(!out||!width||!height||width>16384||height>16384||!mips||mips>32||!layers||layers>2048||(bytes!=1&&bytes!=4)||(uint64_t)mips*layers>DEPTH_PROXY_SUBRESOURCES)return false;
 uint32_t max=width>height?width:height,levels=1;while(max>1){max>>=1;levels++;}if(mips>levels)return false;
 uint64_t total=0;for(uint32_t mip=0;mip<mips;mip++){total+=(uint64_t)width*height*layers*bytes;width=width>1?width>>1:1;height=height>1?height>>1:1;}
 if(total>DEPTH_PROXY_BUDGET)return false;*out=total;return true;
}
static inline bool depth_proxy_range(uint32_t total,uint32_t layers,uint32_t mip,uint32_t count,uint32_t first,uint32_t n,uint32_t *mips,uint32_t *array){
 if(!mips||!array||!total||!layers||mip>=total||first>=layers)return false;
 uint32_t mc=!count||count==UINT32_MAX?total-mip:count,lc=n?n:layers-first;
 if(mc>total-mip||lc>layers-first)return false;*mips=mc;*array=lc;return true;
}
#endif
