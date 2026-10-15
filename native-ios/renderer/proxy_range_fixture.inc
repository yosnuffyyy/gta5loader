/* Descriptor fixtures, not a capture of the missing iPhone descriptor. Both
   conversion and final binding use the production shared-ring DRAW path. */
static bool proxy_range_fixture(struct w2c_env *h,unsigned invalid,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+30000000000ull;while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}bool ready=native_gpu_ready(g),ok=ready;uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if((count)>128||tail>127488-8-(count)*4){ok=false;fail(g,"Proxy range fixture overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 cmd[0]=901;cmd[1]=1;cmd[2]=0x155;cmd[3]=UINT32_MAX;cmd[4]=0;EMIT(14,5);
 cmd[0]=902;cmd[2]=0x156;EMIT(14,5);
 uint32_t sampler[]={903,3,0x80,3,3,3,0,0,4,0,0,0,0,0,0x42000000};memcpy(cmd,sampler,sizeof(sampler));EMIT(16,15);cmd[0]=1;cmd[1]=2;cmd[2]=1;cmd[3]=903;EMIT(25,4);
 static const unsigned expected[]={0,255,0,255,0,0,255,255,0};
 for(unsigned test=0;test<(invalid?1u:9u);test++){
  if(test==0||test==6||test==7||test==8){if(test){cmd[0]=900;EMIT(4,1);}memset(cmd,0,sizeof(cmd));cmd[0]=900;cmd[1]=2;cmd[2]=cmd[3]=16;cmd[4]=1;cmd[5]=2;cmd[6]=test==7?6:3;cmd[7]=test==8?40:41;cmd[8]=0x28|(test==8?0x40:0);cmd[9]=1;EMIT(3,10);
   unsigned layers=test==7?6:3;for(unsigned m=0;m<2;m++)for(unsigned layer=0;layer<layers;layer++){
    if(test==8){memset(cmd,0,sizeof(cmd));cmd[0]=900;cmd[1]=m;cmd[2]=layer;cmd[3]=1;float z=.25f;memcpy(cmd+4,&z,4);EMIT(6,6);}
    else {memset(cmd,0,sizeof(cmd));cmd[0]=900;cmd[1]=m;cmd[2]=layer;float z=test==6||test==7||layer==1?.75f:.25f;memcpy(cmd+3,&z,4);EMIT(5,7);}
   }
  }
  if(test==4){memset(cmd,0,sizeof(cmd));cmd[0]=900;cmd[2]=1;float z=.25f;memcpy(cmd+3,&z,4);EMIT(5,7);}
  if(test){cmd[0]=904;EMIT(4,1);}uint32_t srv[]={904,0,900,41,5,test==3?1:0,UINT32_MAX,test==1||test==3||test==4||test==5||test==6?1:test==2?2:0,UINT32_MAX};
  if(test==7){srv[4]=9;srv[8]=6;}if(test==8)srv[3]=41;
  if(invalid==1)srv[6]=3; /* explicit excessive mip count */
  if(invalid==2)srv[7]=3; /* first layer == size */
  if(invalid==3)srv[8]=4; /* explicit excessive layer count */
  if(invalid==4){srv[4]=9;srv[8]=6;} /* 3 layers cannot be a cube */
  memcpy(cmd,srv,sizeof(srv));EMIT(17,9);cmd[0]=1;cmd[1]=2;cmd[2]=1;cmd[3]=904;EMIT(24,4);
  cmd[0]=1;cmd[1]=test==7||invalid==4?902:901;EMIT(18,2);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
  memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)(dst+test*1024);cmd[10]=(dst+test*1024)>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
 }
 cmd[0]=900;EMIT(4,1);cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=777;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ready&&ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=777&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);if(invalid)ok=ready&&native_gpu_error(g)&&strstr(native_gpu_error(g),"SRV range outside texture:")&&strstr(native_gpu_error(g),"SRV 904 raw[")&&g->draw.draws==0&&!g->draw.proxy_bytes;
 else {ok=ok&&!native_gpu_error(g)&&now_ns()<deadline&&g->draw.draws==9&&!g->draw.proxy_bytes&&!g->draw.proxy_passes;
  for(unsigned t=0;t<9&&ok;t++){uint8_t pixels[1024];host_load(h,pixels,dst+t*1024,sizeof(pixels));for(unsigned i=0;i<256;i++)if(pixels[i*4]!=expected[t]||pixels[i*4+1]||pixels[i*4+2]||pixels[i*4+3]!=255){ok=false;break;}}
 }
 if(report&&capacity)snprintf(report,capacity,"%s: SRV/proxy remaining ranges: 9 GPU pixel cases (2304 pixels), mip/array suffixes, source writes, source ID reuse, actual six-face cube, direct depth view; invalid boundary %u. %s",ok?"PASS":"FAIL",invalid,native_gpu_error(g)?native_gpu_error(g):"Generic descriptor fixture; captured 709/708 replay is tested separately.");if(!invalid&&report&&capacity){size_t used=strlen(report);TimingSummary maps=timing_summary(&g->metrics.windows[METRIC_READBACK]);if(used<capacity)snprintf(report+used,capacity-used,"\nFixture CPU submit/map %llu calls: mean/max %.3f/%.3f ms; post-completion sleeps avoided %llu; proxy payload peak %llu bytes. Offscreen Mac/fixture timings are not iPhone frame times or GPU execution time.",(unsigned long long)g->metrics.windows[METRIC_READBACK].count,maps.mean_ms,maps.worst_ms,(unsigned long long)g->metrics.callback_sleeps_avoided,(unsigned long long)g->draw.proxy_peak);}native_gpu_end(g);return ok;
}
