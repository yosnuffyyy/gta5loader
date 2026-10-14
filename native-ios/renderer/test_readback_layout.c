#include "readback_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 ReadbackRequest base={.dimension=2,.width=8,.height=8,.depth=2,.mips=2,.sub=3,.block_width=1,.block_height=1,.block_bytes=4};
 ReadbackLayout l;assert(!readback_layout(&base,&l));assert(l.mip==1&&l.z==1&&l.width==4&&l.height==4&&l.gpu_row==16&&l.pitch==256&&l.span==64);
 ReadbackRequest r=base;r.box=true;r.left=r.top=1;r.right=r.bottom=3;r.back=1;r.row_pitch=20;r.slice_pitch=40;r.capacity=28;
 assert(!readback_layout(&r,&l));assert(l.x==1&&l.y==1&&l.span==28&&l.output_row==8);
 r.capacity=27;assert(readback_layout(&r,&l));r.capacity=0;r.row_pitch=7;assert(readback_layout(&r,&l));
 r=base;r.dimension=3;r.depth=3;r.mips=1;r.sub=0;r.box=true;r.left=r.top=r.front=1;r.right=r.bottom=r.back=3;r.row_pitch=20;r.slice_pitch=56;
 assert(!readback_layout(&r,&l));assert(l.z==1&&l.depth==2&&l.span==84);r.slice_pitch=27;assert(readback_layout(&r,&l));r.sub=1;assert(readback_layout(&r,&l));
 r=base;r.sub=4;assert(readback_layout(&r,&l));r=base;r.box=true;r.right=9;r.bottom=r.back=1;assert(readback_layout(&r,&l));r.right=0;assert(readback_layout(&r,&l));
 r=base;r.mips=0;assert(readback_layout(&r,&l));r.mips=33;assert(readback_layout(&r,&l));r=base;r.block_bytes=0;assert(readback_layout(&r,&l));
 r=base;r.width=r.height=4;r.depth=1;r.sub=1;r.block_width=r.block_height=4;r.block_bytes=8;
 assert(!readback_layout(&r,&l));assert(l.width==2&&l.height==2&&l.gpu_row==8&&l.rows==1&&l.span==8);
 r.box=true;r.left=1;r.right=r.bottom=2;r.back=1;assert(readback_layout(&r,&l));
 r=base;r.width=2048;r.height=129;r.depth=r.mips=1;r.sub=0;assert(!readback_layout(&r,&l));assert(l.tile_rows==128&&l.span==1056768);
 r.width=UINT32_MAX;assert(readback_layout(&r,&l));r.width=8;r.height=UINT32_MAX;assert(readback_layout(&r,&l));
 r.height=8;r.row_pitch=UINT32_MAX;r.slice_pitch=UINT32_MAX;r.depth=UINT32_MAX;r.dimension=3;assert(readback_layout(&r,&l));
 r=base;r.alpha_only=true;assert(!readback_layout(&r,&l));assert(l.gpu_row==16&&l.output_row==4&&l.span==16);
 puts("PASS: readback layout: mip/array/3D boxes, compressed edge blocks, A8 packing, row/slice strides, capacity/overflow rejection and 1 MiB staging bound.");return 0;
}
