cmd[0]=560;cmd[1]=2;cmd[2]=cmd[3]=8;cmd[4]=1;cmd[5]=4;cmd[6]=2;cmd[7]=28;cmd[8]=0x28;cmd[9]=1;EMIT(3,10);
uint32_t mip_view[]={561,0,560,28,5,0,3,1,1};memcpy(cmd,mip_view,sizeof(mip_view));EMIT(17,9);mip_view[0]=562;mip_view[6]=4;mip_view[7]=0;mip_view[8]=2;memcpy(cmd,mip_view,sizeof(mip_view));EMIT(17,9);
for(unsigned layer=0;layer<2;layer++){memset(cmd,0,sizeof(cmd));cmd[0]=560;cmd[2]=layer;cmd[6]=cmd[7]=8;cmd[8]=1;cmd[9]=32;cmd[10]=8;cmd[11]=256;uint8_t *data=(void*)(cmd+12);for(unsigned i=0;i<64;i++){data[i*4]=(uint8_t)(i*3);data[i*4+1]=(uint8_t)(layer*64);data[i*4+2]=128;data[i*4+3]=255;}EMIT(13,76);}
/* A non-array SRV selects one layer, even when the unused protocol count is
   zero. Mip generation must not overwrite the adjacent array slice. */
uint32_t mip_single[]={563,0,560,28,4,0,2,0,0};memcpy(cmd,mip_single,sizeof(mip_single));EMIT(17,9);cmd[0]=563;EMIT(41,1);
for(unsigned layer=0;layer<2;layer++){memset(cmd,0,sizeof(cmd));cmd[0]=560;cmd[1]=layer*4+1;uint64_t at=uav_dst+12000+layer*64;cmd[9]=(uint32_t)at;cmd[10]=at>>32;cmd[11]=16;cmd[12]=cmd[14]=64;EMIT(42,17);}
cmd[0]=561;EMIT(41,1);
memset(cmd,0,sizeof(cmd));cmd[0]=560;cmd[1]=5;cmd[9]=(uint32_t)(uav_dst+10240);cmd[10]=(uav_dst+10240)>>32;cmd[11]=16;cmd[12]=cmd[14]=64;EMIT(42,17);
cmd[0]=562;EMIT(41,1);cmd[0]=562;EMIT(41,1);
for(unsigned layer=0;layer<2;layer++){memset(cmd,0,sizeof(cmd));cmd[0]=560;cmd[1]=layer*4+3;uint64_t at=uav_dst+11264+layer*16;cmd[9]=(uint32_t)at;cmd[10]=at>>32;cmd[11]=4;cmd[12]=cmd[14]=4;EMIT(42,17);}
cmd[0]=560;EMIT(4,1);
