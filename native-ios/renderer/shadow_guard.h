#ifndef NATIVE_SHADOW_GUARD_H
#define NATIVE_SHADOW_GUARD_H
#include <stdbool.h>
#include <stdint.h>
enum { SHADOW_GUARDS=8, SHADOW_VARIANTS=4 };
typedef struct {uint32_t slot,word;float equals;bool true_arm;} ShadowCondition;
typedef struct {uint32_t binding,decl_start,decl_length,call_start,call_length,count;ShadowCondition conditions[2];} ShadowGuard;
typedef struct {uint32_t count;ShadowGuard guards[SHADOW_GUARDS];} ShadowPlan;
/* A true arm tests !(value == literal); IEEE NaN, signed zero and infinity
   follow the original f32 comparison. Never infer an inactive path from a
   missing resource, its dimensions, or a nonfinite uniform alone. */
static inline bool shadow_condition_runs(ShadowCondition c,float value){return c.true_arm?(value!=c.equals):(value==c.equals);}
#endif
