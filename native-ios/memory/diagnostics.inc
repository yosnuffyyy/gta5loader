static void native_memory_record(struct w2c_env *h,const char *line,bool emit){if(pthread_mutex_lock(&h->memory_report_lock))abort();snprintf(h->memory_history[h->memory_history_count++%8],512,"%s",line);if(pthread_mutex_unlock(&h->memory_report_lock))abort();if(emit&&h->progress)h->progress(h->progress_context,line);}
void native_memory_summary(struct w2c_env *h){char lines[8][512];unsigned count;if(pthread_mutex_lock(&h->memory_report_lock))abort();count=h->memory_history_count;unsigned n=count<8?count:8;for(unsigned i=0;i<n;i++)memcpy(lines[i],h->memory_history[(count-n+i)%8],512);if(pthread_mutex_unlock(&h->memory_report_lock))abort();if(h->progress){char summary[160];snprintf(summary,sizeof(summary),"Allocator retry/failure history: latest %u of %u bounded events; original growth target and 4 GiB linear limit retained.",n,count);h->progress(h->progress_context,summary);for(unsigned i=0;i<n;i++)h->progress(h->progress_context,lines[i]);}}
/* Called from the original request path while its allocator lock is held.
   Never call GrowBuddy::GetLargestAvailableBlock here: it can grow the heap. */
uint64_t native_stream_growth_context(w2c_gta *e,uint64_t owner,uint64_t request){
 struct w2c_env *h=e->w2c_env_instance;uint32_t count;host_load(h,&count,owner+508,4);uint64_t cap=native_stream_growth_limit(request,count);
 if(cap>33554432){unsigned log=atomic_fetch_add_explicit(&h->memory_growth_logs,1,memory_order_relaxed);char line[512];
  uint64_t used=w2c_gta_f3040(e,owner,UINT32_MAX),heap=w2c_gta_f3041(e,owner),free_bytes=0,largest=0;
  for(unsigned i=0;i<count&&i<32;i++)if(host_u64(h,owner+16384+i*8)){uint64_t child=owner+512+i*488,n=w2c_gta_f3021(e,child);free_bytes+=w2c_gta_f3020(e,child);if(n>largest)largest=n;}
  snprintf(line,sizeof(line),"Streaming allocation retry: owner 0x%llx request %llu, segment cap %llu; growth target %llu; live %llu, retained heap %llu, actual free %llu, largest %llu, segments %u/32; linear committed %llu/4294967296. Locked allocator snapshot; process footprint/headroom reported separately.",(unsigned long long)owner,(unsigned long long)request,(unsigned long long)cap,(unsigned long long)host_u64(h,owner+488),(unsigned long long)used,(unsigned long long)heap,(unsigned long long)free_bytes,(unsigned long long)largest,count,(unsigned long long)atomic_load(&h->memory.size));native_memory_record(h,line,log<8);
 }
 return cap;
}
void native_stream_allocation_result(w2c_gta *e,uint64_t owner,uint64_t request,uint64_t result){
 struct w2c_env *h=e->w2c_env_instance;if(!result){unsigned log=atomic_fetch_add_explicit(&h->memory_failure_logs,1,memory_order_relaxed);char line[512];snprintf(line,sizeof(line),"Streaming allocation FAILED: owner 0x%llx request %llu; returned NULL; linear committed %llu/4294967296. No archive destination is fabricated.",(unsigned long long)owner,(unsigned long long)request,(unsigned long long)atomic_load(&h->memory.size));native_memory_record(h,line,log<8);}
}

uint64_t native_stream_default_limit(w2c_gta *e,uint64_t owner){uint32_t count;host_load(e->w2c_env_instance,&count,owner+508,4);return native_stream_growth_limit(0,count);}

/* Restore the pre-profile GetMemoryAvailable contract: actual retained heap
   minus live allocations, with the original 1 MiB largest-block / 4 MiB free
   reserve and genuine growth failures. The v7 target-used shortcut can lie or
   unsigned-underflow; the growth target is not an enforced total heap budget. */
uint64_t native_stream_available(w2c_gta *e,uint64_t owner){
 struct w2c_env *h=e->w2c_env_instance;uint64_t largest=w2c_gta_f3043(e,owner);if(largest<1048576)return largest;
 for(;;){uint64_t heap=w2c_gta_f3041(e,owner),used=w2c_gta_f3040(e,owner,UINT32_MAX);if(used>heap)return 0;if(heap-used>=4194304)return heap-used;
  if(w2c_gta_f3036(e,owner)==UINT32_MAX){if(host_u64(h,owner+488)!=heap){host_put64(h,owner+488,heap);char line[256];snprintf(line,sizeof(line),"Allocator growth exhausted: owner 0x%llx retained %llu live %llu; actual free reserve below 4 MiB. Original failure recalibration retained.",(unsigned long long)owner,(unsigned long long)heap,(unsigned long long)used);native_memory_record(h,line,true);}return 0;}
 }
}
