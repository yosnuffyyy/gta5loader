#ifndef NATIVE_GAME_LAUNCH_OPTIONS_H
#define NATIVE_GAME_LAUNCH_OPTIONS_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdatomic.h>
typedef struct {unsigned mode,quality,width,height;} NativeLaunchOptions;
/* Original engine startMode: 1 Story; 2 offline GTA V-map sandbox.
   No new engine flags, mid-session resizing or unverified population writes. */
static inline bool native_launch_options(unsigned mode,unsigned quality,NativeLaunchOptions *out){
 if(!out||(mode!=1&&mode!=2)||quality>2)return false;
 const unsigned widths[]={640,480,320},heights[]={360,270,180};
 *out=(NativeLaunchOptions){mode,quality,widths[quality],heights[quality]};return true;
}
static inline const char *native_launch_saves(unsigned mode){return mode==2?"NativeSandboxProbe":"NativeRuntimeProbe";}
static inline const char *native_quality_name(unsigned quality){return quality==2?"Ultra Potato":quality==1?"Potato":"Low";}
/* Pinned WasmInputBlock, relative to the pointer published by original main.
   +280 is mouse focus, NOT startMode. Publish mode last with release ordering. */
enum {NATIVE_LAUNCH_BLOCK_BYTES=444,NATIVE_LAUNCH_FOCUS=280,NATIVE_LAUNCH_MODE=440};
static inline bool native_launch_publish(volatile void *block,size_t bytes,NativeLaunchOptions options){
 NativeLaunchOptions checked;
 if(!block||bytes<NATIVE_LAUNCH_BLOCK_BYTES||(uintptr_t)block%4||!native_launch_options(options.mode,options.quality,&checked))return false;
 volatile _Atomic uint8_t *p=block;
 const uint32_t offsets[]={NATIVE_LAUNCH_FOCUS,424,432,436},values[]={1,UINT32_MAX,checked.width,checked.height};
 /* Match host_store's atomic byte accesses for ordinary shared fields. */
 for(unsigned i=0;i<4;i++)for(unsigned j=0;j<4;j++)atomic_store_explicit(p+offsets[i]+j,(uint8_t)(values[i]>>(j*8)),memory_order_relaxed);
 atomic_store_explicit((volatile _Atomic uint32_t*)(p+NATIVE_LAUNCH_MODE),checked.mode,memory_order_release);return true;
}
/* Preserve the established new-Story policy; Sandbox follows the original
   reference startup without the Story-only new-game/autoload override. */
static inline bool native_launch_noautoload(unsigned mode){return mode==1;}
/* Existing reference launcher's city test spawn, consumed by the original
   playercoords sysParam/streaming path. Never set for Story. */
static inline const char *native_launch_city_spawn(unsigned mode){return mode==2?"-playercoords=94.95,-742.96,44.75":NULL;}
/* Engine-generated commit line, not an assertion of playable gameplay. */
static inline unsigned native_launch_commit(const char *line){
 /* WASI stdout retains the engine printf's newline; host_log callers may
    supply a complete line without it. Accept only that exact framing, never
    an arbitrary suffix or an embedded second message. */
 const char prefix[]="[wasm] start mode: ";if(strncmp(line,prefix,sizeof(prefix)-1))return 0;
 const char *p=line+sizeof(prefix)-1;unsigned mode=0,n=0;
 if(!strncmp(p,"story",5)){mode=1;n=5;}else if(!strncmp(p,"sandbox",7)){mode=2;n=7;}else return 0;
 p+=n;if(!*p||!strcmp(p,"\n")||!strcmp(p,"\r\n"))return mode;
 return 0;
}
static inline bool native_launch_story_script(const char *line){return strstr(line,">>>SCRIPT[Prologue1]")||strstr(line,"Currently running script thread \"Prologue1\"");}
/* Original CGameLogic::Update emits this only after a real player ped,
   inventory and usable weapon slots exist and at least one weapon is given.
   Liveness evidence only: not a collision/camera/visual gameplay assertion. */
static inline bool native_launch_player_event(const char *line){
 const char prefix[]="[wasm] spawn loadout: ";if(strncmp(line,prefix,sizeof(prefix)-1))return false;
 const char *p=line+sizeof(prefix)-1;unsigned n=0,digits=0;
 while(*p>='0'&&*p<='9'){if(++digits>4)return false;n=n*10+(unsigned)(*p++-'0');}
 const char suffix[]=" weapons restocked, 9999 rounds each:";return n>0&&!strncmp(p,suffix,sizeof(suffix)-1);
}
#endif
