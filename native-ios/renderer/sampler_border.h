#ifndef NATIVE_SAMPLER_BORDER_H
#define NATIVE_SAMPLER_BORDER_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
/* D3D11 float border colors; wgpu's Metal API exposes three exact colors.
   Do not silently substitute an unsupported arbitrary border color. */
static inline unsigned sampler_border_color(const uint32_t bits[4]){
 float x[4];memcpy(x,bits,sizeof(x));for(unsigned i=0;i<4;i++)if(!isfinite(x[i]))return 0;
 if(x[0]==0&&x[1]==0&&x[2]==0){if(x[3]==0)return 1;if(x[3]==1)return 2;}
 if(x[0]==1&&x[1]==1&&x[2]==1&&x[3]==1)return 3;
 return 0;
}
#endif
