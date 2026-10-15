static bool layout_cache_fixture(struct w2c_env *h,bool legacy,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;
#ifdef NATIVE_PROBE_CLI
 g->test_legacy_layout_ids=legacy;
#else
 if(legacy){native_gpu_end(g);return false;}
#endif
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+30000000000ull;while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if((count)>128||tail>127488-8-(count)*4){ok=false;fail(g,"Layout fixture ring overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 els[0]=903;memcpy(cmd,els,sizeof(els));EMIT(15,23);els[0]=904;els[13]=0;memcpy(cmd,els,sizeof(els));EMIT(15,23);
 unsigned ids[]={103,903,103,903,904,103};for(unsigned test=0;test<6;test++){cmd[0]=ids[test];EMIT(19,1);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)(dst+test*1024);cmd[10]=(dst+test*1024)>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);cmd[0]=116;EMIT(7,1);}
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=889;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=889&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&g->draw.draws==6&&g->metrics.windows[METRIC_FRAME].count==5&&g->metrics.cost_count==5&&g->draw.creates==(legacy?3u:2u)&&g->draw.hits==(legacy?3u:4u);
 for(unsigned i=0;i<5&&ok;i++){FrameCost *c=g->metrics.costs+i;uint64_t expected_creates=(i==3||(legacy&&i==0))?1:0;if(c->frame!=i+2||c->commands!=4||c->draws!=1||c->creates!=expected_creates||(expected_creates==0&&c->pipeline!=0))ok=false;}
 for(unsigned test=0;test<6&&ok;test++){uint8_t pixels[1024];host_load(h,pixels,dst+test*1024,sizeof(pixels));for(unsigned i=0;i<256&&ok;i++){if(test!=4){static const unsigned rgba[]={200,100,50,255};for(unsigned c=0;c<4;c++)if(abs((int)pixels[i*4+c]-(int)rgba[c])>1)ok=false;}else if(pixels[i*4]||pixels[i*4+1]||abs((int)pixels[i*4+2]-25)>1)ok=false;}}
 if(report&&capacity)snprintf(report,capacity,"%s: equivalent vertex-layout identity %s: six draws/1536 pixels; creates/hits %llu/%llu; changed color offset remains a distinct pipeline and changes pixels. Pipeline API CPU wall %.3f ms; driver cache/order dependent, not iPhone FPS.",ok?"PASS":"FAIL",legacy?"0.21 reference":"semantic key",(unsigned long long)g->draw.creates,(unsigned long long)g->draw.hits,g->metrics.windows[METRIC_PIPELINE].total_ns/1e6);native_gpu_end(g);return ok;
}
