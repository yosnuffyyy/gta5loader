/* Renderer V2's first vertical slice: share identical queue fences, with
   bounded logical fences and real completion at their covering submission. */
static void completion_report(NativeGpu *g){
 if(!g->host->progress)return;
 char line[700];snprintf(line,sizeof(line),"Renderer execution: %s; END types0/1/2/3/5 %llu/%llu/%llu/%llu/%llu; completion registrations/joined/already-complete %llu/%llu/%llu; pending-write fence submissions %llu; completion state %zu bytes. Bounded deferred fences (32 events / 1 ms between commands, idle always flushes); timestamp replies remain CPU compatibility, not GPU timing.",g->completion?"V2 experimental":"Existing",(unsigned long long)g->query_types[0],(unsigned long long)g->query_types[1],(unsigned long long)g->query_types[2],(unsigned long long)g->query_types[3],(unsigned long long)g->query_types[5],(unsigned long long)g->completion_registers,(unsigned long long)g->completion_joins,(unsigned long long)g->completion_ready,(unsigned long long)g->completion_write_submits,g->completion?sizeof(*g->completion):0);g->host->progress(g->host->progress_context,line);
 timing_text(&g->completion_latency,"Event END to reply CPU wall (includes fence preparation; not GPU execution)",line,sizeof(line));g->host->progress(g->host->progress_context,line);
}
static void completion_done(WGPUQueueWorkDoneStatus status,WGPUStringView message,void *context,void *unused){
 (void)unused;CompletionContext *ctx=context;NativeGpu *g=ctx->owner;CompletionSerial *s=g->completion;
 if(!check(g,s&&ctx->batch<COMPLETION_CAPACITY&&s->batches[ctx->batch].busy,"Completion callback ownership invalid"))return;
 CompletionBatch *batch=s->batches+ctx->batch;
 for(unsigned at=batch->head;at<COMPLETION_CAPACITY;at=s->replies[at].next){
  CompletionReply *ticket=s->replies+at;
  EventReply reply={.owner=g,.id=ticket->id,.sequence=ticket->sequence,.generation=ticket->generation,.token=ticket->token,.value=ticket->value};
  event_publish(status,message,&reply);
  timing_add(&g->completion_latency,now_ns()-ticket->started);
 }
 bool success=status==WGPUQueueWorkDoneStatus_Success&&!atomic_load(&g->stop)&&!atomic_load(&g->closing)&&!atomic_load(&g->failed);
 completion_finish(s,ctx->batch,success);g->event_pending=s->pending+s->deferred_count;
}
static bool completion_flush_writes(NativeGpu *g){
 if(!g->completion||!g->queue_writes_pending)return true;
 g->completion_write_submits++;return submit(g,encoder(g));
}
/* Attach only after the encoder covering the logical fence is submitted.
   Completion may include later commands in the same bounded batch, never less
   than the preceding work. END's CPU compatibility timestamp is captured once. */
static bool completion_attach(NativeGpu *g,CompletionReply ticket){
 unsigned slot;CompletionAction action=completion_add(g->completion,ticket,&slot);
 if(!check(g,action!=COMPLETION_FULL,"Completion combined budget invariant"))return false;
 g->event_pending=g->completion->pending+g->completion->deferred_count;
 if(action==COMPLETION_READY){EventReply reply={.owner=g,.id=ticket.id,.sequence=ticket.sequence,.generation=ticket.generation,.token=ticket.token,.value=ticket.value};WGPUStringView message=WGPU_STRING_VIEW_INIT;event_publish(WGPUQueueWorkDoneStatus_Success,message,&reply);g->completion_ready++;timing_add(&g->completion_latency,now_ns()-ticket.started);}
 else if(action==COMPLETION_JOIN)g->completion_joins++;
 else {g->completion_registers++;CompletionContext *ctx=g->completion_contexts+slot;ctx->owner=g;ctx->batch=slot;
  WGPUQueueWorkDoneCallbackInfo cb=WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;cb.mode=WGPUCallbackMode_AllowProcessEvents;cb.callback=completion_done;cb.userdata1=ctx;wgpuQueueOnSubmittedWorkDone(g->queue,cb);
 }
 return !atomic_load(&g->failed);
}
static bool completion_submitted(NativeGpu *g){
 if(!g->completion)return true;
 CompletionSerial *s=g->completion;unsigned count=s->deferred_count;s->deferred_count=0;
 for(unsigned i=0;i<count;i++)if(!completion_attach(g,s->deferred[i]))return false;
 return !atomic_load(&g->failed);
}
static bool completion_progress(NativeGpu *g){
 if(!g->completion||!g->completion->deferred_count)return draw_flush(g);
 if(!draw_flush(g)||!completion_flush_writes(g))return false;
 return completion_submitted(g);
}
static bool completion_event_end(NativeGpu *g,Object *q,uint32_t sequence,uint64_t value){
 uint64_t started=now_ns(),deadline=started+15000000000ull;bool pressure=false;
 if(!check(g,g->event_token<UINT64_MAX,"Event query token exhausted"))return false;
 while(g->completion->pending+g->completion->deferred_count>=COMPLETION_CAPACITY){
  if(!pressure){g->event_pressure++;pressure=true;}if(!completion_progress(g))return false;wgpuDevicePoll(g->device,false,NULL);
  if(atomic_load(&g->stop)||atomic_load(&g->failed))return false;
  if(now_ns()>deadline)return check(g,false,"Bounded V2 completion pool timeout");
  if(g->completion->pending==COMPLETION_CAPACITY){struct timespec delay={0,1000000};nanosleep(&delay,NULL);}
 }
 CompletionReply ticket={.id=q->id,.sequence=sequence,.generation=q->generation,.token=++g->event_token,.value=value,.started=started};
 q->event_token=ticket.token;g->event_requests++;
 if(g->draw.encoder||g->queue_writes_pending||g->completion->deferred_count){
  if(!check(g,completion_defer(g->completion,ticket),"Deferred completion budget invariant"))return false;
  g->event_pending=g->completion->pending+g->completion->deferred_count;
  if(g->event_pending>g->event_peak)g->event_peak=g->event_pending;
  return !completion_due(g->completion,started)||completion_progress(g);
 }
 bool ok=completion_attach(g,ticket);if(g->event_pending>g->event_peak)g->event_peak=g->event_pending;return ok;
}
