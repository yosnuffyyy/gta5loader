/* Production command replay, deterministic pixels and exact occlusion counts.
   CLI comparisons isolate policy changes; their times are not iPhone FPS. */
static bool uniform_order_fixture(struct w2c_env *h,bool old,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000,address=base+126000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;
#ifdef NATIVE_PROBE_CLI
 g->test_uniform_wrap_wait=old;
#else
 if(old){native_gpu_end(g);return false;}
#endif
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+60000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if(tail>127488-8-(count)*4){ok=false;fail(g,"Uniform ordering fixture overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 cmd[0]=118;cmd[1]=4;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);
 cmd[0]=105;EMIT(29,1);cmd[0]=11165;cmd[1]=1;cmd[2]=(uint32_t)address;cmd[3]=address>>32;EMIT(44,4);cmd[0]=11165;EMIT(45,1);
 for(unsigned i=0;i<96;i++){
  query_color[63]=(i&1)?.5f:1;query_color[60]=(i&2)?.5f:1;
  memset(cmd,0,sizeof(cmd));cmd[0]=112;cmd[2]=256;memcpy(cmd+3,query_color,sizeof(query_color));EMIT(12,67);
  cmd[0]=1;cmd[1]=i%16;cmd[2]=i/16;cmd[3]=cmd[1]+1;cmd[4]=cmd[2]+1;EMIT(31,5);
  memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
  /* Existing GPU resource mutation ends each batch, without a CPU readback. */
  cmd[0]=118;cmd[1]=0;cmd[2]=4;cmd[3]=i;EMIT(12,4);
 }
 cmd[0]=11165;cmd[1]=901;EMIT(46,2);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)dst;cmd[10]=dst>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=902;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=902&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&host_u64(h,address+8)==96&&atomic_load_explicit(heap_word(g,address),memory_order_acquire)==901&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)==902&&g->draw.batches==96&&g->draw.draws==96;
 uint8_t pixels[1024];host_load(h,pixels,dst,sizeof(pixels));for(unsigned i=0;i<256&&ok;i++){float scale=(i&1)?.5f:1,alpha=(i&2)?.5f:1;int expected[]={i<96?(int)lroundf(200*scale):0,i<96?(int)lroundf(100*scale):0,i<96?(int)lroundf(50*scale):0,i<96?(int)lroundf(255*alpha):0};for(unsigned c=0;c<4;c++)if(abs((int)pixels[i*4+c]-expected[c])>1)ok=false;}
 ok=ok&&g->draw.uniform_pressure_waits==(old?0u:6u)&&g->metrics.windows[METRIC_COMPLETION].count==(old?33u:7u);
 if(report&&capacity)snprintf(report,capacity,"%s: uniform queue ordering %s: 96 changing-constant batches, 32 slot wraps, all 256 pixels and exact 96 samples; completion waits %llu, CPU wait %.3fms, pressure waits %llu; no stale uniform data. %s",ok?"PASS":"FAIL",old?"0.23 reference":"bounded queue reuse",(unsigned long long)g->metrics.windows[METRIC_COMPLETION].count,g->metrics.windows[METRIC_COMPLETION].total_ns/1e6,(unsigned long long)g->draw.uniform_pressure_waits,native_gpu_error(g)?native_gpu_error(g):"");
 native_gpu_end(g);return ok;
}
#ifdef NATIVE_PROBE_CLI
static bool pipeline_working_set_fixture(struct w2c_env *h,unsigned mode,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000,address=base+126000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;g->test_pipeline_capacity=mode;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+90000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if(tail>127488-8-(count)*4){ok=false;fail(g,"Pipeline working set fixture overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 depth_words[0]=130;depth_words[3]=0;depth_words[4]=8;memcpy(cmd,depth_words,sizeof(depth_words));EMIT(16,15);cmd[0]=130;cmd[1]=0;EMIT(28,2);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[32]=117;EMIT(26,37);
 for(unsigned i=0;i<192;i++){raster[0]=1000+i;raster[5]=i;raster[9]=0;memcpy(cmd,raster,sizeof(raster));EMIT(16,12);}
 cmd[0]=11165;cmd[1]=1;cmd[2]=(uint32_t)address;cmd[3]=address>>32;EMIT(44,4);cmd[0]=11165;EMIT(45,1);
 for(unsigned i=0;i<576;i++){cmd[0]=1000+i%192;EMIT(29,1);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);}
 cmd[0]=11165;cmd[1]=903;EMIT(46,2);memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)dst;cmd[10]=dst>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=904;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=904&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);unsigned creates=mode==128?576:mode==256?192:448,evicts=mode==128?448:mode==256?0:256;
 ok=ok&&!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)==904&&atomic_load_explicit(heap_word(g,address),memory_order_acquire)==903&&host_u64(h,address+8)==147456&&g->draw.draws==576&&g->draw.creates==creates&&g->draw.hits==576-creates&&g->metrics.pipeline_evictions==evicts&&g->draw.pipeline_pressure_evictions==(mode==1?64u:0u);
 uint8_t pixels[1024];host_load(h,pixels,dst,sizeof(pixels));int expected[]={200,100,50,255};for(unsigned i=0;i<256&&ok;i++)for(unsigned c=0;c<4;c++)if(abs((int)pixels[i*4+c]-expected[c])>1)ok=false;
 if(report&&capacity)snprintf(report,capacity,"%s: pipeline working set policy %u: 192 distinct required keys, 3 cycles, 576 draws/147456 samples/256 pixels; creates/hits/evictions %llu/%llu/%llu; live peak %u, pressure evictions %llu; pipeline API %.3fms, link %.3fms; encoded commands survive eviction. Policy 1 shrinks 256->128 then grows. Driver bytes and iPhone FPS unmeasured. %s",ok?"PASS":"FAIL",mode,(unsigned long long)g->draw.creates,(unsigned long long)g->draw.hits,(unsigned long long)g->metrics.pipeline_evictions,g->draw.pipeline_peak,(unsigned long long)g->draw.pipeline_pressure_evictions,g->metrics.windows[METRIC_PIPELINE].total_ns/1e6,g->metrics.windows[METRIC_LINK].total_ns/1e6,native_gpu_error(g)?native_gpu_error(g):"");
 native_gpu_end(g);return ok;
}
#endif
