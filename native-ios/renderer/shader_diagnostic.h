#ifndef NATIVE_SHADER_DIAGNOSTIC_H
#define NATIVE_SHADER_DIAGNOSTIC_H
#include <stdint.h>
/* A failed binding may name a destroyed/reused object or malformed metadata.
   Diagnostic formatting must not read past that descriptor while reporting it. */
static uint32_t shader_diagnostic_word(uint32_t kind,uint32_t words,const uint32_t *meta,unsigned at){
 return kind==14&&words>=5&&meta&&at<words?meta[at]:0;
}
#endif
