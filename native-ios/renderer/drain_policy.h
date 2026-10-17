#ifndef NATIVE_DRAIN_POLICY_H
#define NATIVE_DRAIN_POLICY_H
#include <stdbool.h>
#include <stdint.h>
/* Read already-published work only. A drain budget is 256 commands. Never
   retain across producer idle or beyond four drains / a 4 ms check window.
   This is not a deadline for individual shader/driver API calls. */
static inline bool native_drain_keep(unsigned drains,uint64_t elapsed,bool full,bool ready){
 return drains>0&&drains<4&&elapsed<UINT64_C(4000000)&&full&&ready;
}
#endif
