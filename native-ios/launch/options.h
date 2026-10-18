#ifndef NATIVE_GAME_LAUNCH_OPTIONS_H
#define NATIVE_GAME_LAUNCH_OPTIONS_H
#include <stdbool.h>
#include <stdint.h>
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
#endif
