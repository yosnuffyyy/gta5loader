/* scene_status_mutex protects runtime ownership; watch_mutex protects policy.
   No engine calls, GPU waits or UI callbacks while either policy lock is held. */
static void scene_watch_hook(NativeThreads *r){if(!r->watch_enabled)return;checked(pthread_mutex_lock(&r->watch_mutex));session_hook(&r->session,now_ns());checked(pthread_mutex_unlock(&r->watch_mutex));}
static bool scene_watch_poll(NativeThreads *r){
 if(!r->watch_enabled||atomic_load(&scene_stop)||atomic_load(&r->stopping))return false;
 uint64_t now=now_ns(),next=atomic_load_explicit(&r->watch_check_at,memory_order_relaxed);
 if(now<next||!atomic_compare_exchange_strong_explicit(&r->watch_check_at,&next,now+100000000, memory_order_relaxed,memory_order_relaxed))return false;
 checked(pthread_mutex_lock(&r->watch_mutex));unsigned before=r->session.phase;
 /* Cancellation can arrive while this poll waits for the policy lock. */
 if(atomic_load(&scene_stop)||atomic_load(&r->stopping)){checked(pthread_mutex_unlock(&r->watch_mutex));return false;}
 uint64_t commands=0,frames=0;native_gpu_progress(r->gpu,&commands,&frames);
 unsigned fault=session_check(&r->session,now_ns(),commands,frames,native_assets_progress(r->assets));
 bool first=fault&&!r->watch_reported;if(first)r->watch_reported=true;
 bool transitioned=before==SESSION_STARTUP&&r->session.phase==SESSION_LIVE;char text[512];
 if(first||transitioned)session_summary(&r->session,now_ns(),text,sizeof(text));
 checked(pthread_mutex_unlock(&r->watch_mutex));
 if(fault)atomic_store(&r->stopping,true);
 if(transitioned)native_log_push(&r->log,"Session transition: camera scene event + subsequent engine pad hook + successful PRESENT; startup-only deadline retired. Visual correctness and mission progression remain separate.");
 if(first){char line[768];snprintf(line,sizeof(line),"Error: session watchdog: %s. %s",session_failure(fault),text);native_log_push(&r->log,line);native_gpu_snapshot(r->gpu,line,sizeof(line));native_log_push(&r->log,line);snprintf(line,sizeof(line),"Watchdog thread context: %s; phase %s; engine workers %u; proxy completions %u; owner mailbox %u; stop %u",worker?"engine worker":"engine owner/UI",worker&&worker->phase?worker->phase:"owner/runtime monitor",atomic_load(&r->game_started),atomic_load(&r->proxies),(unsigned)atomic_load(&r->mailbox_pending),(unsigned)atomic_load(&r->stopping));native_log_push(&r->log,line);}
 return fault!=SESSION_OK;
}
static void scene_watch_summary(NativeThreads *r,char *text,size_t size){
 if(!r->watch_enabled){snprintf(text,size,"Diagnostic startup deadline remains active.");return;}
 checked(pthread_mutex_lock(&r->watch_mutex));session_summary(&r->session,now_ns(),text,size);checked(pthread_mutex_unlock(&r->watch_mutex));
}
void native_scene_suspend(bool suspended){
 atomic_store(&scene_suspended,suspended);checked(pthread_mutex_lock(&scene_status_mutex));NativeThreads *r=scene_runtime;
 if(r&&r->watch_enabled){checked(pthread_mutex_lock(&r->watch_mutex));session_suspend(&r->session,now_ns(),suspended);checked(pthread_mutex_unlock(&r->watch_mutex));}
 checked(pthread_mutex_unlock(&scene_status_mutex));
}
