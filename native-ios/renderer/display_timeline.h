#ifndef NATIVE_DISPLAY_TIMELINE_H
#define NATIVE_DISPLAY_TIMELINE_H
#include <math.h>
#include "timing_window.h"
typedef struct {TimingWindow intervals,cold;uint64_t displayed,dropped,out_of_order,consecutive,longest;double last;} DisplayTimeline;
static inline void display_observe(DisplayTimeline *d,double time){
 if(!isfinite(time)||time<=0){d->dropped++;d->consecutive=0;return;}
 if(d->last&&(time<=d->last||(time-d->last)>=UINT64_MAX/1e9)){d->out_of_order++;d->consecutive=0;return;}
 if(d->last){uint64_t ns=(uint64_t)((time-d->last)*1e9);timing_add(&d->intervals,ns);if(d->cold.count<64)timing_add(&d->cold,ns);}
 d->last=time;d->displayed++;d->consecutive++;if(d->consecutive>d->longest)d->longest=d->consecutive;
}
#endif
