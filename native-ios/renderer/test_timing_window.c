#include "timing_window.h"
#include <assert.h>
#include <string.h>
int main(void){TimingWindow w={0};assert(timing_summary(&w).n==0);char text[400];timing_text(&w,"fixture",text,sizeof(text));assert(strstr(text,"no samples"));
 for(unsigned i=1;i<=256;i++)timing_add(&w,(uint64_t)i*1000000);assert(timing_recent_ms(&w,16)==248.5&&timing_recent_ms(&w,0)==0);TimingSummary s=timing_summary(&w);assert(s.n==256&&s.mean_ms==128.5&&s.median_ms==128&&s.p95_ms==244&&s.p99_ms==254&&s.worst_ms==256);
 for(unsigned i=0;i<256;i++)timing_add(&w,2000000);s=timing_summary(&w);assert(timing_recent_ms(&w,16)==2);assert(s.n==256&&s.mean_ms==2&&s.worst_ms==2&&w.worst==256000000&&w.count==512&&w.total_ns==33408000000.0);
 TimingWindow tiny={0};timing_add(&tiny,0);timing_text(&tiny,"tiny",text,sizeof(text));assert(strstr(text,"quantiles withheld"));timing_add(&tiny,UINT64_MAX);s=timing_summary(&tiny);assert(s.n==2&&s.worst_ms>1e12);timing_text(&tiny,"bounded",text,1);assert(text[0]==0);
 puts("PASS: 256-entry bounded timing windows, wrap/reuse, quantiles, startup sample limits, large values and bounded formatting.");return 0;}
