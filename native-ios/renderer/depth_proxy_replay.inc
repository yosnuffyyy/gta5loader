/* Actual DRAW/SRV consumers, original IM shader, and two diagnostic variants.
   No direct proxy calls: writes must invalidate cached groups/views correctly. */
for(unsigned i=0;i<5;i++){
 cmd[0]=400+i;cmd[1]=2;cmd[2]=cmd[3]=i==1||i==4?8:16;cmd[4]=1;cmd[5]=i==2?1:2;cmd[6]=i==2?1:2;cmd[7]=i==1||i==4?41:i==2?45:40;cmd[8]=i==1||i==4?0x28:0x48;cmd[9]=1;EMIT(3,10);
}
uint32_t depth_srv[]={410,0,400,41,4,0,1,0,1};memcpy(cmd,depth_srv,sizeof(depth_srv));EMIT(17,9);
depth_srv[0]=411;depth_srv[2]=403;memcpy(cmd,depth_srv,sizeof(depth_srv));EMIT(17,9);
depth_srv[0]=412;depth_srv[2]=400;depth_srv[5]=1;depth_srv[7]=1;memcpy(cmd,depth_srv,sizeof(depth_srv));EMIT(17,9);
depth_srv[0]=413;depth_srv[2]=402;depth_srv[3]=47;depth_srv[5]=depth_srv[7]=0;memcpy(cmd,depth_srv,sizeof(depth_srv));EMIT(17,9);
depth_srv[0]=414;depth_srv[2]=401;depth_srv[3]=41;memcpy(cmd,depth_srv,sizeof(depth_srv));EMIT(17,9);
depth_srv[0]=415;depth_srv[2]=404;memcpy(cmd,depth_srv,sizeof(depth_srv));EMIT(17,9);
cmd[0]=420;cmd[1]=1;cmd[2]=0x151;cmd[3]=0xffffffff;cmd[4]=0;EMIT(14,5);
cmd[0]=421;cmd[2]=0x152;EMIT(14,5);
uint32_t compare_sampler[]={422,3,0x80,3,3,3,0,0,4,0,0,0,0,0,0x42000000};memcpy(cmd,compare_sampler,sizeof(compare_sampler));EMIT(16,15);
uint32_t proxy_cbv[]={0,1,1,111,1,12};memcpy(cmd,proxy_cbv,sizeof(proxy_cbv));EMIT(23,6);
uint32_t proxy_cbp[]={1,2,1,112,0,16};memcpy(cmd,proxy_cbp,sizeof(proxy_cbp));EMIT(23,6);
memset(cmd,0,sizeof(cmd));cmd[0]=112;cmd[2]=256;float proxy_constants[64]={0};proxy_constants[60]=proxy_constants[63]=1;memcpy(cmd+3,proxy_constants,sizeof(proxy_constants));EMIT(12,67);
cmd[0]=111;cmd[1]=16+11*16+8;cmd[2]=4;cmd[3]=0;EMIT(12,4);
cmd[0]=0;cmd[1]=101;EMIT(18,2);cmd[0]=103;EMIT(19,1);cmd[0]=4;EMIT(20,1);
uint32_t proxy_vb[]={0,1,109,24,0};memcpy(cmd,proxy_vb,sizeof(proxy_vb));EMIT(21,5);cmd[0]=104;EMIT(29,1);
cmd[0]=110;cmd[1]=57;cmd[2]=0;EMIT(22,3);
memset(cmd,0,sizeof(cmd));for(unsigned i=1;i<5;i++)cmd[i]=0x3f800000;cmd[5]=UINT32_MAX;EMIT(27,6);
static const float proxy_expected[]={.25f,.25f,.75f,.5f,.125f,.125f,37.f/255,201.f/255,0,1,0,0,.125f,0};
for(unsigned test=0;test<14;test++){
 if(test==0||test==2||test==3||test==6||test==7){memset(cmd,0,sizeof(cmd));cmd[0]=test>=6?402:400;cmd[1]=test==3?1:0;cmd[2]=test==3?1:0;cmd[3]=test>=6?3:1;float z=proxy_expected[test];memcpy(cmd+4,&z,4);cmd[5]=test==6?37:test==7?201:0;EMIT(6,6);}
 if(test==8||test==9){memset(cmd,0,sizeof(cmd));cmd[0]=401;float z=test==8?.25f:.75f;memcpy(cmd+3,&z,4);EMIT(5,7);}
 if(test==4){
  cmd[0]=111;cmd[1]=16+11*16+8;cmd[2]=4;float z=.125f;memcpy(cmd+3,&z,4);EMIT(12,4);
  memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[32]=400;EMIT(26,37);cmd[0]=107;cmd[1]=0;EMIT(28,2);cmd[0]=1;cmd[1]=102;EMIT(18,2);cmd[0]=1;cmd[1]=2;cmd[2]=1;cmd[3]=115;EMIT(24,4);cmd[3]=108;EMIT(25,4);
  memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;cmd[3]=6;EMIT(32,6);
 }
 if(test==5){cmd[0]=403;cmd[1]=400;EMIT(35,2);}
 if(test==10){memset(cmd,0,sizeof(cmd));cmd[0]=401;cmd[6]=cmd[7]=8;cmd[8]=1;cmd[9]=32;cmd[10]=8;cmd[11]=256;for(unsigned i=0;i<64;i++){float z=.125f;memcpy(cmd+12+i,&z,4);}EMIT(13,76);}
 if(test==11){cmd[0]=404;cmd[1]=401;EMIT(35,2);}
 memset(cmd,0,sizeof(cmd));cmd[0]=116;if(test==12){cmd[32]=400;cmd[36]=3;}EMIT(26,37);cmd[0]=cmd[1]=0;EMIT(28,2);
 cmd[0]=1;cmd[1]=test==6||test==7?420:((test>=8&&test<=11)||test==13)?421:102;EMIT(18,2);
 cmd[0]=1;cmd[1]=2;cmd[2]=1;cmd[3]=test==3?412:test==5?411:test==6||test==7?413:test==11?415:test>=8&&test<=10?414:410;EMIT(24,4);
 cmd[3]=((test>=8&&test<=11)||test==13)?422:108;EMIT(25,4);
 memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;cmd[3]=6;EMIT(32,6);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;uint64_t out=dst+44000+test*1024;cmd[9]=(uint32_t)out;cmd[10]=out>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
}
EMIT(33,0);for(unsigned i=0;i<5;i++){cmd[0]=400+i;EMIT(4,1);}
