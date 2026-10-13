#ifndef NATIVE_RENDERER_H
#define NATIVE_RENDERER_H
#include <stddef.h>
#include <stdint.h>
/* Small offscreen feasibility fixture; not the engine's GPU ring consumer. */
int native_renderer_probe(char *report,size_t capacity,uint8_t *rgba,size_t rgba_capacity);
#endif
