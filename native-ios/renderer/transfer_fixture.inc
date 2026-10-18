#ifdef NATIVE_PROBE_CLI
static uint64_t transfer_baseline;
static bool transfer_fixture(struct w2c_env *h,bool legacy){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;g->test_legacy_transfers=legacy;g->test_drain_controlled_clock=true;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+60000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if(tail>127488-8-(count)*4){ok=false;fail(g,"Transfer replay overflow");}else{host_put32(h,base+tail,op);host_put32(h,base+tail+4,count);host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 cmd[0]=118;cmd[1]=2;cmd[2]=cmd[3]=16;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=28;cmd[8]=0x28;cmd[9]=1;EMIT(3,10);
 for(unsigned id=4000;id<=4001;id++){cmd[0]=id;cmd[1]=8;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);cmd[0]=id;cmd[1]=0;cmd[2]=8;cmd[3]=1;cmd[4]=2;EMIT(12,5);}
 cmd[0]=105;EMIT(29,1);cmd[0]=1;float vp[]={0,0,16,16,0,1};memcpy(cmd+1,vp,sizeof(vp));EMIT(30,7);
 for(unsigned i=0;i<128;i++){
  memset(cmd,0,sizeof(cmd));cmd[0]=116;EMIT(5,7);
  cmd[0]=1;cmd[1]=i%16;cmd[2]=i/16;cmd[3]=cmd[1]+1;cmd[4]=cmd[2]+1;EMIT(31,5);
  memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
  memset(cmd,0,sizeof(cmd));cmd[0]=118;cmd[2]=i%16;cmd[3]=i/16;cmd[5]=116;cmd[7]=1;cmd[8]=cmd[2];cmd[9]=cmd[3];cmd[11]=cmd[8]+1;cmd[12]=cmd[9]+1;cmd[13]=1;EMIT(34,14);
 }
 /* Both source and destination hazards must participate in upload epochs. */
 memset(cmd,0,sizeof(cmd));cmd[0]=4001;cmd[5]=4000;EMIT(34,14);
 cmd[0]=4000;cmd[1]=0;cmd[2]=8;cmd[3]=3;cmd[4]=4;EMIT(12,5);
 cmd[0]=4001;cmd[1]=4;cmd[2]=4;cmd[3]=99;EMIT(47,4);
 for(unsigned id=4000;id<=4001;id++){memset(cmd,0,sizeof(cmd));cmd[0]=id;cmd[9]=(uint32_t)(dst+1024+(id-4000)*8);cmd[10]=(dst+1024+(id-4000)*8)>>32;cmd[13]=1;cmd[14]=8;EMIT(42,17);cmd[0]=id;EMIT(4,1);}
 memset(cmd,0,sizeof(cmd));cmd[0]=118;cmd[9]=(uint32_t)dst;cmd[10]=dst>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
 cmd[0]=116;EMIT(4,1);cmd[0]=118;EMIT(4,1);
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=929;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 uint64_t started=now_ns();if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=929&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 uint64_t wall=now_ns()-started;native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)==929;
 uint8_t pixels[1024];host_load(h,pixels,dst,sizeof(pixels));for(unsigned i=0;i<256&&ok;i++)for(unsigned c=0;c<4;c++){const unsigned rgba[]={200,100,50,255};unsigned expected=i<128?rgba[c]:0;if(abs((int)pixels[i*4+c]-(int)expected)>1)ok=false;}
 uint32_t hazard[4];host_load(h,hazard,dst+1024,sizeof(hazard));ok=ok&&hazard[0]==3&&hazard[1]==4&&hazard[2]==1&&hazard[3]==99;
 if(legacy)transfer_baseline=g->submissions;else ok=ok&&g->submissions*4<transfer_baseline&&g->transfer_submits_avoided>=240;
 printf("%s: ordered clear/draw/copy legacy%u: 128 original draws, 256 verified RGBA pixels; submissions %llu, batches %llu, transfer submits avoided %llu; wall %.3f ms, handler wall %.3f ms, uniform bytes %llu. Host fixture, not iPhone FPS. %s\n",ok?"PASS":"FAIL",legacy,(unsigned long long)g->submissions,(unsigned long long)g->draw.batches,(unsigned long long)g->transfer_submits_avoided,wall/1e6,g->metrics.frame_busy/1e6,(unsigned long long)g->draw.uniform_upload_bytes,native_gpu_error(g)?native_gpu_error(g):"");native_gpu_end(g);return ok;
}
#endif
