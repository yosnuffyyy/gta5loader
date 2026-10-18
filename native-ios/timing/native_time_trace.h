#ifndef NATIVE_TIME_TRACE_H
#define NATIVE_TIME_TRACE_H
/* Called only from the actual engine owner pad hook, not the UI or GPU thread.
   Original pinned fwTimer addresses, sampled once per 5s; never mutate clocks. */
typedef struct {uint64_t at;uint32_t game,unwarped;unsigned hooks;} NativeTimeTrace;
static void native_time_observe(struct w2c_env *h,NativeTimeTrace *t,uint64_t now){
 t->hooks++;if(t->at&&now-t->at<5000000000ull)return;
 uint32_t game=0,unwarped=0;float dt=0,min=0,max=0,forced=0;unsigned char pauses[4]={0};
 host_load(h,&game,12321248,4);host_load(h,&unwarped,12321416,4);host_load(h,&dt,12321260,4);host_load(h,&min,6632100,4);host_load(h,&max,6632104,4);host_load(h,&forced,12321456,4);host_load(h,pauses,12321460,4);
 if(t->at){char line[512];snprintf(line,sizeof(line),"Engine timer at pad boundary: wall %.3fs, game/unwarped %u/%u ms, hooks %u, last dt %.6fs, allowed %.6f..%.6fs, forced %.6fs, pause bytes %u/%u/%u/%u. Not animation clip time, physics CPU or GPU duration.",(now-t->at)/1e9,game-t->game,unwarped-t->unwarped,t->hooks,dt,min,max,forced,pauses[0],pauses[1],pauses[2],pauses[3]);host_log(h,line);}
 t->at=now;t->game=game;t->unwarped=unwarped;t->hooks=0;
}
#endif
