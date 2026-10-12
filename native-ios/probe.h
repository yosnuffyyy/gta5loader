#ifndef GTA_PROBE_H
#define GTA_PROBE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int native_engine_probe(char *report,size_t capacity);
typedef void (*native_progress)(void *context,const char *message);
int native_runtime_probe(const char *game_folder,const char *save_folder,
                         native_progress progress,void *context,char *report,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
