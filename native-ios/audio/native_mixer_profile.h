#ifndef NATIVE_MIXER_PROFILE_H
#define NATIVE_MIXER_PROFILE_H
/* Instrumentation only. Called on the original mixer thread, outside Core Audio. */
void native_audio_mix_begin(void);
void native_audio_mix_end(void);
#endif
