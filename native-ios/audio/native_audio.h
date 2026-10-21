#ifndef NATIVE_AUDIO_H
#define NATIVE_AUDIO_H
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool native_audio_start(volatile _Atomic unsigned char*,uint64_t,uint64_t,uint64_t,char*,size_t);
bool native_audio_set_volume(float volume,bool muted);
void native_audio_stop(void);
void native_audio_suspend(bool);
void native_audio_summary(char*,size_t);
#endif
