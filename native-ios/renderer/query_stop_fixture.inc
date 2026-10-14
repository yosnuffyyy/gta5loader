static bool query_stop_fixture(struct w2c_env *h){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;
 host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+15000000000ull;while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}while(0)
 #include "query_setup.inc"
 host_put32(h,dst,0);cmd[0]=11165;cmd[1]=1;cmd[2]=(uint32_t)dst;cmd[3]=dst>>32;EMIT(44,4);cmd[0]=11165;EMIT(45,1);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
 host_put32(h,flag,0);cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=888;EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=888&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&atomic_load(heap_word(g,flag))==888&&atomic_load(heap_word(g,dst))==0&&g->query_publications==0&&g->query_active_count==0&&g->query_pending_pages==0&&g->query_gpu_bytes==0;native_gpu_end(g);return ok;
}
