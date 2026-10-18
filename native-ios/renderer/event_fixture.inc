#ifdef NATIVE_PROBE_CLI
/* Callback fault injection is separate from the real Metal completion replay. */
static bool event_callback_fixture(struct w2c_env *h){
 bool ok=true;uint64_t address=atomic_load(&h->memory.size)-1024;
 for(unsigned mode=0;mode<7&&ok;mode++){
  NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;
  Object *q=create(g,700,44);if(!q){native_gpu_end(g);return false;}
  q->words=4;q->meta=calloc(4,sizeof(uint32_t));if(!q->meta){native_gpu_end(g);return false;}q->meta[2]=(uint32_t)address;q->meta[3]=address>>32;q->event_token=1;
  host_put32(h,address,0);host_put32(h,address+8,0);host_put32(h,address+12,0);
  EventReply reply={.owner=g,.id=700,.generation=q->generation,.token=1,.sequence=91,.value=UINT64_C(0x123456789abcdef0),.busy=true};g->event_pending=1;
  if(mode==1)q->event_token++;if(mode==2)q->generation++;if(mode==3)atomic_store(&g->stop,true);if(mode==4)atomic_store(&g->closing,true);if(mode==6)q->meta[2]++;
  WGPUStringView message=WGPU_STRING_VIEW_INIT;event_done(mode==5?WGPUQueueWorkDoneStatus_Error:WGPUQueueWorkDoneStatus_Success,message,&reply,NULL);
  ok=!reply.busy&&g->event_pending==0&&atomic_load(heap_word(g,address))==(mode==0?91u:0u);
  if(mode==0)ok=ok&&host_u64(h,address+8)==reply.value&&g->event_results==1;
  if(mode==1||mode==2)ok=ok&&g->event_stale==1;
  ok=ok&&((native_gpu_error(g)!=NULL)==(mode==5||mode==6));
  CFDictionaryRemoveValue(g->objects,(const void*)(uintptr_t)q->id);destroy(q);native_gpu_end(g);
 }
 printf("%s: event callback fault injection: 64-bit reply, stale token, replaced generation, stop, closing, GPU error and invalid address; no fabricated successful completion. Separate from hardware replay.\n",ok?"PASS":"FAIL");return ok;
}
static bool event_query_fixture(struct w2c_env *h,bool legacy,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+160000,flag=base+128000,results=base+132000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;g->test_sync_events=legacy;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+90000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if(tail>127488-8-(count)*4){ok=false;fail(g,"Event fixture overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 depth_words[0]=130;depth_words[3]=0;depth_words[4]=8;memcpy(cmd,depth_words,sizeof(depth_words));EMIT(16,15);cmd[0]=130;cmd[1]=0;EMIT(28,2);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[32]=117;EMIT(26,37);
 /* More pending results than the fixed callback pool; each event follows an
    actual original-shader draw. No fake completion or visibility results. */
 for(unsigned i=0;i<384;i++){
  uint64_t address=results+i*32;host_put32(h,address,0);host_put32(h,address+8,0);host_put32(h,address+12,0);
  cmd[0]=2000+i;cmd[1]=i%3==0?0:i%3==1?2:3;cmd[2]=(uint32_t)address;cmd[3]=address>>32;EMIT(44,4);
  memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);cmd[0]=2000+i;cmd[1]=100+i;EMIT(46,2);
 }
 /* Re-End the same handle: only its newest token may publish after a later
    completion. This also exercises events with no additional draw. */
 for(unsigned i=0;i<128;i++){cmd[0]=2000;cmd[1]=1000+i;EMIT(46,2);}
 /* Destroy/reuse one ID with a different address; old callbacks must never
    target the replacement's result slot. */
 cmd[0]=2001;EMIT(4,1);uint64_t replacement=results+384*32;host_put32(h,replacement,0);
 cmd[0]=2001;cmd[1]=0;cmd[2]=(uint32_t)replacement;cmd[3]=replacement>>32;EMIT(44,4);cmd[0]=2001;cmd[1]=9001;EMIT(46,2);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)dst;cmd[10]=dst>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=905;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 uint64_t start=now_ns();if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&now_ns()<deadline){bool done=atomic_load_explicit(heap_word(g,flag),memory_order_acquire)==905&&atomic_load_explicit(heap_word(g,replacement),memory_order_acquire)==9001;for(unsigned i=0;i<384&&done;i++)if(i!=1)done=atomic_load_explicit(heap_word(g,results+i*32),memory_order_acquire)==(i?100+i:1127);if(done)break;struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 uint64_t elapsed=now_ns()-start;native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&atomic_load(heap_word(g,flag))==905&&atomic_load(heap_word(g,replacement))==9001;
 for(unsigned i=0;i<384&&ok;i++){if(i==1)continue;uint64_t address=results+i*32;ok=atomic_load(heap_word(g,address))==(i?100+i:1127);uint64_t value=host_u64(h,address+8);ok=ok&&(i%3==1?value!=0:value==1);}
 uint8_t pixels[1024];host_load(h,pixels,dst,1024);int expected[]={200,100,50,255};for(unsigned i=0;i<256&&ok;i++)for(unsigned c=0;c<4;c++)if(abs((int)pixels[i*4+c]-expected[c])>1)ok=false;
 ok=ok&&g->draw.draws==384&&g->event_pending==0&&(legacy?g->event_requests==0:g->event_requests==513)&&g->event_peak<=EVENT_REPLIES;
 snprintf(report,capacity,"%s: event-query %s: 384 original shader draws/256 pixels,513 ENDs; latest-token/replaced-handle/type0,2,3 values checked; synchronous completion calls %llu, event requests/results/stale %llu/%llu/%llu peak%u pressure%llu; %.3fms wall. CPU timestamp shim preserved; no GPU timing or iPhone FPS claim. %s",ok?"PASS":"FAIL",legacy?"legacy":"async",(unsigned long long)g->metrics.windows[METRIC_COMPLETION].count,(unsigned long long)g->event_requests,(unsigned long long)g->event_results,(unsigned long long)g->event_stale,g->event_peak,(unsigned long long)g->event_pressure,elapsed/1e6,native_gpu_error(g)?native_gpu_error(g):"");native_gpu_end(g);return ok;
}
#endif
