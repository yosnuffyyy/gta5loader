static void shadow_trace_decision(NativeGpu *g,DrawProgram *p,ShadowGuard *q,Object *srv,Object *source,const uint32_t *bits,bool inactive){
 if(!g->host->progress)return;
 ShadowTraceEntry e={0};memcpy(e.hash,p->hash,sizeof(e.hash));e.binding=q->binding;e.srv=srv->id;e.source=source->id;e.conditions=q->count;e.inactive=inactive;e.srv_generation=srv->generation;e.source_generation=source->generation;e.source_version=source->version;e.first_command=e.last_command=g->ring.executed;e.present=g->presents;
 for(unsigned i=0;i<q->count;i++){unsigned slot=q->conditions[i].slot;Object *o=object(g,g->cb[p->stage][slot][0]);e.uniform[i]=(ShadowTraceUniform){g->cb[p->stage][slot][0],g->cb[p->stage][slot][1],bits[i],o?o->generation:0};}
 (void)shadow_trace_push(&g->shadow_trace,&e);
}
static void shadow_trace_report(NativeGpu *g,DrawProgram *p){
 if(!g->host->progress)return;char line[480];ShadowTrace *t=&g->shadow_trace;
 snprintf(line,sizeof(line),"Shadow transition history: %u retained, %llu overwritten; %zu fixed bytes. Only consecutive identical decisions coalesce; this is not a proof cache.",t->count,(unsigned long long)t->overwritten,sizeof(*t));g->host->progress(g->host->progress_context,line);
 for(unsigned i=0;i<t->count;i++){ShadowTraceEntry *e=&t->entries[(t->next+SHADOW_TRACE_CAPACITY-t->count+i)%SHADOW_TRACE_CAPACITY];
  snprintf(line,sizeof(line),"Shadow transition %u: PS %s binding %u SRV/source %u/%u generation %llu/%llu version %llu; commands %llu..%llu PRESENT %llu inactive %u repeats %llu",i,e->hash,e->binding,e->srv,e->source,(unsigned long long)e->srv_generation,(unsigned long long)e->source_generation,(unsigned long long)e->source_version,(unsigned long long)e->first_command,(unsigned long long)e->last_command,(unsigned long long)e->present,e->inactive,(unsigned long long)e->repeats);g->host->progress(g->host->progress_context,line);
  for(unsigned j=0;j<e->conditions;j++){ShadowTraceUniform *u=e->uniform+j;snprintf(line,sizeof(line),"Shadow transition %u condition %u: CB id %u generation %llu firstConstant %u raw %08x",i,j,u->id,(unsigned long long)u->generation,u->first,u->bits);g->host->progress(g->host->progress_context,line);}
 }
 /* Snapshot the packed source bytes only on failure. At most two original
    blocks (944+496 bytes); no whole-buffer copies, game-memory chasing or
    GPU synchronization. Offsets and zero-fill match draw_group exactly. */
 if(!p->shadow_plan)return;
 const unsigned slots[]={3,6},limits[]={236,188};
 for(unsigned i=0;i<2;i++){unsigned slot=slots[i];Object *o=object(g,g->cb[p->stage][slot][0]);uint64_t from=(uint64_t)g->cb[p->stage][slot][1]*16;
  snprintf(line,sizeof(line),"Shadow fatal CB%u: id %u kind %u generation %llu bytes %u firstConstant %u numConstants %u; following raw words use packed zero-fill ABI",slot,g->cb[p->stage][slot][0],o?o->kind:0,(unsigned long long)(o?o->generation:0),o?o->size:0,g->cb[p->stage][slot][1],g->cb[p->stage][slot][2]);g->host->progress(g->host->progress_context,line);
  for(unsigned row=0;row<limits[i];row+=8){uint32_t words[8]={0};uint64_t at=from+(uint64_t)row*4;size_t bytes=(limits[i]-row<8?limits[i]-row:8)*4;if(o&&o->kind==11&&o->shadow&&at<o->size){if(bytes>o->size-at)bytes=(size_t)(o->size-at);memcpy(words,o->shadow+at,bytes);}
   snprintf(line,sizeof(line),"Shadow fatal CB%u words %u: %08x %08x %08x %08x %08x %08x %08x %08x",slot,row,words[0],words[1],words[2],words[3],words[4],words[5],words[6],words[7]);g->host->progress(g->host->progress_context,line);
  }
 }
}
