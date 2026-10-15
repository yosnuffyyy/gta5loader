/* Deterministic production command fixtures. Timings are CI CPU API times,
   not physical iPhone FPS or GPU execution measurements. */
static bool pipeline_state_fixture(struct w2c_env *h,bool raw,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;
#ifdef NATIVE_PROBE_CLI
 g->test_raw_pipeline_keys=raw;
#else
 if(raw){native_gpu_end(g);return false;}
#endif
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+45000000000ull;while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if(tail>127488-8-(count)*4){ok=false;fail(g,"Pipeline state fixture ring overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 for(unsigned n=0;n<32;n++){memset(cmd,0,sizeof(cmd));cmd[0]=1000+n;cmd[11]=15;for(unsigned i=0;i<3;i++)cmd[5+i]=1+((n>>i)&1);EMIT(16,68);}
 for(unsigned test=0;test<64;test++){unsigned n=test%32;memset(cmd,0,sizeof(cmd));cmd[0]=1000+n;cmd[5]=UINT32_MAX;EMIT(27,6);cmd[0]=110;cmd[1]=(n&8)?42:57;cmd[2]=0;EMIT(22,3);cmd[0]=(n&16)?105:104;EMIT(29,1);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)(dst+test*1024);cmd[10]=(dst+test*1024)>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);}
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=890;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=890&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&g->draw.draws==64&&g->draw.creates==(raw?32u:1u)&&g->draw.hits==(raw?32u:63u);
 for(unsigned test=0;test<64&&ok;test++){uint8_t pixels[1024];host_load(h,pixels,dst+test*1024,sizeof(pixels));static const int rgba[]={200,100,50,255};for(unsigned i=0;i<256&&ok;i++)for(unsigned c=0;c<4;c++)if(abs((int)pixels[i*4+c]-rgba[c])>1)ok=false;}
 if(report&&capacity)snprintf(report,capacity,"%s: pipeline state equivalence %s: 64 draws/16384 checked pixels; creates/hits %llu/%llu, evictions %llu; pipeline API wall %.3fms. Cache remains 128; retained native pipelines %llu (driver bytes unknown), no iPhone FPS claim. %s",ok?"PASS":"FAIL",raw?"0.22 raw key":"semantic key",(unsigned long long)g->draw.creates,(unsigned long long)g->draw.hits,(unsigned long long)g->metrics.pipeline_evictions,g->metrics.windows[METRIC_PIPELINE].total_ns/1e6,(unsigned long long)g->draw.creates,native_gpu_error(g)?native_gpu_error(g):"");native_gpu_end(g);return ok;
}
static bool fresh_object_fixture(struct w2c_env *h,bool old,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000,address=base+126000;NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;
#ifdef NATIVE_PROBE_CLI
 g->test_flush_new_objects=old;
#else
 if(old){native_gpu_end(g);return false;}
#endif
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+30000000000ull;while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if(tail>127488-8-(count)*4){ok=false;fail(g,"Fresh metadata fixture ring overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 cmd[0]=11165;cmd[1]=1;cmd[2]=(uint32_t)address;cmd[3]=address>>32;EMIT(44,4);cmd[0]=11165;EMIT(45,1);
 for(unsigned i=0;i<8;i++){memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);els[0]=2000+i;memcpy(cmd,els,sizeof(els));EMIT(15,23);}
 cmd[0]=11165;cmd[1]=891;EMIT(46,2);memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)dst;cmd[10]=dst>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=892;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=892&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);uint64_t samples=host_u64(h,address+8);ok=ok&&!native_gpu_error(g)&&g->draw.draws==8&&g->draw.batches==(old?8u:1u)&&samples==2048&&atomic_load_explicit(heap_word(g,address),memory_order_acquire)==891;
 uint8_t pixels[1024];host_load(h,pixels,dst,sizeof(pixels));static const int rgba[]={200,100,50,255};for(unsigned i=0;i<256&&ok;i++)for(unsigned c=0;c<4;c++)if(abs((int)pixels[i*4+c]-rgba[c])>1)ok=false;
 if(report&&capacity)snprintf(report,capacity,"%s: fresh-object ordering %s: 8 draws, 256 pixels, exact 2048 occlusion samples; batches %llu, fresh flushes avoided %llu. Existing-resource uploads/destruction still flush. %s",ok?"PASS":"FAIL",old?"0.22 reference":"corrected",(unsigned long long)g->draw.batches,(unsigned long long)g->fresh_flushes_avoided,native_gpu_error(g)?native_gpu_error(g):"");native_gpu_end(g);return ok;
}
