/* Separate production-ring fixture retains all 0.19 fixture counters. */
static bool query_fixture(struct w2c_env *h,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;
 g->draw.fixture=true;host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+60000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if((count)>128||tail>127488-8-(count)*4){ok=false;fail(g,"Query fixture ring overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_replay.inc"
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=777;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);
 while(ok&&!native_gpu_error(g)&&now_ns()<deadline){bool done=atomic_load_explicit(heap_word(g,flag),memory_order_acquire)==777;for(unsigned i=0;i<Q_TOTAL&&done;i++){if(i==19)continue;done=atomic_load_explicit(heap_word(g,dst+i*32),memory_order_acquire)==sequences[i];}if(done)break;struct timespec t={0,1000000};nanosleep(&t,NULL);}
 ok=ok&&!native_gpu_error(g)&&now_ns()<deadline;
 native_gpu_stop(g); /* joins and cancels contexts before fixture host memory is released */
 for(unsigned i=0;i<Q_TOTAL&&ok;i++){uint8_t data[32];host_load(h,data,dst+i*32,32);uint32_t seq;memcpy(&seq,data,4);uint64_t value;memcpy(&value,data+8,8);
  if(i==19){if(seq!=0){fail(g,"Destroyed active query published into retired result storage");ok=false;}}
  else if(seq!=sequences[i]||value!=expected[i]){char why[220];snprintf(why,sizeof(why),"Occlusion case %u: got seq %u count %llu, expected %u/%llu",i,seq,(unsigned long long)value,sequences[i],(unsigned long long)expected[i]);fail(g,why);ok=false;}
  for(unsigned j=4;j<32&&ok;j++)if((j<8||j>=16)&&data[j]!=0xa5){fail(g,"Occlusion result wrote outside protocol words");ok=false;}
  if(ok)g->query_cases++;
 }
 if(ok&&(g->query_cases!=Q_TOTAL||g->query_pending_pages||g->query_gpu_bytes||g->query_active_count||g->query_pressure_waits||!g->query_stale||!g->query_pages_allocated||g->query_pages_allocated>QUERY_PAGES||g->query_resolved_slots!=g->query_segments||g->query_resolves!=g->query_map_requests||g->query_copies!=g->query_resolves||g->query_maps_completed!=g->query_map_requests)){fail(g,"Query cleanup, stale suppression or bounded asynchronous pool verification failed");ok=false;}
 char summary[2200];native_gpu_summary(g,summary,sizeof(summary));snprintf(report,capacity,"%s: native counting occlusion/predicate queries: %u cases; exact 0/1/64/256/320/512/768/1024 results, repeated draws, depth/stencil/scissor rejection, MSAA coverage, overlapping scopes, pass/submit splits, empty scopes, pending reuse, destroyed IDs and protocol guards. Four bounded pages, asynchronous maps; no per-query completion wait. Original IM geometry; not a scene.\n%s\n%s",ok?"PASS":"FAIL",g->query_cases,summary,native_gpu_error(g)?native_gpu_error(g):"");native_gpu_end(g);return ok;
}
static bool query_mask_boundary_fixture(struct w2c_env *h,uint32_t mask){
 uint64_t base=atomic_load(&h->memory.size)-262144;NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+15000000000ull;while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}bool ready=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}while(0)
 #include "query_setup.inc"
 memset(cmd,0,sizeof(cmd));cmd[0]=13001;cmd[1]=2;cmd[2]=cmd[3]=16;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=28;cmd[8]=0x28;cmd[9]=4;EMIT(3,10);
 memset(cmd,0,sizeof(cmd));cmd[0]=13001;EMIT(26,37);memset(cmd,0,sizeof(cmd));for(unsigned i=1;i<5;i++)cmd[i]=0x3f800000;cmd[5]=mask;EMIT(27,6);
 memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
 #undef EMIT
 if(ready){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);bool ok=ready&&native_gpu_error(g)&&strstr(native_gpu_error(g),"Metal partial/zero pipeline sample mask is not implemented by pinned backend")&&g->draw.draws==0&&g->buffer_bytes==0&&g->texture_bytes==0;native_gpu_end(g);return ok;
}
static bool query_invalid_fixture(struct w2c_env *h,unsigned which){
 if(which>=6)return query_mask_boundary_fixture(h,which==6?5:0);
 uint64_t base=atomic_load(&h->memory.size)-262144,address=base+131072;NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+15000000000ull;while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}uint32_t commands[]={44,4,11165,1,(uint32_t)address,(uint32_t)(address>>32),45,1,11165,45,1,11165};unsigned words=12;const char *expected="Occlusion Begin ordering";
 if(which==1){commands[6]=46;commands[7]=2;commands[9]=1;words=10;expected="Occlusion End without matching Begin";}
 if(which==2){commands[8]=999;words=9;expected="Begin query object missing";}
 if(which==3){commands[4]++;words=6;expected="Query type/descriptor/result address unsupported";}
 if(which==4){commands[3]=4;words=6;expected="Query type/descriptor/result address unsupported";}
 if(which==5){commands[4]=0;commands[5]=1;words=6;expected="Query type/descriptor/result address unsupported";}
 bool ready=native_gpu_ready(g);if(ready){host_store(h,base+64,commands,words*4);atomic_store_explicit(heap_word(g,base),64+words*4,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);bool ok=ready&&native_gpu_error(g)&&strstr(native_gpu_error(g),expected)&&!g->query_gpu_bytes&&!g->query_pending_pages;native_gpu_end(g);return ok;
}
