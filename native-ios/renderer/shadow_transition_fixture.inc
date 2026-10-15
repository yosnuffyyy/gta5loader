/* Repeated production-ring transitions. The shader contains the unchanged
   original count/cube/spot branch; geometry and real cube texels are known
   test inputs. A bound active/ambiguous 2D texture must still fail safely. */
static bool shadow_transition_fixture(struct w2c_env *h,unsigned invalid,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+45000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ready=native_gpu_ready(g),ok=ready;uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if((count)>128||tail>127488-8-(count)*4){ok=false;fail(g,"Shadow transition fixture ring overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 cmd[0]=901;cmd[1]=1;cmd[2]=0x223;cmd[3]=UINT32_MAX;cmd[4]=0;EMIT(14,5);
 uint32_t sampler[]={902,3,0x80,3,3,3,0,0,4,0,0,0,0,0,0x42000000};memcpy(cmd,sampler,sizeof(sampler));EMIT(16,15);
 cmd[0]=1;cmd[1]=14;cmd[2]=1;cmd[3]=902;EMIT(25,4);cmd[0]=1;cmd[1]=901;EMIT(18,2);
 uint32_t capture[]={708,2,4,4,1,1,1,71,8,1},captured_srv[]={709,0,708,71,4,0,1,0,0};
 memcpy(cmd,capture,sizeof(capture));EMIT(3,10);memset(cmd,0,sizeof(cmd));cmd[0]=708;cmd[6]=cmd[7]=4;cmd[8]=1;cmd[9]=8;cmd[10]=1;cmd[11]=8;EMIT(13,14);memcpy(cmd,captured_srv,sizeof(captured_srv));EMIT(17,9);
 uint32_t cube[]={710,2,4,4,1,1,6,41,0x28,1};memcpy(cmd,cube,sizeof(cube));EMIT(3,10);
 for(unsigned face=0;face<6;face++){memset(cmd,0,sizeof(cmd));cmd[0]=710;cmd[2]=face;float z=.25f;memcpy(cmd+3,&z,4);EMIT(5,7);}
 uint32_t cube_srv[]={711,0,710,41,9,0,1,0,6};memcpy(cmd,cube_srv,sizeof(cube_srv));EMIT(17,9);
 uint32_t spot[]={712,2,4,4,1,1,1,41,0x28,1};memcpy(cmd,spot,sizeof(spot));EMIT(3,10);
 memset(cmd,0,sizeof(cmd));cmd[0]=712;float z=.75f;memcpy(cmd+3,&z,4);EMIT(5,7);
 uint32_t spot_srv[]={713,0,712,41,4,0,1,0,0};memcpy(cmd,spot_srv,sizeof(spot_srv));EMIT(17,9);
 cmd[0]=1;cmd[1]=24;cmd[2]=1;cmd[3]=713;EMIT(24,4);
 unsigned expected[32]={0};
 for(unsigned test=0;test<32;test++){
  unsigned mode=test%8,first=(test/8)%2?2:0;
  if(test%8==0){
   if(test){cmd[0]=714;EMIT(4,1);cmd[0]=715;EMIT(4,1);}
   cmd[0]=714;cmd[1]=528;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);
   cmd[0]=715;cmd[1]=80;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);
   float values[124]={0};values[60]=values[65]=values[70]=1;values[71]=.5f;values[74]=-1;
   memset(cmd,0,sizeof(cmd));cmd[0]=714;cmd[1]=first*16;cmd[2]=496;memcpy(cmd+3,values,sizeof(values));EMIT(12,127);
   uint32_t cb[]={1,6,1,714,first,31};memcpy(cmd,cb,sizeof(cb));EMIT(23,6);cb[1]=3;cb[3]=715;cb[5]=3;memcpy(cmd,cb,sizeof(cb));EMIT(23,6);
   if(test==16){cmd[0]=709;EMIT(4,1);cmd[0]=708;EMIT(4,1);memcpy(cmd,capture,sizeof(capture));EMIT(3,10);memset(cmd,0,sizeof(cmd));cmd[0]=708;cmd[6]=cmd[7]=4;cmd[8]=1;cmd[9]=8;cmd[10]=1;cmd[11]=8;EMIT(13,14);memcpy(cmd,captured_srv,sizeof(captured_srv));EMIT(17,9);}
   if(test==16){cmd[0]=711;EMIT(4,1);cmd[0]=710;EMIT(4,1);memcpy(cmd,cube,sizeof(cube));EMIT(3,10);for(unsigned face=0;face<6;face++){memset(cmd,0,sizeof(cmd));cmd[0]=710;cmd[2]=face;float replacement=.75f;memcpy(cmd+3,&replacement,4);EMIT(5,7);}memcpy(cmd,cube_srv,sizeof(cube_srv));EMIT(17,9);}
  }
  uint32_t type=mode==0?0:mode==3?0x40400000u:mode==6?0x80000000u:0x40000000u;
  uint32_t count=mode==2||mode==7?0:1,selected=mode==1||mode==4||mode==5?711:709;
  /* Split writes to the selector exercise byte uploads and re-evaluation;
     an old inactive cache hit must never hide an active final value. */
  cmd[0]=714;cmd[1]=first*16+252;cmd[2]=2;cmd[3]=type&0xffff;EMIT(12,4);
  cmd[1]+=2;cmd[3]=type>>16;EMIT(47,4);
  cmd[0]=715;cmd[1]=first*16+32;cmd[2]=4;cmd[3]=count;EMIT(12,4);
  cmd[0]=1;cmd[1]=14;cmd[2]=1;cmd[3]=selected;EMIT(24,4);
  memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
  expected[test]=selected==711&&test<16?0:255;
  memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)(dst+test*1024);cmd[10]=(dst+test*1024)>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
 }
 memset(cmd,0,sizeof(cmd));cmd[0]=708;cmd[9]=(uint32_t)(dst+32768);cmd[10]=(dst+32768)>>32;cmd[11]=cmd[12]=cmd[14]=8;EMIT(42,17);
 if(invalid){
  cmd[0]=714;cmd[1]=32+252;cmd[2]=4;cmd[3]=invalid==1?0x40000000:0x7fc00000;EMIT(12,4);
  cmd[0]=715;cmd[1]=32+32;cmd[2]=4;cmd[3]=1;EMIT(12,4);
  cmd[0]=1;cmd[1]=14;cmd[2]=1;cmd[3]=709;EMIT(24,4);
  memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
 }
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=888;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ready&&ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=888&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);
 bool state=invalid?(native_gpu_error(g)&&strstr(native_gpu_error(g),"SRV range outside texture")&&strstr(native_gpu_error(g),invalid==1?"raw40000000":"raw7fc00000")):!native_gpu_error(g);
 ok=ok&&state&&g->draw.draws==32&&g->metrics.shadow_creates==1&&g->metrics.shadow_hits>=19&&g->draw.creates==2&&g->draw.hits==30&&g->metrics.pipeline_evictions==0;
 for(unsigned t=0;t<32&&ok;t++){uint8_t pixels[1024];host_load(h,pixels,dst+t*1024,sizeof(pixels));for(unsigned i=0;i<256&&ok;i++)for(unsigned c=0;c<4;c++)if(pixels[i*4+c]!=(c==3?255:expected[t]))ok=false;}
 {uint32_t original[2];host_load(h,original,dst+32768,8);ok=ok&&!original[0]&&!original[1];}
 if(report&&capacity)snprintf(report,capacity,"%s: dynamic shadow transitions: 32 draws/8192 pixels; selector2/count1 genuine cube, count0/selector0/selector3 inactive2D, CB byte writes/offsets/ID reuse, source708/709 replacement; 2 pipelines/30 hits/0 evictions, one inactive variant; final %s. Original2D bytes/layers retained.",ok?"PASS":"FAIL",invalid==1?"active2D rejected":invalid==2?"ambiguous2D rejected":"valid transitions");
 if(!ok&&report&&capacity){size_t used=strlen(report);if(used<capacity)snprintf(report+used,capacity-used," Actual draws/pipelines/hits/variants/variantHits %llu/%llu/%llu/%llu/%llu. %s",(unsigned long long)g->draw.draws,(unsigned long long)g->draw.creates,(unsigned long long)g->draw.hits,(unsigned long long)g->metrics.shadow_creates,(unsigned long long)g->metrics.shadow_hits,native_gpu_error(g)?native_gpu_error(g):"Pixel or completion mismatch");}
 native_gpu_end(g);return ok;
}
