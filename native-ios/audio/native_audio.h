#ifndef NATIVE_AUDIO_H
#define NATIVE_AUDIO_H
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool native_audio_start(_Atomic unsigned char*,uint64_t,uint64_t,uint64_t,char*,size_t);
void native_audio_stop(void);
void native_audio_suspend(bool);
void native_audio_summary(char*,size_t);
#endif
