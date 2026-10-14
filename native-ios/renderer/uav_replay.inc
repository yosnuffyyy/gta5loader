/* Same production SET_UAVS/DISPATCH/COPY_COUNTER/CLEAR_UAV consumers. */
uint64_t uav_dst=base-131072;
for(unsigned i=0;i<29;i++){uint64_t hash=strtoull(compute_fixtures[i].hash,NULL,16);cmd[0]=600+i;cmd[1]=2;cmd[2]=(uint32_t)hash;cmd[3]=hash>>32;cmd[4]=0;EMIT(14,5);cmd[0]=2;cmd[1]=600+i;EMIT(18,2);memset(cmd,0,12);EMIT(39,3);}
cmd[0]=640;cmd[1]=2;cmd[2]=0x160;cmd[3]=0xffffffff;cmd[4]=0;EMIT(14,5);cmd[0]=641;cmd[2]=0x161;EMIT(14,5);
for(unsigned i=0;i<4;i++){cmd[0]=500+i;cmd[1]=256;cmd[2]=0x88;cmd[3]=i==0?0x40:0;cmd[4]=i==0?4:0;EMIT(11,5);}
cmd[0]=506;cmd[1]=128;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);
uint32_t append_view[]={510,1,500,0,1,0,32,0,2};memcpy(cmd,append_view,sizeof(append_view));EMIT(37,9);
append_view[0]=511;append_view[5]=8;append_view[6]=24;append_view[8]=4;memcpy(cmd,append_view,sizeof(append_view));EMIT(37,9);
append_view[0]=512;append_view[2]=501;append_view[3]=39;append_view[5]=4;append_view[6]=8;append_view[8]=1;memcpy(cmd,append_view,sizeof(append_view));EMIT(37,9);
append_view[0]=513;append_view[2]=502;append_view[5]=4;append_view[6]=32;memcpy(cmd,append_view,sizeof(append_view));EMIT(37,9);
append_view[0]=514;append_view[2]=503;append_view[3]=42;append_view[5]=8;append_view[8]=0;memcpy(cmd,append_view,sizeof(append_view));EMIT(37,9);
uint32_t source_view[]={515,1,502,42,1,4,32,0,0};memcpy(cmd,source_view,sizeof(source_view));EMIT(17,9);
cmd[0]=520;cmd[1]=16;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);
uint32_t cs_constants[]={2,0,1,520,0,1};memcpy(cmd,cs_constants,sizeof(cs_constants));EMIT(23,6);
memset(cmd,0,sizeof(cmd));cmd[0]=520;cmd[2]=16;cmd[3]=100;EMIT(12,7);
cmd[0]=2;cmd[1]=640;EMIT(18,2);
for(unsigned pass=0;pass<3;pass++){
 if(pass==2){cmd[0]=520;cmd[1]=0;cmd[2]=4;cmd[3]=200;EMIT(12,4);}
 cmd[0]=2;cmd[1]=0;cmd[2]=1;cmd[3]=pass==2?511:510;cmd[4]=pass==1?UINT32_MAX:2;EMIT(38,5);
 cmd[0]=cmd[1]=cmd[2]=1;EMIT(39,3);cmd[0]=506;cmd[1]=pass*4;cmd[2]=pass==2?511:510;EMIT(43,3);
}
cmd[0]=506;cmd[1]=12;cmd[2]=510;EMIT(43,3);
memset(cmd,0,sizeof(cmd));cmd[0]=500;cmd[9]=(uint32_t)uav_dst;cmd[10]=uav_dst>>32;cmd[13]=1;cmd[14]=256;EMIT(42,17);
cmd[0]=510;cmd[1]=0;cmd[2]=77;cmd[3]=88;cmd[4]=99;cmd[5]=111;EMIT(40,6);cmd[0]=506;cmd[1]=16;cmd[2]=510;EMIT(43,3);
memset(cmd,0,sizeof(cmd));cmd[0]=500;cmd[9]=(uint32_t)(uav_dst+256);cmd[10]=(uav_dst+256)>>32;cmd[13]=1;cmd[14]=256;EMIT(42,17);
cmd[0]=2;cmd[1]=0;cmd[2]=1;cmd[3]=512;cmd[4]=42;EMIT(38,5);
cmd[0]=512;cmd[1]=0;cmd[2]=0xabcdef01;cmd[3]=2;cmd[4]=3;cmd[5]=4;EMIT(40,6);
memset(cmd,0,sizeof(cmd));cmd[0]=501;cmd[9]=(uint32_t)(uav_dst+512);cmd[10]=(uav_dst+512)>>32;cmd[13]=1;cmd[14]=256;EMIT(42,17);
memset(cmd,0,sizeof(cmd));cmd[0]=512;EMIT(40,6);
memset(cmd,0,sizeof(cmd));cmd[0]=501;cmd[9]=(uint32_t)(uav_dst+768);cmd[10]=(uav_dst+768)>>32;cmd[13]=1;cmd[14]=256;EMIT(42,17);
/* Execute the game's unmodified CS_GpuFill512 and CS_GpuCopy512. Both views
   start away from storage alignment: only the selected ranges may change. */
uint32_t fill_shader=0,copy_shader=0;for(unsigned i=0;i<29;i++){if(!strcmp(compute_fixtures[i].hash,"14c5e4cf01998350"))fill_shader=600+i;if(!strcmp(compute_fixtures[i].hash,"3b52bb04d4fa2723"))copy_shader=600+i;}
cmd[0]=520;cmd[1]=0;cmd[2]=16;cmd[3]=128;cmd[4]=4096;cmd[5]=0xdecafbad;cmd[6]=0;EMIT(12,7);
cmd[0]=2;cmd[1]=0;cmd[2]=1;cmd[3]=513;cmd[4]=0;EMIT(38,5);cmd[0]=2;cmd[1]=fill_shader;EMIT(18,2);cmd[0]=cmd[1]=cmd[2]=1;EMIT(39,3);
cmd[0]=2;cmd[1]=0;cmd[2]=1;cmd[3]=514;cmd[4]=UINT32_MAX;EMIT(38,5);cmd[3]=515;EMIT(24,4);cmd[0]=2;cmd[1]=copy_shader;EMIT(18,2);cmd[0]=cmd[1]=cmd[2]=1;EMIT(39,3);
for(unsigned i=0;i<2;i++){memset(cmd,0,sizeof(cmd));cmd[0]=502+i;uint64_t at=uav_dst+1024+i*256;cmd[9]=(uint32_t)at;cmd[10]=at>>32;cmd[13]=1;cmd[14]=256;EMIT(42,17);}
for(unsigned i=0;i<2;i++){cmd[0]=530+i;cmd[1]=2;cmd[2]=cmd[3]=8;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=i?10:41;cmd[8]=0x88;cmd[9]=1;EMIT(3,10);uint32_t view[]={532+i,0,530+i,i?10:41,4,0,0,0,0};memcpy(cmd,view,sizeof(view));EMIT(37,9);view[0]=534+i;view[6]=view[8]=1;memcpy(cmd,view,sizeof(view));EMIT(17,9);}
cmd[0]=2;cmd[1]=641;EMIT(18,2);
for(unsigned i=0;i<2;i++){cmd[0]=2;cmd[1]=0;cmd[2]=1;cmd[3]=532+i;cmd[4]=0;EMIT(38,5);cmd[0]=cmd[1]=cmd[2]=1;EMIT(39,3);memset(cmd,0,sizeof(cmd));cmd[0]=530+i;uint64_t at=uav_dst+2048+i*1024;cmd[9]=(uint32_t)at;cmd[10]=at>>32;cmd[11]=i?64:32;cmd[12]=cmd[14]=i?512:256;EMIT(42,17);}
/* Same shader and source format reuse, then a return to the first variant. */
cmd[0]=2;cmd[1]=0;cmd[2]=1;cmd[3]=532;cmd[4]=UINT32_MAX;EMIT(38,5);cmd[0]=cmd[1]=cmd[2]=1;EMIT(39,3);
/* Original game vertex/fragment pair samples the actual compute outputs. */
uint32_t draw_cbv[]={0,1,1,111,1,12};memcpy(cmd,draw_cbv,sizeof(draw_cbv));EMIT(23,6);uint32_t draw_cbp[]={1,2,1,112,0,16};memcpy(cmd,draw_cbp,sizeof(draw_cbp));EMIT(23,6);
cmd[0]=0;cmd[1]=101;EMIT(18,2);cmd[0]=1;cmd[1]=102;EMIT(18,2);cmd[0]=103;EMIT(19,1);cmd[0]=4;EMIT(20,1);uint32_t uvb[]={0,1,109,24,0};memcpy(cmd,uvb,sizeof(uvb));EMIT(21,5);cmd[0]=104;EMIT(29,1);cmd[0]=110;cmd[1]=57;cmd[2]=0;EMIT(22,3);cmd[0]=0;cmd[1]=0;EMIT(28,2);
memset(cmd,0,sizeof(cmd));cmd[0]=116;EMIT(26,37);
for(unsigned i=0;i<2;i++){cmd[0]=1;cmd[1]=2;cmd[2]=1;cmd[3]=534+i;EMIT(24,4);cmd[3]=108;EMIT(25,4);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;cmd[3]=6;EMIT(32,6);memset(cmd,0,sizeof(cmd));cmd[0]=116;uint64_t at=uav_dst+4096+i*1024;cmd[9]=(uint32_t)at;cmd[10]=at>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);cmd[0]=116;EMIT(7,1);}
cmd[0]=532;cmd[1]=1;float clears[]={-.25f,.5f,.75f,1};memcpy(cmd+2,clears,16);EMIT(40,6);memset(cmd,0,sizeof(cmd));cmd[0]=530;cmd[9]=(uint32_t)(uav_dst+6144);cmd[10]=(uav_dst+6144)>>32;cmd[11]=32;cmd[12]=cmd[14]=256;EMIT(42,17);
cmd[0]=533;cmd[1]=0;cmd[2]=0x3c00;cmd[3]=0x3800;cmd[4]=0;cmd[5]=0x3c00;EMIT(40,6);memset(cmd,0,sizeof(cmd));cmd[0]=531;cmd[9]=(uint32_t)(uav_dst+7168);cmd[10]=(uav_dst+7168)>>32;cmd[11]=64;cmd[12]=cmd[14]=512;EMIT(42,17);
memset(cmd,0,sizeof(cmd));cmd[0]=532;EMIT(40,6);memset(cmd,0,sizeof(cmd));cmd[0]=530;cmd[9]=(uint32_t)(uav_dst+8192);cmd[10]=(uav_dst+8192)>>32;cmd[11]=32;cmd[12]=cmd[14]=256;EMIT(42,17);
/* A complete drain batch exercises unique reset slots past their bound. */
while(fixture_commands%256)EMIT(0,0);
for(unsigned i=0;i<140;i++){cmd[0]=2;cmd[1]=0;cmd[2]=8;for(unsigned k=0;k<8;k++){cmd[3+k]=510;cmd[11+k]=i*8+k;}EMIT(38,19);if(i==0||i==126||i==127||i==128||i==139){cmd[0]=506;cmd[1]=32+(i==0?0:i==126?1:i==127?2:i==128?3:4)*4;cmd[2]=510;EMIT(43,3);}}
memset(cmd,0,sizeof(cmd));cmd[0]=506;cmd[9]=(uint32_t)(uav_dst+9216);cmd[10]=(uav_dst+9216)>>32;cmd[13]=1;cmd[14]=128;EMIT(42,17);
EMIT(33,0);for(unsigned i=0;i<6;i++){cmd[0]=510+i;EMIT(4,1);}for(unsigned i=0;i<4;i++){cmd[0]=500+i;EMIT(4,1);}for(unsigned i=0;i<2;i++){cmd[0]=530+i;EMIT(4,1);}
