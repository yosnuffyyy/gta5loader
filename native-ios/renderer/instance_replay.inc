/* Original IM shader with a diagnostic InstanceID output. Ten cases validate
   constant/divided streams, mixed rates and ID continuity across split draws. */
cmd[0]=305;cmd[1]=0;cmd[2]=0x00000014;cmd[3]=0xffffffff;cmd[4]=3;
cmd[5]=1;cmd[6]=0;cmd[7]=0;cmd[8]=7;cmd[9]=2;cmd[10]=0;cmd[11]=2;cmd[12]=15;cmd[13]=3;cmd[14]=0;cmd[15]=3;cmd[16]=3;EMIT(14,17);
for(unsigned layout=0;layout<5;layout++){
 uint32_t rate=layout==0?0:layout==1||layout==3?2:layout==2?3:1;
 uint32_t els[]={306+layout,3,1,0,6,2,0,1,1,2,0,28,1,0,1,rate,3,0,16,layout==3?3:0,0,layout==3?1:0,layout==3?3:0};memcpy(cmd,els,sizeof(els));EMIT(15,23);
}
static const uint8_t instance_colors[8][4]={{255,0,0,255},{0,255,0,255},{0,0,255,255},{255,255,0,255},{255,0,255,255},{0,255,255,255},{255,255,255,255},{0,0,0,255}};
cmd[0]=301;cmd[1]=96;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);
memset(cmd,0,sizeof(cmd));cmd[0]=301;cmd[2]=96;for(unsigned i=0;i<8;i++){float pos[]={2.f*(2*i+1.5f)/16-1,1-2.f*8.5f/16,0};memcpy((uint8_t*)(cmd+3)+i*12,pos,sizeof(pos));}EMIT(12,27);
cmd[0]=302;cmd[1]=32;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);memset(cmd,0,sizeof(cmd));cmd[0]=302;cmd[2]=32;memcpy(cmd+3,instance_colors,32);EMIT(12,11);
cmd[0]=303;cmd[1]=64;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);memset(cmd,0,sizeof(cmd));cmd[0]=303;cmd[2]=64;for(unsigned i=0;i<8;i++){float uv[]={i%2?1.f:0.f,.5f};memcpy((uint8_t*)(cmd+3)+i*8,uv,sizeof(uv));}EMIT(12,19);
cmd[0]=304;cmd[1]=36;cmd[2]=0;cmd[3]=0x10;cmd[4]=0;EMIT(11,5);uint32_t instance_args[]={304,0,36,1,5,0,1,1,3,0,0,2};memcpy(cmd,instance_args,sizeof(instance_args));EMIT(12,12);
memset(cmd,0,sizeof(cmd));cmd[0]=112;cmd[2]=256;float instance_constants[64]={0};instance_constants[60]=instance_constants[63]=1;memcpy(cmd+3,instance_constants,sizeof(instance_constants));EMIT(12,67);
uint32_t instance_cbv[]={0,1,1,111,1,12};memcpy(cmd,instance_cbv,sizeof(instance_cbv));EMIT(23,6);
uint32_t instance_cbp[]={1,2,1,112,0,16};memcpy(cmd,instance_cbp,sizeof(instance_cbp));EMIT(23,6);
cmd[0]=1;cmd[1]=2;cmd[2]=1;cmd[3]=115;EMIT(24,4);cmd[3]=108;EMIT(25,4);
cmd[0]=110;cmd[1]=57;cmd[2]=0;EMIT(22,3);
/* UV uses the original VB's (0.5,0.5) at byte16 unless testing two divisors. */
static const struct {unsigned layout,first,count,kind,stride,offset;} instance_cases[]={
 {306,2,3,2,4,0},{306,1,4,3,4,0},{306,3,3,2,0,0},{307,1,5,2,4,0},{308,2,4,3,4,0},
 {309,1,6,2,4,0},{306,2,3,4,4,0},{308,1,5,5,4,0},{307,0,6,2,4,24},{310,2,3,2,4,0}
};
for(unsigned test=0;test<10;test++){
 const unsigned layout=instance_cases[test].layout;
 memset(cmd,0,sizeof(cmd));cmd[0]=116;EMIT(5,7);memset(cmd,0,sizeof(cmd));cmd[0]=116;EMIT(26,37);
 cmd[0]=0;cmd[1]=305;EMIT(18,2);cmd[0]=1;cmd[1]=102;EMIT(18,2);cmd[0]=layout;EMIT(19,1);cmd[0]=1;EMIT(20,1);
 uint32_t bindings[]={0,4,109,0,16,302,instance_cases[test].stride,instance_cases[test].offset,301,12,0,303,8,0};memcpy(cmd,bindings,sizeof(bindings));EMIT(21,14);
 cmd[0]=104;EMIT(29,1);cmd[0]=0;cmd[1]=0;EMIT(28,2);memset(cmd,0,sizeof(cmd));for(unsigned i=1;i<5;i++)cmd[i]=0x3f800000;cmd[5]=UINT32_MAX;EMIT(27,6);
 memset(cmd,0,sizeof(cmd));cmd[0]=instance_cases[test].kind;cmd[1]=1;cmd[2]=instance_cases[test].count;cmd[5]=instance_cases[test].first;
 if(test==6||test==7){cmd[1]=304;cmd[2]=test==6?16:0;}EMIT(32,6);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;uint64_t out=dst+32000+test*1024;cmd[9]=(uint32_t)out;cmd[10]=out>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
}
EMIT(33,0);
