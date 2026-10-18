#ifdef NATIVE_PROBE_CLI
static bool copy_cast_fixture(struct w2c_env *h){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+60000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if(tail>127488-8-(count)*4){ok=false;fail(g,"Copy-cast replay overflow");}else{host_put32(h,base+tail,op);host_put32(h,base+tail+4,count);host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 const unsigned formats[]={71,72,74,75,77,78,80,81,83,84,95,96,98,99,42,29};
 unsigned bytes[16];
 for(unsigned t=0;t<16;t++){
  bool bc=t<14;unsigned block=(t<2||t==6||t==7)?8:bc?16:4;bytes[t]=block;
  unsigned raw=bc?(block==8?17:3):(t==14?41:28);
  /* Retained fatal opcode and IDs; descriptors are reference-semantic fixtures,
     because the phone report did not capture the two original descriptors. */
  for(unsigned side=0;side<2;side++){memset(cmd,0,sizeof(cmd));cmd[0]=side?35378:10933;cmd[1]=2;cmd[2]=cmd[3]=side&&bc?4:1;cmd[4]=cmd[5]=1;cmd[6]=2;cmd[7]=side?formats[t]:raw;cmd[8]=8;cmd[9]=1;EMIT(3,10);}
  for(unsigned iteration=0;iteration<2;iteration++){
   memset(cmd,0,sizeof(cmd));cmd[0]=10933;cmd[2]=iteration;cmd[6]=cmd[7]=cmd[8]=1;cmd[9]=cmd[11]=block;cmd[10]=1;for(unsigned i=0;i<block;i++)((uint8_t*)(cmd+12))[i]=(uint8_t)(t*19+iteration*37+i*7);EMIT(13,12+block/4);
   memset(cmd,0,sizeof(cmd));cmd[0]=35378;cmd[1]=iteration;cmd[5]=10933;cmd[6]=iteration;EMIT(34,14);
   memset(cmd,0,sizeof(cmd));cmd[0]=35378;cmd[1]=iteration;cmd[9]=(uint32_t)(dst+t*64+iteration*32);cmd[10]=(dst+t*64+iteration*32)>>32;cmd[11]=cmd[12]=cmd[14]=block;EMIT(42,17);
   memset(cmd,0,sizeof(cmd));cmd[0]=10933;cmd[1]=iteration;cmd[5]=35378;cmd[6]=iteration;EMIT(34,14);
  }
  cmd[0]=35378;EMIT(4,1);memset(cmd,0,sizeof(cmd));cmd[0]=10933;cmd[1]=1;cmd[9]=(uint32_t)(dst+2048+t*32);cmd[10]=(dst+2048+t*32)>>32;cmd[11]=cmd[12]=cmd[14]=block;EMIT(42,17);cmd[0]=10933;EMIT(4,1);
 }
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=919;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=919&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)==919;
 for(unsigned t=0;t<16&&ok;t++)for(unsigned sample=0;sample<3&&ok;sample++){uint8_t b[16];host_load(h,b,sample<2?dst+t*64+sample*32:dst+2048+t*32,bytes[t]);for(unsigned j=0;j<bytes[t];j++)if(b[j]!=(uint8_t)(t*19+(sample?37:0)+j*7))ok=false;}
 ok=ok&&g->copy_reinterpretations==64&&!g->buffer_bytes&&!g->texture_bytes;
 printf("%s: bit-preserving COPY_REGION: 16 format pairs, 64 copies, 48 byte-checked readbacks, array layers, repeated resource IDs and source destruction; GPU-only scratch at most 1 MiB. %s\n",ok?"PASS":"FAIL",native_gpu_error(g)?native_gpu_error(g):"");native_gpu_end(g);return ok;
}
#endif
