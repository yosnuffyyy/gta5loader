#ifndef NATIVE_GPU_METRICS_H
#define NATIVE_GPU_METRICS_H
#include <stdbool.h>
#include "timing_window.h"
#include "frame_costs.h"
enum { METRIC_FRAME, METRIC_CPU, METRIC_BUSY, METRIC_SHADER, METRIC_PIPELINE,
 METRIC_READBACK, METRIC_COMPLETION, METRIC_UPLOAD, METRIC_ALLOC, METRIC_LINK, METRIC_SHADOW, METRIC_COUNT };
typedef struct {uint64_t elapsed,frame,vs,ps,key;bool recreated;} PipelineCost;
typedef struct {PipelineCost pipelines[8];unsigned pipeline_cost_count;TimingWindow windows[METRIC_COUNT],cold_frames,post_compute_frames;uint64_t last_present,last_cpu,frame_busy,frame_upload,frame_alloc,readback_logs,callback_sleeps_avoided,pipeline_evictions;FrameCost previous_cost,costs[FRAME_COST_RECORDS];SlowCommand slow[FRAME_COST_RECORDS];unsigned cost_count,slow_count;uint64_t command_flush_ns,idle_wait_ns,drain_flush_ns,upload_bytes;uint64_t shadow_creates,shadow_hits,shadow_evictions,shadow_logs,shadow_decisions,shadow_eviction_flushes;bool compute_seen;} NativeMetrics;
#endif
