#ifndef NATIVE_READBACK_LAYOUT_H
#define NATIVE_READBACK_LAYOUT_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define READBACK_STAGING_MAX (1024u*1024u)
#define READBACK_OUTPUT_MAX (64u*1024u*1024u)
typedef struct {
 uint32_t dimension,width,height,depth,mips,sub;
 uint32_t block_width,block_height,block_bytes;bool alpha_only,box;
 uint32_t left,top,front,right,bottom,back,row_pitch,slice_pitch,capacity;
} ReadbackRequest;
typedef struct {
 uint32_t mip,x,y,z,width,height,depth,rows,gpu_row,output_row,pitch,row_stride,slice_stride,tile_rows;
 uint64_t span;
} ReadbackLayout;
static inline uint32_t rb_mip(uint32_t size,uint32_t level){return level<32&&size>>level?size>>level:1;}
/* All sizes are widened before arithmetic; output is capped before narrowing.
   The GPU copies physical texel blocks; the CPU receives original row/slice
   strides. A8 is stored as RGBA8 by this renderer and must pack alpha back. */
static inline const char *readback_layout(const ReadbackRequest *r,ReadbackLayout *p){
 memset(p,0,sizeof(*p));
 if(r->dimension<1||r->dimension>3||!r->width||!r->height||!r->depth||!r->mips||r->mips>32)return "Invalid readback texture descriptor";
 if(!r->block_width||!r->block_height||!r->block_bytes)return "Unsupported readback texel layout";
 p->mip=r->sub%r->mips;uint32_t layer=r->sub/r->mips;
 if(r->dimension==3?layer!=0:layer>=r->depth)return "Readback subresource outside texture";
 uint32_t mw=rb_mip(r->width,p->mip),mh=r->dimension==1?1:rb_mip(r->height,p->mip),md=r->dimension==3?rb_mip(r->depth,p->mip):1;
 p->x=r->box?r->left:0;p->y=r->box?r->top:0;uint32_t z=r->box?r->front:0;
 uint32_t right=r->box?r->right:mw,bottom=r->box?r->bottom:mh,back=r->box?r->back:md;
 if(p->x>=right||p->y>=bottom||z>=back||right>mw||bottom>mh||back>md)return "Readback box outside texture";
 if(p->x%r->block_width||p->y%r->block_height||(right%r->block_width&&right!=mw)||(bottom%r->block_height&&bottom!=mh))return "Readback box splits a compressed texel block";
 p->width=right-p->x;p->height=bottom-p->y;p->depth=back-z;p->z=r->dimension==3?z:layer;
 uint64_t blocks=((uint64_t)p->width+r->block_width-1)/r->block_width,rows=((uint64_t)p->height+r->block_height-1)/r->block_height;
 uint64_t gpu_row=blocks*r->block_bytes,output_row=r->alpha_only?p->width:gpu_row,pitch=(gpu_row+255)&~UINT64_C(255);
 if(!gpu_row||pitch>READBACK_STAGING_MAX||rows>UINT32_MAX)return "Readback row exceeds staging bound";
 uint64_t row_stride=r->row_pitch?r->row_pitch:output_row;
 if(row_stride<output_row)return "Readback row pitch too small";
 uint64_t slice_span=(rows-1)*row_stride+output_row;
 uint64_t slice_stride=r->slice_pitch?r->slice_pitch:rows*row_stride;
 if(slice_span>READBACK_OUTPUT_MAX||slice_stride>UINT32_MAX)return "Readback slice exceeds destination bound";
 if(p->depth>1&&slice_stride<slice_span)return "Readback slice pitch too small";
 uint64_t span=(p->depth-1)*slice_stride+slice_span;
 if(span>READBACK_OUTPUT_MAX||(r->capacity&&span>r->capacity)||row_stride>UINT32_MAX||slice_stride>UINT32_MAX)return "Readback output exceeds destination capacity";
 p->rows=(uint32_t)rows;p->gpu_row=(uint32_t)gpu_row;p->output_row=(uint32_t)output_row;p->pitch=(uint32_t)pitch;
 p->row_stride=(uint32_t)row_stride;p->slice_stride=(uint32_t)slice_stride;p->span=span;p->tile_rows=READBACK_STAGING_MAX/p->pitch;
 if(p->tile_rows>p->rows)p->tile_rows=p->rows;
 return NULL;
}
#endif
