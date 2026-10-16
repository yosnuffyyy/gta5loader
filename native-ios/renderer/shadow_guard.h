#ifndef NATIVE_SHADOW_GUARD_H
#define NATIVE_SHADOW_GUARD_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
enum { SHADOW_GUARDS=8, SHADOW_REGIONS=64, SHADOW_VARIANTS=4, SHADOW_CONDITIONS=8 };
enum { SHADOW_FLOAT_NE=0, SHADOW_INT_LE=1, SHADOW_INT_GT=2 };
typedef struct {uint32_t slot,word;float equals;bool true_arm;uint32_t predicate;} ShadowCondition;
typedef struct {uint32_t binding,decl_start,decl_length,call_start,call_length,count,sampler_slot;bool reference_compare,reference_noticed;ShadowCondition conditions[SHADOW_CONDITIONS];} ShadowGuard;
typedef struct {uint32_t count;ShadowGuard guards[SHADOW_GUARDS];} ShadowPlan;
/* A true arm tests !(value == literal); IEEE NaN, signed zero and infinity
   follow the original f32 comparison. Never infer an inactive path from a
   missing resource, its dimensions, or a nonfinite uniform alone. */
static inline bool shadow_condition_runs(ShadowCondition c,float value){return c.true_arm?(value!=c.equals):(value==c.equals);}
/* Only an exact zero/three equality can prove a true arm unreachable. Never
   prune an else arm from a nonzero/subnormal/NaN CPU value: GPU denormal and
   nonfinite rules can differ. Unsupported proof forms keep validation. */
static inline bool shadow_proven_inactive(ShadowCondition c,uint32_t bits){
 if(c.predicate==SHADOW_INT_LE||c.predicate==SHADOW_INT_GT){
  /* WGSL bitcast<i32>, not a float conversion. Restrict the proof language
     to the exact original 0..7 light-count comparisons. */
  if(!(c.equals==0||c.equals==1||c.equals==2||c.equals==3||c.equals==4||c.equals==5||c.equals==6||c.equals==7))return false;
  /* Only certify the low-count side. Counts are packed as float bits;
     1..3 are subnormal floats. A backend flush to zero cannot invalidate
     this proof, whereas a high-count/negative/NaN-bit-pattern inference
     could. Unknown/invalid counts therefore retain normal validation. */
  bool low_arm=(c.predicate==SHADOW_INT_LE&&!c.true_arm)||(c.predicate==SHADOW_INT_GT&&c.true_arm);
  return low_arm&&bits<=(uint32_t)c.equals;
 }
 if(c.predicate!=SHADOW_FLOAT_NE)return false;
 uint32_t literal;memcpy(&literal,&c.equals,4);return c.true_arm&&(((literal&0x7fffffffu)==0)?((bits&0x7fffffffu)==0):(literal==0x40400000u&&bits==literal));
}
#endif
