#ifndef NATIVE_GPU_METRICS_H
#define NATIVE_GPU_METRICS_H
#include <stdbool.h>
#include "timing_window.h"
enum { METRIC_FRAME, METRIC_CPU, METRIC_BUSY, METRIC_SHADER, METRIC_PIPELINE,
 METRIC_READBACK, METRIC_COMPLETION, METRIC_UPLOAD, METRIC_ALLOC, METRIC_LINK, METRIC_COUNT };
typedef struct {TimingWindow windows[METRIC_COUNT],cold_frames,post_compute_frames;uint64_t last_present,last_cpu,frame_busy,frame_upload,frame_alloc,readback_logs,callback_sleeps_avoided,pipeline_evictions;bool compute_seen;} NativeMetrics;
#endif
