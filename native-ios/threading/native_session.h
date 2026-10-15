#ifndef NATIVE_SESSION_H
#define NATIVE_SESSION_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
/* Policy only. Callers serialize access; tests supply the monotonic clock.
   Runtime activity is not evidence of visually correct gameplay. */
enum { SESSION_STARTUP,SESSION_LIVE,SESSION_STOPPED };
enum { SESSION_OK,SESSION_STARTUP_TIMEOUT,SESSION_ENGINE_STALL,SESSION_FRAME_STALL };
#define SESSION_SECOND UINT64_C(1000000000)
typedef struct {
 uint64_t start,pause_start,paused_ns,hooks,marker_hook,marker_frames,engine_at,frame_at,io_at;
 uint64_t commands,frames,reads,command_at,live_at;
 unsigned phase,fault;bool suspended,scene_marker;
 char script[112];
} NativeSession;
static inline uint64_t session_delta(uint64_t now,uint64_t before){return now>=before?now-before:0;}
static inline uint64_t session_time(const NativeSession *s,uint64_t now){
 uint64_t elapsed=session_delta(s->suspended?s->pause_start:now,s->start);
 return elapsed>=s->paused_ns?elapsed-s->paused_ns:0;
}
static inline void session_begin(NativeSession *s,uint64_t now){memset(s,0,sizeof(*s));s->start=now;}
static inline void session_suspend(NativeSession *s,uint64_t now,bool suspended){
 if(s->suspended==suspended||s->phase==SESSION_STOPPED)return;
 if(suspended)s->pause_start=now;else {uint64_t n=session_delta(now,s->pause_start);s->paused_ns=UINT64_MAX-s->paused_ns<n?UINT64_MAX:s->paused_ns+n;}
 s->suspended=suspended;
}
static inline void session_hook(NativeSession *s,uint64_t now){if(s->phase!=SESSION_STOPPED){s->hooks++;s->engine_at=session_time(s,now);}}
static inline void session_log(NativeSession *s,const char *line){
 if(s->phase==SESSION_STOPPED)return;
 if(!s->scene_marker&&strstr(line,"Animated camera intial frame")){s->scene_marker=true;s->marker_hook=s->hooks;s->marker_frames=s->frames;}
 if(strstr(line,"Currently running script thread")||strstr(line,">>>SCRIPT["))snprintf(s->script,sizeof(s->script),"%.111s",line);
}
static inline unsigned session_check(NativeSession *s,uint64_t now,uint64_t commands,uint64_t frames,uint64_t reads){
 if(s->fault||s->phase==SESSION_STOPPED)return s->fault;
 uint64_t at=session_time(s,now);
 if(commands>s->commands){s->commands=commands;s->command_at=at;}
 if(frames>s->frames){s->frames=frames;s->frame_at=at;}
 if(reads>s->reads){s->reads=reads;s->io_at=at;}
 if(s->suspended)return SESSION_OK;
 /* Camera initialization is an actual engine scene event. A subsequent pad
    simulation hook and presentation are both required; draw counts alone
    cannot retire the startup deadline. This is liveness, not visual proof. */
 if(s->phase==SESSION_STARTUP&&s->scene_marker&&s->hooks>s->marker_hook&&s->frames>s->marker_frames){s->phase=SESSION_LIVE;s->live_at=at;}
 if(s->phase==SESSION_STARTUP){if(at>=180*SESSION_SECOND)s->fault=SESSION_STARTUP_TIMEOUT;}
 else if(session_delta(at,s->engine_at)>=60*SESSION_SECOND&&session_delta(at,s->io_at)>=60*SESSION_SECOND)s->fault=SESSION_ENGINE_STALL;
 else if(session_delta(at,s->frame_at)>=60*SESSION_SECOND&&session_delta(at,s->io_at)>=60*SESSION_SECOND)s->fault=SESSION_FRAME_STALL;
 return s->fault;
}
static inline void session_stop(NativeSession *s){s->phase=SESSION_STOPPED;}
static inline const char *session_failure(unsigned fault){
 switch(fault){case SESSION_STARTUP_TIMEOUT:return "native scene startup made no verified live-session transition within 180 active seconds";
 case SESSION_ENGINE_STALL:return "native live session stalled: no engine input-update hook or completed asset request for 60 active seconds";
 case SESSION_FRAME_STALL:return "native live session stalled: no successful PRESENT or completed asset request for 60 active seconds";
 default:return NULL;}
}
static inline void session_summary(const NativeSession *s,uint64_t now,char *out,size_t size){
 uint64_t at=session_time(s,now);
 snprintf(out,size,"Session %s%s: active %.1fs; engine/frame/IO age %.1f/%.1f/%.1fs; hooks %llu; last script %.111s; liveness is not visual verification",s->phase==SESSION_LIVE?"live":s->phase==SESSION_STOPPED?"stopped":"startup",s->suspended?" suspended":"",at/1e9,session_delta(at,s->engine_at)/1e9,session_delta(at,s->frame_at)/1e9,session_delta(at,s->io_at)/1e9,(unsigned long long)s->hooks,s->script[0]?s->script:"not yet observed");
}
#endif
