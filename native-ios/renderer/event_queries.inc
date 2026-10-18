/* Event/timestamp compatibility replies use the existing protocol values,
   but publish only after actual queue completion. They are not GPU timers.
   Fixed callback storage belongs to NativeGpu until instance destruction. */
static void event_done(WGPUQueueWorkDoneStatus status,WGPUStringView message,void *context,void *unused){
 (void)message;(void)unused;EventReply *r=context;NativeGpu *g=r->owner;
 bool cancel=atomic_load(&g->stop)||atomic_load(&g->closing)||atomic_load(&g->failed);
 if(!cancel&&status!=WGPUQueueWorkDoneStatus_Success)fail(g,"Event query GPU completion failed");
 if(!cancel&&status==WGPUQueueWorkDoneStatus_Success){Object *q=object(g,r->id);
  if(q&&q->kind==44&&q->generation==r->generation&&q->event_token==r->token){
   uint64_t address=q->meta[2]|((uint64_t)q->meta[3]<<32);
   if(check(g,address%4==0&&host_range(g->host,address,16),"Event query result address invalid")){
    atomic_store_explicit(heap_word(g,address+8),(uint32_t)r->value,memory_order_relaxed);
    atomic_store_explicit(heap_word(g,address+12),(uint32_t)(r->value>>32),memory_order_relaxed);
    atomic_store_explicit(heap_word(g,address),r->sequence,memory_order_release);wake(g,address);g->event_results++;
   }
  }else g->event_stale++;
 }
 r->busy=false;if(g->event_pending)g->event_pending--;
}
static bool event_end(NativeGpu *g,Object *q,uint32_t sequence,uint64_t value){
 if(!check(g,g->event_token<UINT64_MAX,"Event query token exhausted"))return false;
 uint64_t deadline=now_ns()+15000000000ull;bool pressure=false;EventReply *r=NULL;
 while(!r){for(unsigned i=0;i<EVENT_REPLIES;i++)if(!g->events[i].busy){r=g->events+i;break;}
  if(r)break;if(!pressure){g->event_pressure++;pressure=true;}wgpuDevicePoll(g->device,false,NULL);
  if(atomic_load(&g->stop)||atomic_load(&g->failed))return false;
  if(now_ns()>deadline)return check(g,false,"Bounded event-query completion pool timeout");
  bool full=true;for(unsigned i=0;i<EVENT_REPLIES;i++)if(!g->events[i].busy){full=false;break;}
  if(full){struct timespec delay={0,1000000};nanosleep(&delay,NULL);}
 }
 /* Keep the exact prior submission boundary. No early reply, driver resource
    reuse, future-batch retirement or asynchronous engine-state mutation. */
 if(!draw_flush(g))return false;
 q->event_token=++g->event_token;
 *r=(EventReply){.owner=g,.id=q->id,.generation=q->generation,.token=q->event_token,.sequence=sequence,.value=value,.busy=true};
 g->event_pending++;g->event_requests++;
 if(g->event_pending>g->event_peak)g->event_peak=g->event_pending;
 WGPUQueueWorkDoneCallbackInfo c=WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;c.mode=WGPUCallbackMode_AllowProcessEvents;c.callback=event_done;c.userdata1=r;
 wgpuQueueOnSubmittedWorkDone(g->queue,c);return !atomic_load(&g->failed);
}
