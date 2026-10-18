#ifndef NATIVE_PROFILE_H
#define NATIVE_PROFILE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Optional, one service-thread owner. Frozen for each NativeGpu instance.
   No GPU timing or engine-function inference is made from these CPU clocks. */
enum { NP_OPS=48, NP_SAMPLES=256 };
typedef enum { NP_SUBMIT, NP_COMPLETION, NP_PIPELINE, NP_LINK, NP_PREFLUSH,
 NP_DRAIN_FLUSH, NP_RING_RESIDUAL, NP_FRAME_HANDLERS, NP_STAGES } NPStage;
typedef struct {uint64_t count,wall,cpu,max_wall,max_cpu,bytes,submissions,flushes;} NPOperation;
typedef struct {uint64_t wall[NP_SAMPLES],cpu[NP_SAMPLES],count,total_wall,total_cpu;unsigned used,next;} NPWindow;
typedef struct NativeProfile {NPOperation op[NP_OPS];NPWindow stage[NP_STAGES];uint64_t command_wall,command_cpu,frame_wall,frame_cpu,clock_failures,residual_underflows;} NativeProfile;
void native_profile_set_enabled(bool enabled);
bool native_profile_enabled(void);
NativeProfile *native_profile_create(void);
uint64_t native_profile_cpu(void); /* UINT64_MAX if unsupported/failure. */
void native_profile_stage(NativeProfile *,NPStage,uint64_t wall,uint64_t cpu_start);
void native_profile_operation(NativeProfile *,unsigned op,uint64_t wall,uint64_t cpu_start,uint64_t bytes,uint64_t submissions,bool flushed);
void native_profile_residual(NativeProfile *,uint64_t wall,uint64_t cpu_start,uint64_t command_wall_before,uint64_t command_cpu_before);
void native_profile_report(const NativeProfile *,void (*emit)(void *,const char *),void *context);
#endif
