/* Included inside shared-ring replay while EMIT is defined. All commands pass
   through the production DRAW consumer, with the original 0.7 shader pair. */
cmd[0]=101;cmd[1]=0;cmd[2]=0x9df627f5;cmd[3]=0x02892187;cmd[4]=3;
cmd[5]=1;cmd[6]=0;cmd[7]=0;cmd[8]=7;cmd[9]=2;cmd[10]=0;cmd[11]=2;cmd[12]=15;cmd[13]=3;cmd[14]=0;cmd[15]=3;cmd[16]=3;EMIT(14,17);
cmd[0]=102;cmd[1]=1;cmd[2]=0x542193ff;cmd[3]=0x943b0ad9;cmd[4]=0;EMIT(14,5);
uint32_t els[]={103,3,1,0,6,0,0,0,0,2,0,28,0,12,0,0,3,0,16,0,16,0,0};memcpy(cmd,els,sizeof(els));EMIT(15,23);
uint32_t raster[]={104,2,3,1,0,0,0,0,1,0,0,0};memcpy(cmd,raster,sizeof(raster));EMIT(16,12);
raster[0]=105;raster[9]=1;memcpy(cmd,raster,sizeof(raster));EMIT(16,12);
memset(cmd,0,sizeof(cmd));cmd[0]=106;cmd[1]=0;cmd[4]=1;cmd[5]=5;cmd[6]=6;cmd[7]=1;cmd[8]=2;cmd[9]=1;cmd[10]=1;cmd[11]=15;EMIT(16,68);
uint32_t depth_words[]={107,1,1,1,2,0,0xffff,1,1,1,8,1,1,1,8};memcpy(cmd,depth_words,sizeof(depth_words));EMIT(16,15);
memset(cmd,0,sizeof(cmd));cmd[0]=108;cmd[1]=3;cmd[3]=cmd[4]=cmd[5]=3;float lod=32;memcpy(cmd+14,&lod,4);EMIT(16,15);
cmd[0]=109;cmd[1]=288;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);
memset(cmd,0,sizeof(cmd));cmd[0]=109;cmd[2]=288;float quad[6][2]={{-1,1},{1,1},{-1,-1},{-1,-1},{1,1},{1,-1}};
for(unsigned i=0;i<12;i++){uint8_t *v=(uint8_t*)(cmd+3)+i*24;float position[]={quad[i%6][0],quad[i%6][1],0};memcpy(v,position,12);memset(v+12,255,4);float uv[]={.5f,.5f};memcpy(v+16,uv,8);}EMIT(12,75);
cmd[0]=110;cmd[1]=12;cmd[2]=2;cmd[3]=cmd[4]=0;EMIT(11,5);memset(cmd,0,sizeof(cmd));cmd[0]=110;cmd[2]=12;for(unsigned i=0;i<6;i++)((uint16_t*)(cmd+3))[i]=i;EMIT(12,6);
cmd[0]=111;cmd[1]=208;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);memset(cmd,0,sizeof(cmd));cmd[0]=111;cmd[2]=208;float matrix[52]={0};matrix[4+8*4]=1;matrix[4+9*4+1]=1;matrix[4+10*4+2]=1;matrix[4+11*4+3]=1;memcpy(cmd+3,matrix,sizeof(matrix));EMIT(12,55);
cmd[0]=112;cmd[1]=256;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);
cmd[0]=113;cmd[1]=36;cmd[2]=0;cmd[3]=0x10;cmd[4]=0;EMIT(11,5);uint32_t args[]={113,0,36,6,2,6,0,6,2,0,6,0};memcpy(cmd,args,sizeof(args));EMIT(12,12);
cmd[0]=114;cmd[1]=2;cmd[2]=cmd[3]=1;cmd[4]=1;cmd[5]=cmd[6]=1;cmd[7]=28;cmd[8]=8;cmd[9]=1;EMIT(3,10);
memset(cmd,0,sizeof(cmd));cmd[0]=114;cmd[6]=cmd[7]=cmd[8]=1;cmd[9]=4;cmd[10]=1;cmd[11]=4;cmd[12]=0xff3264c8;EMIT(13,13);
uint32_t srv[]={115,0,114,28,4,0,1,0,1};memcpy(cmd,srv,sizeof(srv));EMIT(17,9);
cmd[0]=116;cmd[1]=2;cmd[2]=cmd[3]=16;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=28;cmd[8]=0x28;cmd[9]=1;EMIT(3,10);
cmd[0]=117;cmd[1]=2;cmd[2]=cmd[3]=16;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=40;cmd[8]=0x40;cmd[9]=1;EMIT(3,10);
cmd[0]=0;cmd[1]=101;EMIT(18,2);cmd[0]=1;cmd[1]=102;EMIT(18,2);cmd[0]=103;EMIT(19,1);cmd[0]=4;EMIT(20,1);
uint32_t vb[]={0,1,109,24,0};memcpy(cmd,vb,sizeof(vb));EMIT(21,5);cmd[0]=110;cmd[1]=57;cmd[2]=0;EMIT(22,3);
uint32_t cbv[]={0,1,1,111,1,12};memcpy(cmd,cbv,sizeof(cbv));EMIT(23,6);uint32_t cbp[]={1,2,1,112,0,16};memcpy(cmd,cbp,sizeof(cbp));EMIT(23,6);
cmd[0]=1;cmd[1]=2;cmd[2]=1;cmd[3]=115;EMIT(24,4);cmd[3]=108;EMIT(25,4);
for(unsigned test=0;test<10;test++){
 memset(cmd,0,sizeof(cmd));cmd[0]=116;float clear_color[]={test==7?.2f:0,test==7?.1f:0,test==7?.3f:0,test==7?.4f:0};memcpy(cmd+3,clear_color,16);EMIT(5,7);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;if(test>=8)cmd[32]=117;EMIT(26,37);
 if(test>=8){memset(cmd,0,sizeof(cmd));cmd[0]=117;cmd[3]=1;float z=.5f;memcpy(cmd+4,&z,4);EMIT(6,6);cmd[0]=111;cmd[1]=16+11*16+8;cmd[2]=4;z=test==8?.75f:.25f;memcpy(cmd+3,&z,4);EMIT(12,4);}
 cmd[0]=test>=8?107:0;cmd[1]=0;EMIT(28,2);cmd[0]=test==6?105:104;EMIT(29,1);cmd[0]=1;cmd[1]=4;cmd[2]=3;cmd[3]=12;cmd[4]=13;EMIT(31,5);
 memset(cmd,0,sizeof(cmd));cmd[0]=test==7?106:0;for(unsigned i=1;i<5;i++)cmd[i]=0x3f800000;cmd[5]=UINT32_MAX;EMIT(27,6);
 memset(cmd,0,sizeof(cmd));cmd[0]=112;cmd[2]=256;float color[64]={0};color[60]=test==7?.5f:1;color[63]=test==7?.75f:(test%3+1)*.25f;memcpy(cmd+3,color,sizeof(color));EMIT(12,67);
 memset(cmd,0,sizeof(cmd));cmd[0]=test<6?test:0;cmd[1]=6;cmd[2]=test==2||test==3?2:1;cmd[3]=test==1||test==3?0:6;cmd[4]=test==1||test==3?6:0;cmd[5]=0;if(test==4||test==5){cmd[1]=113;cmd[2]=test==4?16:0;}EMIT(32,6);
 /* Two consecutive identical draws exercise pipeline/BG reuse and batching. */
 if(test==0)EMIT(32,6);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;uint64_t out=dst+4096+test*1024;cmd[9]=(uint32_t)out;cmd[10]=out>>32;cmd[11]=64;cmd[12]=1024;cmd[14]=1024;EMIT(42,17);
}
EMIT(33,0);
