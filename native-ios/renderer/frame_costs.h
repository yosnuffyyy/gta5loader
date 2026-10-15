#ifndef NATIVE_FRAME_COSTS_H
#define NATIVE_FRAME_COSTS_H
/* All records are GPU-service-thread owned, fixed-size and CPU-side only. */
typedef struct {uint64_t frame,end,interval,busy,upload,allocation,idle,drain,commands,draws,creates,hits,evictions,bytes;double shader,link,pipeline,map,completion,shadow;bool post_compute;} FrameCost;
typedef struct {uint64_t frame,at,elapsed,flush,shader;uint32_t opcode,resource,bytes;} SlowCommand;
enum { FRAME_COST_RECORDS=8 };
#endif
