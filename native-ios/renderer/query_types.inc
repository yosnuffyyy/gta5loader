#include "query_state.h"
enum {QUERY_PAGES=4,QUERY_SEGMENTS=64,QUERY_SCOPES=32,QUERY_RESOLVE_BYTES=QUERY_SEGMENTS*8,QUERY_MAP_BYTES=QUERY_SEGMENTS*8};
typedef struct {uint32_t id;uint64_t token;} QueryRef;
typedef struct {unsigned count;QueryRef owners[QUERY_SCOPES];} QuerySegment;
typedef struct {
 NativeGpu *owner;WGPUQuerySet set;WGPUBuffer resolve,map;
 QuerySegment segments[QUERY_SEGMENTS];unsigned used;bool busy;
} QueryPage;
