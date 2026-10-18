#ifndef NATIVE_TRAP_TRACE_H
#define NATIVE_TRAP_TRACE_H
#include <stddef.h>
#include <stdbool.h>
void native_trap_reset(void);
void native_trap_cancel(void);
bool native_trap_was_cancelled(void);
void native_trap_text(char*,size_t);
#endif
