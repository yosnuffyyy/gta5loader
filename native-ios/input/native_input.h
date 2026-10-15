#ifndef NATIVE_INPUT_H
#define NATIVE_INPUT_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* ioPad enums from the pinned engine, not XInput or browser button numbers. */
enum { NI_L2=1u<<0,NI_R2=1u<<1,NI_L1=1u<<2,NI_R1=1u<<3,
 NI_TRIANGLE=1u<<4,NI_CIRCLE=1u<<5,NI_CROSS=1u<<6,NI_SQUARE=1u<<7,
 NI_SELECT=1u<<8,NI_L3=1u<<9,NI_R3=1u<<10,NI_START=1u<<11,
 NI_UP=1u<<12,NI_RIGHT=1u<<13,NI_DOWN=1u<<14,NI_LEFT=1u<<15 };
typedef enum { NI_TOUCH=0, NI_CONTROLLER=1 } NISource;
typedef struct {uint32_t buttons;float lx,ly,rx,ry,lt,rt;} NIState;
typedef struct {double mean_ms,max_ms;uint64_t events,consumed,switches,overflows,samples;unsigned queued;bool running,focused;NISource source;} NIMetrics;
void ni_begin(void);
void ni_end(void);
void ni_focus(bool focused);
void ni_connect(bool connected);
void ni_configure(float deadzone,float camera_sensitivity);
/* Input producers are serialized by a mutex. No producer accesses engine RAM. */
NISource ni_controller(NIState state);
NISource ni_touch(NIState state,bool deliberate_begin);
NISource ni_source(void);
void ni_metrics(NIMetrics *out);
/* Called only by the engine's existing synchronous ioPad hook. */
void ni_consume(uint8_t pad[56]);
bool ni_is_control(uint64_t pointer);
void ni_control(uint8_t source_field[4]);
#endif
