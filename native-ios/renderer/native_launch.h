#ifndef NATIVE_LAUNCH_H
#define NATIVE_LAUNCH_H
#include <stddef.h>
#include "../probe.h"
int native_scene_probe(const char*,const char*,void*,native_progress,void*,char*,size_t);
int native_gpu_replay_probe(char*,size_t);
#ifdef NATIVE_PROBE_CLI
int native_gpu_shader_audit(const char*,unsigned,unsigned);
#endif
int native_graphics_checkpoint(const char*,const char*,native_progress,void*,char*,size_t);
void native_scene_request_stop(void);
void native_scene_reset_stop(void);
void native_scene_status(char*,size_t);
#endif
