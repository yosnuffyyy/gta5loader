#include "display_timeline.h"
#include <assert.h>
int main(void){DisplayTimeline d={0};display_observe(&d,0);display_observe(&d,NAN);display_observe(&d,INFINITY);assert(d.dropped==3&&d.displayed==0&&d.intervals.count==0);
 display_observe(&d,1);assert(d.displayed==1&&d.consecutive==1&&!d.intervals.count);for(unsigned i=1;i<=512;i++)display_observe(&d,1+i/32.0);TimingSummary s=timing_summary(&d.intervals);assert(d.displayed==513&&d.consecutive==513&&d.cold.count==64&&s.n==256&&s.mean_ms==31.25&&s.p99_ms==31.25);
 display_observe(&d,0);assert(!d.consecutive&&d.dropped==4);display_observe(&d,2);assert(d.out_of_order==1&&d.displayed==513);display_observe(&d,18);assert(d.displayed==514&&d.consecutive==1&&d.longest==513&&d.intervals.worst==1000000000);
 display_observe(&d,1e30);assert(d.out_of_order==2&&d.last==18);puts("PASS: actual-display timeline, dropped/invalid/out-of-order callbacks, 32 FPS synthetic timestamps, bounded rolling windows and consecutive counters. Synthetic arithmetic test, not measured game FPS.");return 0;}
