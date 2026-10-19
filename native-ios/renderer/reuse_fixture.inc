#ifdef NATIVE_PROBE_CLI
/* Original startup shaders through the production shared command ring.
   Modes1/2 isolate the previous uniform/layout behavior independently. */
static bool reuse_fixture(struct w2c_env *h,unsigned mode,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000,address=base+126000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;if(mode>=5)native_gpu_core_option(g,mode==6);g->test_no_pass_reuse=mode==3;if(mode>=3&&!g->profile){g->profile=calloc(1,sizeof(*g->profile));if(!g->profile){native_gpu_end(g);return false;}}g->test_drain_flush=true;g->test_no_uniform_reuse=mode==1;g->test_unused_layout_keys=mode==2;g->test_measure_draw=true;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+60000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if(tail>127488-8-(count)*4){ok=false;fail(g,"Reuse fixture overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 for(unsigned i=0;i<32;i++){memcpy(cmd,els,sizeof(els));cmd[0]=2000+i;cmd[1]=4;uint32_t extra[]={0xfeed,i,42,15,i*4,0,0};memcpy(cmd+23,extra,sizeof(extra));EMIT(15,30);}
 cmd[0]=3000;cmd[1]=208;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);cmd[0]=3000;cmd[1]=111;EMIT(35,2);
 cmd[0]=3001;cmd[1]=272;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);
 cmd[0]=105;EMIT(29,1);cmd[0]=11165;cmd[1]=1;cmd[2]=(uint32_t)address;cmd[3]=address>>32;EMIT(44,4);cmd[0]=11165;EMIT(45,1);
 for(unsigned i=0;i<96;i++){
  if(i==48){memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)(dst+1024);cmd[10]=(dst+1024)>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
   /* A reused handle owns a fresh constant buffer after a batch boundary. */
   cmd[0]=3000;EMIT(4,1);cmd[1]=208;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);cmd[0]=3000;cmd[1]=111;EMIT(35,2);
  }
  query_color[63]=((i/8)&1)?.5f:1;query_color[60]=((i/8)&2)?.5f:1;
  for(unsigned copy=0;copy<2;copy++){memset(cmd,0,sizeof(cmd));cmd[0]=copy?3001:112;cmd[1]=copy?16:0;cmd[2]=256;memcpy(cmd+3,query_color,sizeof(query_color));EMIT(i&1?47:12,67);}
  uint32_t cb0[]={0,1,1,i&1?3000:111,1,12};memcpy(cmd,cb0,sizeof(cb0));EMIT(23,6);
  uint32_t cb1[]={1,2,1,i>=80&&i<88?0:i&1?3001:112,i&1?1:0,16};memcpy(cmd,cb1,sizeof(cb1));EMIT(23,6);
  cmd[0]=2000+(mode>=5?(i/8)%32:i%32);EMIT(19,1);cmd[0]=1;cmd[1]=i%16;cmd[2]=i/16;cmd[3]=cmd[1]+1;cmd[4]=cmd[2]+1;EMIT(31,5);
  memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
 }
 cmd[0]=11165;cmd[1]=910;EMIT(46,2);memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)dst;cmd[10]=dst>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=911;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=911&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)==911&&atomic_load_explicit(heap_word(g,address),memory_order_acquire)==910&&host_u64(h,address+8)==96&&g->draw.draws==96;
 uint8_t pixels[2048];char mismatch[180]={0};host_load(h,pixels,dst,sizeof(pixels));
 for(unsigned read=0;read<2&&ok;read++)for(unsigned i=0;i<256&&ok;i++){bool painted=i<(read?48u:96u),zero=i>=80&&i<88;float color=((i/8)&1)?.5f:1,alpha=((i/8)&2)?.5f:1;int rgba[]={painted&&!zero?(int)lroundf(200*color):0,painted&&!zero?(int)lroundf(100*color):0,painted&&!zero?(int)lroundf(50*color):0,painted&&!zero?(int)lroundf(255*alpha):0};for(unsigned c=0;c<4;c++)if(abs((int)pixels[read*1024+i*4+c]-rgba[c])>1){snprintf(mismatch,sizeof(mismatch),"Pixel mismatch: read%u pixel%u channel%u got%u expected%d",read,i,c,pixels[read*1024+i*4+c],rgba[c]);ok=false;break;}}
 /*753 commands. Production's256-command drain flushes after draws26/62;
    the midway readback splits at48. Four batches require4 VS+14 PS packs.
    Freeze the prior drain policy here to isolate uniform/layout reuse; the
    separate dependency replay compares the new bounded-drain policy. */
 uint64_t packs=mode==1?192:18;ok=ok&&g->draw.uniform_packs==packs&&g->draw.uniform_reuses==192-packs&&g->draw.uniform_upload_bytes==packs*g->draw.alignment&&g->draw.creates==(mode==2?32u:1u)&&g->draw.hits==(mode==2?64u:95u)&&g->draw.batches==4&&g->draw.pipeline_capacity==256&&!g->metrics.pipeline_evictions;
 printf("Pass-state replay mode%u: emitted/skipped %llu/%llu; cache bytes %zu; draws %llu; submissions %llu; full pixel/query checks %s.\n",mode,(unsigned long long)g->draw.state_emitted,(unsigned long long)g->draw.state_skipped,sizeof(PassState),(unsigned long long)g->draw.draws,(unsigned long long)g->submissions,ok?"PASS":"FAIL");
 if(mode==3||mode==4){TimingSummary t=timing_summary(&g->test_draw_times);printf("PASS_STATE_BENCH mode=%u draws=%llu draw_wall_ms=%.6f draw_cpu_ms=%.6f service_cpu_ms=%.6f cpu_errors=%llu p50=%.6f p95=%.6f p99=%.6f max=%.6f emitted=%llu skipped=%llu submissions=%llu bytes=%llu\n",mode,(unsigned long long)g->draw.draws,g->test_draw_times.total_ns/1e6,g->profile->op[32].cpu/1e6,g->service_cpu_ns/1e6,(unsigned long long)(g->service_cpu_errors+g->profile->clock_failures),t.median_ms,t.p95_ms,t.p99_ms,t.worst_ms,(unsigned long long)g->draw.state_emitted,(unsigned long long)g->draw.state_skipped,(unsigned long long)g->submissions,(unsigned long long)g->draw.uniform_upload_bytes);}
 if(mode>=5){TimingSummary t=timing_summary(&g->test_draw_times);printf("CPU_CORE_BENCH mode=%u draws=%llu draw_wall_ms=%.6f draw_cpu_ms=%.6f service_cpu_ms=%.6f cpu_errors=%llu p50=%.6f p95=%.6f p99=%.6f max=%.6f preparation_hits=%llu preparation_misses=%llu indexed=%llu fallback=%llu index_bytes=%zu submissions=%llu bytes=%llu\n",mode,(unsigned long long)g->draw.draws,g->test_draw_times.total_ns/1e6,g->profile->op[32].cpu/1e6,g->service_cpu_ns/1e6,(unsigned long long)(g->service_cpu_errors+g->profile->clock_failures),t.median_ms,t.p95_ms,t.p99_ms,t.worst_ms,(unsigned long long)g->core_pipeline.hits,(unsigned long long)g->core_pipeline.misses,(unsigned long long)g->core_index_hits,(unsigned long long)g->core_index_misses,g->core_index_peak_bytes,(unsigned long long)g->submissions,(unsigned long long)g->draw.uniform_upload_bytes);if(mode==6)ok=ok&&g->core_pipeline.hits>=80&&g->core_index_hits>0;}
 char timing[440];timing_text(&g->test_draw_times,"DRAW handler CPU wall (CI only; not displayed frame time)",timing,sizeof(timing));
 if(report&&capacity)snprintf(report,capacity,"%s: immutable uniform/layout reuse mode%u:96 original-shader draws,512 checked pixels,96 exact samples; packs/reuses %llu/%llu, uniform GPU bytes %llu, batches %llu; pipelines creates/hits/evictions %llu/%llu/%llu; submissions %llu, completions %llu, pressure waits %llu, logical resource peak %llu bytes; no new staging. Changing constants, null/rebound CBs, first-constant offsets, flush and resource replacement verified.\n%s\n%s",ok?"PASS":"FAIL",mode,(unsigned long long)g->draw.uniform_packs,(unsigned long long)g->draw.uniform_reuses,(unsigned long long)g->draw.uniform_upload_bytes,(unsigned long long)g->draw.batches,(unsigned long long)g->draw.creates,(unsigned long long)g->draw.hits,(unsigned long long)g->metrics.pipeline_evictions,(unsigned long long)g->submissions,(unsigned long long)g->metrics.windows[METRIC_COMPLETION].count,(unsigned long long)g->draw.uniform_pressure_waits,(unsigned long long)g->resource_peak,timing,native_gpu_error(g)?native_gpu_error(g):mismatch[0]?mismatch:"No device FPS or GPU execution claim.");
 native_gpu_end(g);return ok;
}
#endif
