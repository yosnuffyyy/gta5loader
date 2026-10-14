/* Actual Metal sample counts, asynchronously resolved in bounded pages.
   A physical segment is credited to every overlapping logical D3D scope. */
static void query_publish(NativeGpu *g,Object *q){
 if(!query_ready_state(&q->query)||atomic_load(&g->stop)||atomic_load(&g->closing)||atomic_load(&g->failed))return;
 uint64_t address=q->meta[2]|((uint64_t)q->meta[3]<<32),value=query_value_state(&q->query);
 if(!check(g,address%4==0&&host_range(g->host,address,16),"Query publication address outside shared memory"))return;
 atomic_store_explicit(heap_word(g,address+8),(uint32_t)value,memory_order_relaxed);atomic_store_explicit(heap_word(g,address+12),(uint32_t)(value>>32),memory_order_relaxed);
 atomic_store_explicit(heap_word(g,address),q->query.sequence,memory_order_release);q->query.published=true;g->query_publications++;wake(g,address);
 if(g->host->progress&&g->query_publications<=8){char line[240];snprintf(line,sizeof(line),"Native occlusion result: query %u type %u seq %u; GPU samples %llu; protocol value %llu",q->id,q->query.type,q->query.sequence,(unsigned long long)q->query.sum,(unsigned long long)value);g->host->progress(g->host->progress_context,line);}
}
static void query_mapped(WGPUMapAsyncStatus status,WGPUStringView message,void *context,void *unused){
 (void)unused;QueryPage *page=context;NativeGpu *g=page->owner;bool cancel=atomic_load(&g->stop)||atomic_load(&g->closing);
 if(status==WGPUMapAsyncStatus_Success){const uint8_t *data=cancel?NULL:wgpuBufferGetConstMappedRange(page->map,0,page->used*8);
  if(!data&&!cancel)fail(g,"Occlusion mapped results unavailable");
  if(data&&!cancel&&!atomic_load(&g->failed))for(unsigned i=0;i<page->used;i++){uint64_t samples;memcpy(&samples,data+i*8,8);QuerySegment *s=page->segments+i;
   for(unsigned j=0;j<s->count;j++){Object *q=object(g,s->owners[j].id);if(!q||q->kind!=44||q->query.token!=s->owners[j].token){g->query_stale++;continue;}
    if(query_accept_state(&q->query,s->owners[j].token,samples)<0){fail(g,"Occlusion duplicate segment completion");break;}query_publish(g,q);
   }
  }
  if(page->map)wgpuBufferUnmap(page->map);
 }else if(!cancel){char why[700];size_t n=message.data?(message.length==WGPU_STRLEN?strlen(message.data):message.length):0;if(n>500)n=500;snprintf(why,sizeof(why),"Occlusion map failed: %.*s",(int)n,message.data?message.data:"");fail(g,why);}
 page->busy=false;page->used=0;if(g->query_pending_pages)g->query_pending_pages--;g->query_maps_completed++;
}
static bool query_allocate_page(NativeGpu *g,QueryPage *page){
 if(page->set)return true;page->owner=g;
 WGPUQuerySetDescriptor qd=WGPU_QUERY_SET_DESCRIPTOR_INIT;qd.type=WGPUQueryType_Occlusion;qd.count=QUERY_SEGMENTS;page->set=wgpuDeviceCreateQuerySet(g->device,&qd);
 WGPUBufferDescriptor bd=WGPU_BUFFER_DESCRIPTOR_INIT;bd.size=QUERY_RESOLVE_BYTES;bd.usage=WGPUBufferUsage_QueryResolve|WGPUBufferUsage_CopySrc;page->resolve=wgpuDeviceCreateBuffer(g->device,&bd);
 bd.size=QUERY_MAP_BYTES;bd.usage=WGPUBufferUsage_MapRead|WGPUBufferUsage_CopyDst;page->map=wgpuDeviceCreateBuffer(g->device,&bd);
 if(!check(g,page->set&&page->resolve&&page->map&&!atomic_load(&g->failed),"Occlusion page allocation failed"))return false;
 g->query_pages_allocated++;g->query_gpu_bytes+=QUERY_RESOLVE_BYTES+QUERY_MAP_BYTES+QUERY_SEGMENTS*8;if(g->query_gpu_bytes>g->query_gpu_peak)g->query_gpu_peak=g->query_gpu_bytes;return true;
}
static bool query_prepare_draw(NativeGpu *g){
 if(!g->query_active_count)return true;
 if(g->query_current&&g->query_pages[g->query_current-1].used==QUERY_SEGMENTS&&!draw_flush(g))return false;
 if(g->query_current)return true;
 uint64_t start=now_ns(),deadline=start+15000000000ull;bool pressure=false;
 for(;;){for(unsigned i=0;i<QUERY_PAGES;i++)if(!g->query_pages[i].busy){QueryPage *p=g->query_pages+i;if(!query_allocate_page(g,p))return false;g->query_current=i+1;if(pressure)g->query_pressure_ns+=now_ns()-start;return true;}
  if(!pressure){pressure=true;g->query_pressure_waits++;}wgpuDevicePoll(g->device,false,NULL);
  if(atomic_load(&g->stop)||atomic_load(&g->failed))return false;if(now_ns()>deadline)return check(g,false,"Occlusion bounded readback pool timeout");struct timespec t={0,1000000};nanosleep(&t,NULL);
 }
}
static bool query_begin_pass(NativeGpu *g){
 if(!g->query_active_count)return true;if(!check(g,g->query_current&&!g->query_pass,"Occlusion render-pass state invalid"))return false;
 QueryPage *p=g->query_pages+g->query_current-1;if(!check(g,p->used<QUERY_SEGMENTS&&!p->busy,"Occlusion segment page full or mapped"))return false;
 QuerySegment *s=p->segments+p->used;s->count=g->query_active_count;
 for(unsigned i=0;i<s->count;i++){Object *q=object(g,g->query_active[i]);if(!check(g,q&&q->kind==44&&query_reserve_state(&q->query),"Occlusion active scope was destroyed or invalid"))return false;s->owners[i]=(QueryRef){q->id,q->query.token};}
 wgpuRenderPassEncoderBeginOcclusionQuery(g->draw.pass,p->used);p->used++;g->query_pass=true;g->query_segments++;return !atomic_load(&g->failed);
}
static void query_end_pass(NativeGpu *g){
 if(!g->query_pass)return;
 wgpuRenderPassEncoderEndOcclusionQuery(g->draw.pass);g->query_pass=false;
 /* End the render pass first; all used slots resolve together at submission. */
 wgpuRenderPassEncoderEnd(g->draw.pass);wgpuRenderPassEncoderRelease(g->draw.pass);g->draw.pass=NULL;
}
static bool query_resolve_encoder(NativeGpu *g){
 if(!g->query_current)return true;QueryPage *p=g->query_pages+g->query_current-1;if(!p->used)return true;
 if(!check(g,!g->draw.pass&&!g->query_pass&&g->draw.encoder&&!p->busy&&p->used<=QUERY_SEGMENTS,"Occlusion page resolve ordering invalid"))return false;
 /* Slots are contiguous, used once, and all belong to this draw encoder.
    Offset zero satisfies the 256-byte resolve alignment without per-slot padding. */
 wgpuCommandEncoderResolveQuerySet(g->draw.encoder,p->set,0,p->used,p->resolve,0);
 wgpuCommandEncoderCopyBufferToBuffer(g->draw.encoder,p->resolve,0,p->map,0,p->used*8);
 g->query_resolves++;g->query_copies++;g->query_resolved_slots+=p->used;return !atomic_load(&g->failed);
}
static bool query_submitted(NativeGpu *g){
 if(!g->query_current)return true;QueryPage *p=g->query_pages+g->query_current-1;g->query_current=0;if(!p->used)return true;
 p->busy=true;g->query_pending_pages++;g->query_map_requests++;
 WGPUBufferMapCallbackInfo cb=WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;cb.mode=WGPUCallbackMode_AllowProcessEvents;cb.callback=query_mapped;cb.userdata1=p;
 wgpuBufferMapAsync(p->map,WGPUMapMode_Read,0,p->used*8,cb);return !atomic_load(&g->failed);
}
static bool begin_query(NativeGpu *g,const uint8_t *payload){
 Object *q=object(g,gpu_word(payload,0));if(!check(g,q&&q->kind==44&&q->words==4,"Begin query object missing"))return false;
 if(q->meta[1]==0||q->meta[1]==2)return true; /* D3D disables Begin on event/timestamp; protocol reference ignores it. */
 if(q->meta[1]==3)return true; /* Existing CPU-timestamp/disjoint shim retained; not claimed GPU timing. */
 if(!check(g,q->meta[1]==1||q->meta[1]==5,"Begin query type not implemented"))return false;
 if(!check(g,g->counting_queries&&!q->query.active&&g->query_active_count<QUERY_SCOPES&&g->query_token<UINT64_MAX,"Occlusion Begin ordering/capability/scope limit invalid"))return false;
 draw_end_pass(g);if(!query_begin_state(&q->query,++g->query_token))return check(g,false,"Occlusion Begin state invalid");g->query_active[g->query_active_count++]=q->id;g->query_begins++;return !atomic_load(&g->failed);
}
static bool end_occlusion(NativeGpu *g,Object *q,uint32_t sequence){
 if(!check(g,q->query.active,"Occlusion End without matching Begin"))return false;draw_end_pass(g);
 unsigned at=0;while(at<g->query_active_count&&g->query_active[at]!=q->id)at++;if(!check(g,at<g->query_active_count,"Occlusion active scope missing"))return false;
 g->query_active[at]=g->query_active[--g->query_active_count];if(!query_end_state(&q->query,sequence))return check(g,false,"Occlusion End state invalid");g->query_ends++;if(!q->query.pending)g->query_empty++;query_publish(g,q);return !atomic_load(&g->failed);
}
static void query_destroy_scope(NativeGpu *g,Object *q){
 if(q->kind!=44)return;q->query.token=0;q->query.active=q->query.ended=false;for(unsigned i=0;i<g->query_active_count;i++)if(g->query_active[i]==q->id){g->query_active[i]=g->query_active[--g->query_active_count];break;}
 /* Pending jobs retain IDs/tokens, never Object pointers or result addresses. */
}
static void query_cleanup(NativeGpu *g){
 for(unsigned i=0;i<QUERY_PAGES;i++)if(g->query_pages[i].busy)wgpuBufferUnmap(g->query_pages[i].map);
 if(g->device)wgpuDevicePoll(g->device,false,NULL); /* Cancellation, never a shutdown GPU-completion wait. Contexts survive through instance release. */
 /* Pinned wgpu-native 29.0.1.1 QuerySetDestroy drops the core ID; final
    QuerySetRelease also drops it. We own the only public reference, so release
    once. Submitted command buffers retain their internal resource references. */
 for(unsigned i=0;i<QUERY_PAGES;i++){QueryPage *p=g->query_pages+i;if(p->set){wgpuQuerySetRelease(p->set);p->set=NULL;}if(p->resolve){wgpuBufferDestroy(p->resolve);wgpuBufferRelease(p->resolve);p->resolve=NULL;}if(p->map){wgpuBufferDestroy(p->map);wgpuBufferRelease(p->map);p->map=NULL;}}
 g->query_gpu_bytes=0;g->query_current=g->query_active_count=0;
}
