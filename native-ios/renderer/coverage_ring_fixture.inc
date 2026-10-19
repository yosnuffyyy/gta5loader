/* Alpha coverage's color/depth/occlusion effects through actual protocol
   commands. Original startup shaders provide controlled RGBA output. */
static bool coverage_ring_fixture(struct w2c_env *h,bool v2){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;native_gpu_execution_option(g,v2);
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+60000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if(tail>127488-8-(count)*4){ok=false;fail(g,"Coverage ring overflow");}else{host_put32(h,base+tail,op);host_put32(h,base+tail+4,count);host_store(h,base+tail+8,cmd,count*4);tail+=8+count*4;}}while(0)
 #include "query_setup.inc"
 memset(cmd,0,sizeof(cmd));cmd[0]=123;cmd[1]=0;cmd[2]=1;cmd[11]=15;EMIT(16,68);
 const uint32_t values[]={0,0x3e800000,0x3f000000,0x3f400000,0x3f800000,0xbf800000,0x40000000,0x7fc00000,0x3effffff};
 for(unsigned t=0;t<9;t++){
  for(unsigned q=0;q<2;q++){uint64_t address=dst+8192+t*64+q*32;host_put32(h,address,0);cmd[0]=14000+t*2+q;cmd[1]=1;cmd[2]=(uint32_t)address;cmd[3]=address>>32;EMIT(44,4);}
  memset(cmd,0,sizeof(cmd));cmd[0]=116;EMIT(5,7);memset(cmd,0,sizeof(cmd));cmd[0]=117;cmd[3]=1;cmd[4]=0x3f800000;EMIT(6,6);
  memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[32]=117;EMIT(26,37);cmd[0]=107;cmd[1]=0;EMIT(28,2);
  memset(cmd,0,sizeof(cmd));cmd[0]=123;for(unsigned j=1;j<5;j++)cmd[j]=0x3f800000;cmd[5]=UINT32_MAX;EMIT(27,6);
  cmd[0]=112;cmd[1]=240;cmd[2]=4;cmd[3]=values[t];EMIT(12,4);
  cmd[0]=14000+t*2;EMIT(45,1);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);cmd[0]=14000+t*2;cmd[1]=101+t*2;EMIT(46,2);
  memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)(dst+t*1024);cmd[10]=(dst+t*1024)>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
  /* Later geometry at z=.5 must pass only where A2C rejected the z=0 draw. */
  cmd[0]=111;cmd[1]=16+11*16+8;cmd[2]=4;cmd[3]=0x3f000000;EMIT(12,4);cmd[0]=112;cmd[1]=240;cmd[2]=4;cmd[3]=0x3f800000;EMIT(12,4);
  cmd[0]=14001+t*2;EMIT(45,1);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);cmd[0]=14001+t*2;cmd[1]=102+t*2;EMIT(46,2);
  cmd[0]=111;cmd[1]=16+11*16+8;cmd[2]=4;cmd[3]=0;EMIT(12,4);
 }
 /* Discard must suppress stencil writes as well as depth/color. */
 memset(cmd,0,sizeof(cmd));cmd[0]=122;cmd[1]=2;cmd[2]=cmd[3]=16;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=45;cmd[8]=0x40;cmd[9]=1;EMIT(3,10);
 uint32_t sw[]={124,1,0,0,8,1,0xffff,1,1,3,8,1,1,3,8};memcpy(cmd,sw,sizeof(sw));EMIT(16,15);sw[0]=125;sw[9]=sw[13]=1;sw[10]=sw[14]=3;memcpy(cmd,sw,sizeof(sw));EMIT(16,15);
 for(unsigned t=0;t<2;t++){
  uint64_t address=dst+12288+t*32;host_put32(h,address,0);cmd[0]=14100+t;cmd[1]=1;cmd[2]=(uint32_t)address;cmd[3]=address>>32;EMIT(44,4);
  memset(cmd,0,sizeof(cmd));cmd[0]=122;cmd[3]=3;cmd[4]=0x3f800000;EMIT(6,6);memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[32]=122;EMIT(26,37);
  cmd[0]=124;cmd[1]=7;EMIT(28,2);memset(cmd,0,sizeof(cmd));cmd[0]=123;cmd[5]=UINT32_MAX;EMIT(27,6);cmd[0]=112;cmd[1]=240;cmd[2]=4;cmd[3]=t?0x3f800000:0;EMIT(12,4);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
  cmd[0]=125;cmd[1]=7;EMIT(28,2);memset(cmd,0,sizeof(cmd));cmd[5]=UINT32_MAX;EMIT(27,6);cmd[0]=14100+t;EMIT(45,1);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);cmd[0]=14100+t;cmd[1]=201+t;EMIT(46,2);
 }
 /* Native MSAA still uses hardware sample coverage, not a binary discard. */
 memset(cmd,0,sizeof(cmd));cmd[0]=126;cmd[1]=2;cmd[2]=cmd[3]=16;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=28;cmd[8]=0x20;cmd[9]=4;EMIT(3,10);
 for(unsigned t=0;t<5;t++){
  uint64_t address=dst+12544+t*32;host_put32(h,address,0);cmd[0]=14200+t;cmd[1]=1;cmd[2]=(uint32_t)address;cmd[3]=address>>32;EMIT(44,4);
  memset(cmd,0,sizeof(cmd));cmd[0]=126;EMIT(26,37);cmd[0]=cmd[1]=0;EMIT(28,2);memset(cmd,0,sizeof(cmd));cmd[0]=123;cmd[5]=UINT32_MAX;EMIT(27,6);float a=t*.25f;cmd[0]=112;cmd[1]=240;cmd[2]=4;memcpy(cmd+3,&a,4);EMIT(12,4);cmd[0]=14200+t;EMIT(45,1);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);cmd[0]=14200+t;cmd[1]=301+t;EMIT(46,2);
 }
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=991;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=991&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&host_u64(h,flag)%UINT64_C(4294967296)==991;
 for(unsigned t=0;t<9&&ok;t++){float a;memcpy(&a,values+t,4);bool covered=a>=.5f;for(unsigned q=0;q<2;q++){uint64_t address=dst+8192+t*64+q*32;uint64_t want=(q?!covered:covered)?256:0;if(host_u64(h,address+8)!=want||atomic_load(heap_word(g,address))!=101+t*2+q){fail(g,"A2C query/depth result mismatch");ok=false;}}
  uint8_t pixels[1024];host_load(h,pixels,dst+t*1024,1024);for(unsigned i=0;i<256&&ok;i++){int want[4]={covered?200:0,covered?100:0,covered?50:0,covered?(int)lroundf(fminf(1,a)*255):0};for(unsigned c=0;c<4;c++)if(abs((int)pixels[i*4+c]-want[c])>1){fail(g,"A2C color or alpha mismatch");ok=false;break;}}
 }
 for(unsigned t=0;t<2&&ok;t++){uint64_t at=dst+12288+t*32;if(host_u64(h,at+8)!=(t?256u:0u)||atomic_load(heap_word(g,at))!=201+t){fail(g,"A2C stencil write/read mismatch");ok=false;}}
 uint64_t previous=0;for(unsigned t=0;t<5&&ok;t++){uint64_t at=dst+12544+t*32,n=host_u64(h,at+8);if(atomic_load(heap_word(g,at))!=301+t||n>1024||n<previous||(!t&&n)||(t==4&&n!=1024)){fail(g,"Native multisample A2C coverage mismatch");ok=false;}previous=n;}
 printf("%s: A2C protocol %s: 9 alphas incl NaN/negative/above1/threshold edges, 2304 RGBA pixels, 18 exact depth/occlusion, 2 stencil and 5 native MSAA query results; no skipped draws. %s\n",ok?"PASS":"FAIL",v2?"V2":"existing",native_gpu_error(g)?native_gpu_error(g):"");native_gpu_end(g);return ok;
}
