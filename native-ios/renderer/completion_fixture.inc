#ifdef NATIVE_PROBE_CLI
static bool completion_callback_fixture(struct w2c_env *h){
 bool ok=true;uint64_t address=atomic_load(&h->memory.size)-1024;
 for(unsigned mode=0;mode<7&&ok;mode++){
  NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;native_gpu_execution_option(g,true);
  Object *q=create(g,700,44);if(!q){native_gpu_end(g);return false;}
  q->words=4;q->meta=calloc(4,sizeof(uint32_t));if(!q->meta){native_gpu_end(g);return false;}q->meta[2]=(uint32_t)address;q->meta[3]=address>>32;q->event_token=1;
  host_put32(h,address,0);host_put32(h,address+8,0);host_put32(h,address+12,0);
  EventReply reply={.owner=g,.id=700,.generation=q->generation,.token=1,.sequence=91,.value=UINT64_C(0x123456789abcdef0),.busy=true};g->event_pending=1;
  if(mode==1)q->event_token++;if(mode==2)q->generation++;if(mode==3)atomic_store(&g->stop,true);if(mode==4)atomic_store(&g->closing,true);if(mode==6)q->meta[2]++;
  unsigned batch;CompletionReply ticket={.id=reply.id,.generation=reply.generation,.token=reply.token,.sequence=reply.sequence,.value=reply.value,.started=now_ns()};
  ok=completion_add(g->completion,ticket,&batch)==COMPLETION_REGISTER;
  CompletionContext context={.owner=g,.batch=batch};WGPUStringView message=WGPU_STRING_VIEW_INIT;
  completion_done(mode==5?WGPUQueueWorkDoneStatus_Error:WGPUQueueWorkDoneStatus_Success,message,&context,NULL);
  ok=ok&&g->completion->pending==0&&g->event_pending==0&&atomic_load(heap_word(g,address))==(mode==0?91u:0u);
  if(mode==0)ok=ok&&host_u64(h,address+8)==reply.value&&g->event_results==1;
  if(mode==1||mode==2)ok=ok&&g->event_stale==1;
  ok=ok&&((native_gpu_error(g)!=NULL)==(mode==5||mode==6));
  CFDictionaryRemoveValue(g->objects,(const void*)(uintptr_t)q->id);destroy(q);native_gpu_end(g);
 }
 printf("%s: V2 completion fault injection: 64-bit reply, stale token, replaced generation, stop, closing, GPU error and invalid address; no fabricated successful completion. Separate from hardware replay.\n",ok?"PASS":"FAIL");return ok;
}
static bool completion_replay(struct w2c_env *h,bool v2,bool profile,unsigned pair,unsigned order){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+160000,flag=base+128000,results=base+132000;
 native_profile_set_enabled(profile);NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);native_profile_set_enabled(false);if(!g)return false;native_gpu_execution_option(g,v2);g->draw.fixture=true;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+90000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if(tail>127488-8-(count)*4){ok=false;fail(g,"Event fixture overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 depth_words[0]=130;depth_words[3]=0;depth_words[4]=8;memcpy(cmd,depth_words,sizeof(depth_words));EMIT(16,15);cmd[0]=130;cmd[1]=0;EMIT(28,2);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[32]=117;EMIT(26,37);
 /* More pending results than the fixed callback pool; six event fences per
    actual original-shader draw. This is a SYNTHETIC CONTRACT replay, not a phone capture. No fake completion or visibility results. */
 for(unsigned i=0;i<384;i++){
  uint64_t address=results+i*32;host_put32(h,address,0);host_put32(h,address+8,0);host_put32(h,address+12,0);
  cmd[0]=2000+i;cmd[1]=i%3==0?0:i%3==1?2:3;cmd[2]=(uint32_t)address;cmd[3]=address>>32;EMIT(44,4);
  if(i%6==0){memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);}cmd[0]=2000+i;cmd[1]=100+i;EMIT(46,2);
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
 ok=ok&&g->draw.draws==64&&g->event_pending==0&&g->event_requests==513&&g->event_peak<=EVENT_REPLIES;
 TimingSummary latency=timing_summary(&g->completion_latency);
 ok=ok&&(!profile||g->profile)&&(!v2||g->completion_registers<256);
 printf("V2_BENCH {\"ok\":%s,\"backend\":\"%s\",\"profile\":%s,\"pair\":%u,\"order\":%u,\"wall_ms\":%.6f,\"command_cpu_ms\":%.6f,\"end_cpu_ms\":%.6f,\"submissions\":%llu,\"draws\":%llu,\"registrations\":%llu,\"joined\":%llu,\"known_complete\":%llu,\"query_p50_ms\":%.6f,\"query_p95_ms\":%.6f,\"query_max_ms\":%.6f,\"pending_peak\":%u,\"state_bytes\":%zu}\n",
 ok?"true":"false",v2?"v2":"existing",profile?"true":"false",pair,order,elapsed/1e6,g->profile?g->profile->command_cpu/1e6:-1,g->profile?g->profile->op[46].cpu/1e6:-1,(unsigned long long)g->submissions,(unsigned long long)g->draw.draws,(unsigned long long)(v2?g->completion_registers:g->event_requests),(unsigned long long)g->completion_joins,(unsigned long long)g->completion_ready,latency.median_ms,latency.p95_ms,g->completion_latency.worst/1e6,g->event_peak,v2?sizeof(*g->completion):0);
 if(!ok)printf("FAIL: V2 event replay %s\n",native_gpu_error(g)?native_gpu_error(g):"pixel/result/ownership assertion");native_gpu_end(g);return ok;
}
/* Queue writes without a DRAW must be submitted before their event completes.
   The producer waits on each reply before adding the next commands: no PRESENT
   or later readback is available to accidentally make the event progress. */
static bool completion_write_fixture(struct w2c_env *h){
 uint64_t base=atomic_load(&h->memory.size)-262144,result=base+132000,dst=base+140000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;native_gpu_execution_option(g,true);
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+15000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[17]={0};
 #define EMIT(op,n) do{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(n));host_store(h,base+tail+8,cmd,(n)*4);tail+=8+(n)*4;}while(0)
 cmd[0]=900;cmd[1]=16;cmd[2]=1;EMIT(11,5);
 cmd[0]=901;cmd[1]=0;cmd[2]=(uint32_t)result;cmd[3]=result>>32;EMIT(44,4);host_put32(h,result,0);
 for(unsigned phase=1;phase<=3&&ok;phase++){
  if(phase!=2){cmd[0]=900;cmd[1]=0;cmd[2]=8;cmd[3]=0xabcdef00+phase;cmd[4]=0x12345678;EMIT(12,5);}
  cmd[0]=901;cmd[1]=phase;EMIT(46,2);atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);
  while(atomic_load_explicit(heap_word(g,result),memory_order_acquire)!=phase&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
  ok=!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,result),memory_order_acquire)==phase&&host_u64(h,result+8)==1;
 }
 memset(cmd,0,sizeof(cmd));cmd[0]=900;cmd[9]=(uint32_t)dst;cmd[10]=dst>>32;cmd[13]=1;cmd[14]=16;EMIT(42,17);
 cmd[0]=(uint32_t)(result+32);cmd[1]=(result+32)>>32;cmd[2]=77;host_put32(h,result+32,0);EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(atomic_load_explicit(heap_word(g,result+32),memory_order_acquire)!=77&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&atomic_load(heap_word(g,result+32))==77&&host_u64(h,dst)==UINT64_C(0x12345678abcdef03)&&g->completion_write_submits==2&&g->completion_ready==1&&g->completion_registers==2&&g->event_pending==0;
 printf("%s: V2 isolated upload/event progress: two writes submitted before replies, completed fence reused once, later mutation invalidates completion, exact 64-bit readback; no DRAW/PRESENT required. %s\n",ok?"PASS":"FAIL",native_gpu_error(g)?native_gpu_error(g):"");native_gpu_end(g);return ok;
}
#endif
