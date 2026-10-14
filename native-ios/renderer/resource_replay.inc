/* Partial updates start with GPU-copied contents, never a CPU shadow. */
static const uint32_t edge_ranges[][2]={{0,1},{4,3},{1,6},{7,2},{63,1},{3,61}};
for(unsigned t=0;t<6;t++){
 cmd[0]=9000+t;cmd[1]=64;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);cmd[0]=9000+t;cmd[1]=11;EMIT(35,2);
 for(unsigned repetition=0;repetition<2;repetition++){memset(cmd,0,sizeof(cmd));cmd[0]=9000+t;cmd[1]=edge_ranges[t][0];cmd[2]=edge_ranges[t][1];for(unsigned j=0;j<cmd[2];j++)((uint8_t*)(cmd+3))[j]=(uint8_t)(t*17+j+101+repetition);EMIT(repetition?47:12,3+(cmd[2]+3)/4);}
 memset(cmd,0,sizeof(cmd));cmd[0]=9000+t;cmd[9]=(uint32_t)(dst+32000+t*64);cmd[10]=(dst+32000+t*64)>>32;cmd[13]=1;cmd[14]=64;EMIT(42,17);cmd[0]=9000+t;EMIT(4,1);
}
/* Compare batched whole copies with the prior per-subresource algorithm, via
   COPY_REGION, for all mips/layers and 3D depth slices. Every texel is checked. */
for(unsigned t=0;t<2;t++){
 for(unsigned copy=0;copy<3;copy++){memset(cmd,0,sizeof(cmd));cmd[0]=9100+t*3+copy;cmd[1]=t?3:2;cmd[2]=cmd[3]=t?4:8;cmd[4]=t?4:1;cmd[5]=t?3:4;cmd[6]=t?1:3;cmd[7]=28;cmd[8]=8;cmd[9]=1;EMIT(3,10);}
 unsigned mips=t?3:4,layers=t?1:3;
 for(unsigned layer=0;layer<layers;layer++)for(unsigned mip=0;mip<mips;mip++){unsigned w=(t?4:8)>>mip,depth=t?w:1,bytes=w*w*depth*4;memset(cmd,0,sizeof(cmd));cmd[0]=9100+t*3;cmd[1]=mip;cmd[2]=t?0:layer;cmd[6]=cmd[7]=w;cmd[8]=depth;cmd[9]=w*4;cmd[10]=w;cmd[11]=bytes;for(unsigned i=0;i<bytes;i++)((uint8_t*)(cmd+12))[i]=(uint8_t)(t*71+layer*17+mip*23+i);EMIT(13,12+bytes/4);}
 for(unsigned layer=0;layer<layers;layer++)for(unsigned mip=0;mip<mips;mip++){memset(cmd,0,sizeof(cmd));cmd[0]=9102+t*3;cmd[1]=layer*mips+mip;cmd[5]=9100+t*3;cmd[6]=cmd[1];EMIT(34,14);}
 cmd[0]=9101+t*3;cmd[1]=9100+t*3;EMIT(35,2);cmd[0]=9100+t*3;EMIT(4,1);
 for(unsigned copy=0;copy<2;copy++)for(unsigned layer=0;layer<layers;layer++)for(unsigned mip=0;mip<mips;mip++){unsigned w=(t?4:8)>>mip,depth=t?w:1;uint64_t out=dst+33000+t*8192+copy*4096+(layer*mips+mip)*320;memset(cmd,0,sizeof(cmd));cmd[0]=9101+t*3+copy;cmd[1]=layer*mips+mip;cmd[9]=(uint32_t)out;cmd[10]=out>>32;cmd[11]=w*4;cmd[12]=w*w*4;cmd[14]=w*w*depth*4;EMIT(42,17);}
 cmd[0]=9101+t*3;EMIT(4,1);cmd[0]=9102+t*3;EMIT(4,1);
}
/* Resolve the same MSAA source twice. ResolveSubresource must retain source. */
for(unsigned t=0;t<3;t++){memset(cmd,0,sizeof(cmd));cmd[0]=9200+t;cmd[1]=2;cmd[2]=cmd[3]=4;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=28;cmd[8]=0x28;cmd[9]=t?1:4;EMIT(3,10);}
memset(cmd,0,sizeof(cmd));cmd[0]=9200;float resolve_color[]={.25f,.5f,.75f,1};memcpy(cmd+3,resolve_color,16);EMIT(5,7);
for(unsigned t=0;t<2;t++){cmd[0]=9201+t;cmd[1]=0;cmd[2]=9200;cmd[3]=0;EMIT(36,4);memset(cmd,0,sizeof(cmd));cmd[0]=9201+t;cmd[9]=(uint32_t)(dst+51000+t*64);cmd[10]=(dst+51000+t*64)>>32;cmd[11]=16;cmd[12]=cmd[14]=64;EMIT(42,17);cmd[0]=9201+t;EMIT(4,1);}
cmd[0]=9200;EMIT(4,1);
/* BC small mips occupy whole compressed blocks despite virtual 2x2/1x1 size. */
for(unsigned i=0;i<2;i++){memset(cmd,0,sizeof(cmd));cmd[0]=9300+i;cmd[1]=2;cmd[2]=cmd[3]=4;cmd[4]=1;cmd[5]=3;cmd[6]=2;cmd[7]=71;cmd[8]=8;cmd[9]=1;EMIT(3,10);}
for(unsigned layer=0;layer<2;layer++)for(unsigned mip=0;mip<3;mip++){memset(cmd,0,sizeof(cmd));cmd[0]=9300;cmd[1]=mip;cmd[2]=layer;cmd[6]=cmd[7]=4;cmd[8]=1;cmd[9]=8;cmd[10]=1;cmd[11]=8;cmd[12]=0x12345678+layer*17+mip;cmd[13]=0xabcdef09+layer*23+mip;EMIT(13,14);}
cmd[0]=9301;cmd[1]=9300;EMIT(35,2);cmd[0]=9300;EMIT(4,1);
for(unsigned sub=0;sub<6;sub++){memset(cmd,0,sizeof(cmd));cmd[0]=9301;cmd[1]=sub;cmd[9]=(uint32_t)(dst+51500+sub*8);cmd[10]=(dst+51500+sub*8)>>32;cmd[11]=cmd[12]=cmd[14]=8;EMIT(42,17);}cmd[0]=9301;EMIT(4,1);
