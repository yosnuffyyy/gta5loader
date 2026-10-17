/* Called from the original request path while its allocator lock is held.
   Never call GrowBuddy::GetLargestAvailableBlock here: it can grow the heap. */
uint64_t native_stream_growth_context(w2c_gta *e,uint64_t owner,uint64_t request){
 uint64_t cap=native_stream_growth_limit(request);struct w2c_env *h=e->w2c_env_instance;
 if(cap>33554432&&atomic_fetch_add_explicit(&h->memory_growth_logs,1,memory_order_relaxed)<8){
  uint64_t used=w2c_gta_f3040(e,owner,UINT32_MAX),heap=w2c_gta_f3041(e,owner),free_bytes=0,largest=0;uint32_t count;host_load(h,&count,owner+508,4);
  for(unsigned i=0;i<count&&i<32;i++)if(host_u64(h,owner+16384+i*8)){uint64_t child=owner+512+i*488,n=w2c_gta_f3021(e,child);free_bytes+=w2c_gta_f3020(e,child);if(n>largest)largest=n;}
  message(h,"Streaming allocation retry: owner 0x%llx request %llu, segment cap %llu; budget field %llu; live %llu, retained heap %llu, actual free %llu, largest %llu, segments %u/32; linear committed %llu/4294967296. Locked allocator snapshot; process footprint/headroom reported separately.",(unsigned long long)owner,(unsigned long long)request,(unsigned long long)cap,(unsigned long long)host_u64(h,owner+488),(unsigned long long)used,(unsigned long long)heap,(unsigned long long)free_bytes,(unsigned long long)largest,count,(unsigned long long)atomic_load(&h->memory.size));
 }
 return cap;
}
void native_stream_allocation_result(w2c_gta *e,uint64_t owner,uint64_t request,uint64_t result){
 struct w2c_env *h=e->w2c_env_instance;if(!result&&atomic_fetch_add_explicit(&h->memory_failure_logs,1,memory_order_relaxed)<8)message(h,"Streaming allocation FAILED: owner 0x%llx request %llu; returned NULL; linear committed %llu/4294967296. No archive destination is fabricated.",(unsigned long long)owner,(unsigned long long)request,(unsigned long long)atomic_load(&h->memory.size));
}
