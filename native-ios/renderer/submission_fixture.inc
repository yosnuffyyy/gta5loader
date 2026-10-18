#ifdef NATIVE_PROBE_CLI
/* Original startup shaders through the production shared command ring.
   Legacy mode forces prior upload flushes; candidate checks actual buffer dependencies. */
static uint64_t submission_fixture_submissions[3];
static void submission_profile_output(void *unused,const char *line){(void)unused;puts(line);}
static bool submission_fixture(struct w2c_env *h,unsigned legacy,char *report,size_t capacity){
 if(legacy>=3)return false;
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000,address=base+126000;
 native_profile_set_enabled(legacy==2);NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);native_profile_set_enabled(false);if(!g)return false;if(legacy==2&&!g->profile){native_gpu_end(g);return false;}g->draw.fixture=true;g->test_legacy_upload_flush=legacy==1;g->test_drain_flush=legacy<2;g->test_drain_controlled_clock=true;g->test_measure_draw=true;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+60000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if(tail>127488-8-(count)*4){ok=false;fail(g,"Submission fixture overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 for(unsigned i=0;i<32;i++){memcpy(cmd,els,sizeof(els));cmd[0]=2000+i;cmd[1]=4;uint32_t extra[]={0xfeed,i,42,15,i*4,0,0};memcpy(cmd+23,extra,sizeof(extra));EMIT(15,30);}
 cmd[0]=3000;cmd[1]=208;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);cmd[0]=3000;cmd[1]=111;EMIT(35,2);
 cmd[0]=3001;cmd[1]=272;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);
 cmd[0]=105;EMIT(29,1);cmd[0]=11165;cmd[1]=1;cmd[2]=(uint32_t)address;cmd[3]=address>>32;EMIT(44,4);cmd[0]=11165;EMIT(45,1);
 cmd[0]=4000;cmd[1]=32;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);
 for(unsigned i=0;i<96;i++){
  if(i==64){cmd[0]=4000;EMIT(4,1);cmd[1]=32;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);}
  cmd[0]=4000;cmd[1]=0;cmd[2]=32;for(unsigned j=0;j<8;j++)cmd[j+3]=0x12340000+i*8+j;EMIT(i&1?47:12,11);
  if(i==32){cmd[0]=109;cmd[1]=0;cmd[2]=4;cmd[3]=0xbf800000;EMIT(12,4);}
  if(i==48){memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)(dst+1024);cmd[10]=(dst+1024)>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
   /* A reused handle owns a fresh constant buffer after a batch boundary. */
   cmd[0]=3000;EMIT(4,1);cmd[1]=208;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);cmd[0]=3000;cmd[1]=111;EMIT(35,2);
  }
  query_color[63]=((i/8)&1)?.5f:1;query_color[60]=((i/8)&2)?.5f:1;
  for(unsigned copy=0;copy<2;copy++){memset(cmd,0,sizeof(cmd));cmd[0]=copy?3001:112;cmd[1]=copy?16:0;cmd[2]=256;memcpy(cmd+3,query_color,sizeof(query_color));EMIT(i&1?47:12,67);}
  uint32_t cb0[]={0,1,1,i&1?3000:111,1,12};memcpy(cmd,cb0,sizeof(cb0));EMIT(23,6);
  uint32_t cb1[]={1,2,1,i>=80&&i<88?0:i&1?3001:112,i&1?1:0,16};memcpy(cmd,cb1,sizeof(cb1));EMIT(23,6);
  cmd[0]=2000+i%32;EMIT(19,1);cmd[0]=1;cmd[1]=i%16;cmd[2]=i/16;cmd[3]=cmd[1]+1;cmd[4]=cmd[2]+1;EMIT(31,5);
  memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
 }
 cmd[0]=11165;cmd[1]=910;EMIT(46,2);memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)dst;cmd[10]=dst>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
 memset(cmd,0,sizeof(cmd));cmd[0]=4000;cmd[9]=(uint32_t)(dst+2048);cmd[10]=(dst+2048)>>32;cmd[13]=1;cmd[14]=32;EMIT(42,17);
 /* Non-draw writes are dependencies too: a later queue upload must win over
    an encoded zero-clear or hidden-counter copy. Check exact returned bytes. */
 cmd[0]=5000;cmd[1]=32;cmd[2]=0x88;cmd[3]=0x40;cmd[4]=4;EMIT(11,5);
 uint32_t counter_view[]={5001,1,5000,0,1,0,8,0,2};memcpy(cmd,counter_view,sizeof(counter_view));EMIT(37,9);
 memset(cmd,0,sizeof(cmd));cmd[0]=5001;EMIT(40,6);
 cmd[0]=5000;cmd[1]=0;cmd[2]=8;cmd[3]=0xaa55aa55;cmd[4]=0x55aa55aa;EMIT(12,5);
 memset(cmd,0,sizeof(cmd));cmd[0]=5000;cmd[9]=(uint32_t)(dst+2080);cmd[10]=(dst+2080)>>32;cmd[13]=1;cmd[14]=32;EMIT(42,17);
 cmd[0]=2;cmd[1]=0;cmd[2]=1;cmd[3]=5001;cmd[4]=7;EMIT(38,5);
 cmd[0]=4000;cmd[1]=0;cmd[2]=5001;EMIT(43,3);
 cmd[0]=4000;cmd[1]=0;cmd[2]=4;cmd[3]=0xdeadbeef;EMIT(47,4);
 memset(cmd,0,sizeof(cmd));cmd[0]=4000;cmd[9]=(uint32_t)(dst+2112);cmd[10]=(dst+2112)>>32;cmd[13]=1;cmd[14]=32;EMIT(42,17);
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=911;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=911&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)==911&&atomic_load_explicit(heap_word(g,address),memory_order_acquire)==910&&host_u64(h,address+8)==96&&g->draw.draws==96;
 uint8_t pixels[2048];char mismatch[180]={0};host_load(h,pixels,dst,sizeof(pixels));
 for(unsigned read=0;read<2&&ok;read++)for(unsigned i=0;i<256&&ok;i++){bool painted=i<(read?48u:96u),zero=i>=80&&i<88;float color=((i/8)&1)?.5f:1,alpha=((i/8)&2)?.5f:1;int rgba[]={painted&&!zero?(int)lroundf(200*color):0,painted&&!zero?(int)lroundf(100*color):0,painted&&!zero?(int)lroundf(50*color):0,painted&&!zero?(int)lroundf(255*alpha):0};for(unsigned c=0;c<4;c++)if(abs((int)pixels[read*1024+i*4+c]-rgba[c])>1){snprintf(mismatch,sizeof(mismatch),"Pixel mismatch: read%u pixel%u channel%u got%u expected%d",read,i,c,pixels[read*1024+i*4+c],rgba[c]);ok=false;break;}}
 uint32_t buffer[8];host_load(h,buffer,dst+2048,sizeof(buffer));for(unsigned j=0;j<8;j++)if(buffer[j]!=0x12340000+95*8+j)ok=false;
 uint32_t hazard[16];host_load(h,hazard,dst+2080,sizeof(hazard));ok=ok&&hazard[0]==0xaa55aa55&&hazard[1]==0x55aa55aa&&hazard[8]==0xdeadbeef;for(unsigned i=2;i<8;i++)if(hazard[i])ok=false;
 ok=ok&&g->draw.creates==1&&g->draw.hits==95&&g->draw.pipeline_capacity==256&&!g->metrics.pipeline_evictions&&g->dependent_uploads>=3&&g->query_segments==96;
 if(legacy==1)ok=ok&&g->draw.batches>=96&&g->draw.uniform_packs==192;else ok=ok&&g->draw.batches<=12&&g->draw.uniform_packs<40&&g->independent_uploads>80;
 if(legacy==2)ok=ok&&g->drain_submits_avoided>0&&g->submissions<submission_fixture_submissions[0];submission_fixture_submissions[legacy]=g->submissions;
 char timing[440];timing_text(&g->test_draw_times,"DRAW handler CPU wall (CI only; not displayed frame time)",timing,sizeof(timing));
 if(report&&capacity)snprintf(report,capacity,"%s: buffer dependency replay legacy%u:96 original-shader draws,512 checked pixels,96 exact samples / 96 original pass-query segments,32 final buffer bytes, clear/counter upload ordering; batches/submissions %llu/%llu; uniform packs/bytes %llu/%llu; independent/dependent uploads %llu/%llu; completions %llu; pressure waits %llu; same resource replacement, no new staging. ready-drain submits avoided %llu; Handler/upload wall %.3f/%.3f ms; protocol upload payload %llu bytes (nested waits included, not GPU execution).\n%s\n%s",ok?"PASS":"FAIL",legacy,(unsigned long long)g->draw.batches,(unsigned long long)g->submissions,(unsigned long long)g->draw.uniform_packs,(unsigned long long)g->draw.uniform_upload_bytes,(unsigned long long)g->independent_uploads,(unsigned long long)g->dependent_uploads,(unsigned long long)g->metrics.windows[METRIC_COMPLETION].count,(unsigned long long)g->draw.uniform_pressure_waits,(unsigned long long)g->drain_submits_avoided,g->metrics.frame_busy/1e6,g->metrics.frame_upload/1e6,(unsigned long long)g->metrics.upload_bytes,timing,native_gpu_error(g)?native_gpu_error(g):mismatch[0]?mismatch:"Controlled policy clock for deterministic batching; real wall timings and GPU pixels. No device FPS or GPU execution claim.");
 if(legacy==2){uint64_t bytes=0;for(unsigned i=0;i<NP_OPS;i++)bytes+=g->profile->op[i].bytes;ok=ok&&g->profile->op[32].count==96&&g->profile->stage[NP_SUBMIT].count==g->submissions&&g->profile->stage[NP_PIPELINE].count==1&&g->profile->stage[NP_LINK].count==1&&!g->profile->clock_failures&&bytes==53044;native_profile_report(g->profile,submission_profile_output,NULL);if(!ok&&report&&capacity)snprintf(report,capacity,"FAIL: instrumented submission replay pixel/order/profile accounting");}
 native_gpu_end(g);return ok;
}
#endif
