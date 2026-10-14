uint64_t inter_stage_dst=dst+120000;
_Static_assert(120000+3*1024<=131072,"Replay varying output exceeds reserved memory");
/* Derived original IM shaders test both consumed high locations and an unused
   high output alongside real clip distances. Production linker and DRAW used. */
for(unsigned t=0;t<3;t++){
 cmd[0]=9400+t*2;cmd[1]=0;cmd[2]=0x190+t*2;cmd[3]=0xffffffff;cmd[4]=3;cmd[5]=1;cmd[6]=0;cmd[7]=0;cmd[8]=7;cmd[9]=2;cmd[10]=0;cmd[11]=2;cmd[12]=15;cmd[13]=3;cmd[14]=0;cmd[15]=3;cmd[16]=3;EMIT(14,17);
 cmd[0]=9401+t*2;cmd[1]=1;cmd[2]=t==2?0x542193ff:0x191+t*2;cmd[3]=t==2?0x943b0ad9:0xffffffff;cmd[4]=0;EMIT(14,5);
 EMIT(33,0);cmd[0]=0;cmd[1]=9400+t*2;EMIT(18,2);cmd[0]=1;cmd[1]=9401+t*2;EMIT(18,2);cmd[0]=103;EMIT(19,1);cmd[0]=104;EMIT(29,1);
 uint32_t vertex[]={0,1,109,24,0};memcpy(cmd,vertex,sizeof(vertex));EMIT(21,5);uint32_t vc[]={0,1,1,111,1,12};memcpy(cmd,vc,sizeof(vc));EMIT(23,6);uint32_t pc[]={1,2,1,112,0,16};memcpy(cmd,pc,sizeof(pc));EMIT(23,6);
 cmd[0]=1;cmd[1]=2;cmd[2]=1;cmd[3]=115;EMIT(24,4);cmd[3]=108;EMIT(25,4);
 memset(cmd,0,sizeof(cmd));cmd[0]=112;cmd[2]=256;float constants[64]={0};constants[60]=constants[63]=1;memcpy(cmd+3,constants,sizeof(constants));EMIT(12,67);
 memset(cmd,0,sizeof(cmd));cmd[0]=109;cmd[2]=288;for(unsigned i=0;i<12;i++){uint8_t *v=(uint8_t*)(cmd+3)+i*24;float position[]={quad[i%6][0],quad[i%6][1],0};memcpy(v,position,12);memset(v+12,t==1&&i%3?0:255,4);float uv[]={.5f,.5f};memcpy(v+16,uv,8);}EMIT(12,75);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;EMIT(5,7);memset(cmd,0,sizeof(cmd));cmd[0]=116;EMIT(26,37);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)(inter_stage_dst+t*1024);cmd[10]=(inter_stage_dst+t*1024)>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
}
EMIT(33,0);
