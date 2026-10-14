/* Additional READBACK commands through the production shared-ring consumer.
   Raw bits are compared, including half/float NaN encodings and BC blocks. */
static const struct {uint32_t dxgi,bytes,bw;} rb_formats[]={
 {2,16,1},{10,8,1},{16,8,1},{24,4,1},{26,4,1},{28,4,1},{29,4,1},{31,4,1},{34,4,1},{41,4,1},
 {49,2,1},{51,2,1},{54,2,1},{61,1,1},{63,1,1},{65,1,1},{67,4,1},{71,8,4},{72,8,4},{74,16,4},
 {75,16,4},{77,16,4},{78,16,4},{80,8,4},{83,16,4},{87,4,1},{91,4,1},{95,16,4},{98,16,4},{99,16,4}
};
for(unsigned test=0;test<30;test++){
 uint32_t row=rb_formats[test].bw==4?rb_formats[test].bytes:4*rb_formats[test].bytes,rows=rb_formats[test].bw==4?1:4,bytes=row*rows;
 memset(cmd,0,sizeof(cmd));cmd[0]=201+test;cmd[1]=2;cmd[2]=cmd[3]=4;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=rb_formats[test].dxgi;cmd[8]=8;cmd[9]=1;EMIT(3,10);
 memset(cmd,0,sizeof(cmd));cmd[0]=201+test;cmd[6]=cmd[7]=4;cmd[8]=1;cmd[9]=row;cmd[10]=rows;cmd[11]=bytes;
 for(unsigned i=0;i<bytes;i++)((uint8_t*)(cmd+12))[i]=(uint8_t)(i*37+test*13+19);EMIT(13,12+(bytes+3)/4);
 uint64_t out=dst+20000+test*320;uint8_t guards[320];memset(guards,0xa5,sizeof(guards));host_store(h,out,guards,sizeof(guards));
 memset(cmd,0,sizeof(cmd));cmd[0]=201+test;cmd[9]=(uint32_t)(out+16);cmd[10]=(out+16)>>32;cmd[11]=row+4;cmd[12]=(row+4)*rows;cmd[14]=304;EMIT(42,17);
 cmd[0]=201+test;EMIT(4,1);
}
/* Mip 1 in array layer 1, then a boxed 3D read with padded slice stride. */
for(unsigned test=0;test<2;test++){
 memset(cmd,0,sizeof(cmd));cmd[0]=231+test;cmd[1]=test?3:2;cmd[2]=cmd[3]=test?4:8;cmd[4]=test?3:1;cmd[5]=test?1:2;cmd[6]=test?1:2;cmd[7]=test?34:41;cmd[8]=8;cmd[9]=1;EMIT(3,10);
 memset(cmd,0,sizeof(cmd));cmd[0]=231+test;cmd[1]=test?0:1;cmd[2]=test?0:1;cmd[6]=cmd[7]=4;cmd[8]=test?3:1;cmd[9]=16;cmd[10]=4;cmd[11]=test?192:64;
 for(unsigned i=0;i<cmd[11];i++)((uint8_t*)(cmd+12))[i]=(uint8_t)(i+113);EMIT(13,12+cmd[11]/4);
 uint64_t out=dst+30000+test*160;uint8_t guards[128];memset(guards,0xa5,sizeof(guards));host_store(h,out,guards,sizeof(guards));
 memset(cmd,0,sizeof(cmd));cmd[0]=231+test;cmd[1]=test?0:3;cmd[2]=1;cmd[3]=cmd[4]=1;cmd[5]=test?1:0;cmd[6]=cmd[7]=3;cmd[8]=test?3:1;cmd[9]=(uint32_t)(out+16);cmd[10]=(out+16)>>32;cmd[11]=20;cmd[12]=test?56:40;cmd[14]=112;EMIT(42,17);cmd[0]=231+test;EMIT(4,1);
}
memset(cmd,0,sizeof(cmd));cmd[0]=233;cmd[1]=2;cmd[2]=cmd[3]=4;cmd[4]=1;cmd[5]=3;cmd[6]=1;cmd[7]=71;cmd[8]=8;cmd[9]=1;EMIT(3,10);
memset(cmd,0,sizeof(cmd));cmd[0]=233;cmd[1]=1;cmd[6]=cmd[7]=4;cmd[8]=1;cmd[9]=8;cmd[10]=1;cmd[11]=8;cmd[12]=0x12345678;cmd[13]=0xabcdef09;EMIT(13,14);
memset(cmd,0,sizeof(cmd));cmd[0]=233;cmd[1]=1;cmd[9]=(uint32_t)(dst+30400);cmd[10]=(dst+30400)>>32;cmd[11]=8;cmd[12]=8;cmd[14]=8;EMIT(42,17);cmd[0]=233;EMIT(4,1);
/* Cross the staging bound by one row; a second chunk must use the same buffer. */
memset(cmd,0,sizeof(cmd));cmd[0]=234;cmd[1]=2;cmd[2]=2048;cmd[3]=129;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=28;cmd[8]=0x28;cmd[9]=1;EMIT(3,10);
memset(cmd,0,sizeof(cmd));cmd[0]=234;float rb_color[]={0,1,0,1};memcpy(cmd+3,rb_color,sizeof(rb_color));EMIT(5,7);
memset(cmd,0,sizeof(cmd));cmd[0]=234;cmd[9]=65536;cmd[11]=8192;cmd[12]=cmd[14]=1056768;EMIT(42,17);cmd[0]=234;EMIT(4,1);
memset(cmd,0,sizeof(cmd));cmd[0]=235;cmd[1]=READBACK_STAGING_MAX+19;cmd[2]=1;EMIT(11,5);
memset(cmd,0,sizeof(cmd));cmd[0]=235;cmd[1]=READBACK_STAGING_MAX;cmd[2]=16;for(unsigned i=0;i<16;i++)((uint8_t*)(cmd+3))[i]=(uint8_t)(i*3+17);EMIT(12,7);
memset(cmd,0,sizeof(cmd));cmd[0]=235;cmd[9]=1200000;cmd[13]=1;cmd[14]=READBACK_STAGING_MAX+19;EMIT(42,17);
uint8_t rb_guard[32];memset(rb_guard,0xa5,sizeof(rb_guard));host_store(h,dst+30500,rb_guard,sizeof(rb_guard));
memset(cmd,0,sizeof(cmd));cmd[0]=235;cmd[2]=1;cmd[3]=READBACK_STAGING_MAX+4;cmd[6]=READBACK_STAGING_MAX+15;cmd[9]=(uint32_t)(dst+30508);cmd[10]=(dst+30508)>>32;cmd[13]=1;cmd[14]=11;cmd[15]=(uint32_t)(flag+24);cmd[16]=(flag+24)>>32;host_put32(h,flag+24,1);EMIT(42,17);cmd[0]=235;EMIT(4,1);
