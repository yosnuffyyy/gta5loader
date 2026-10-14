#ifndef NATIVE_QUERY_STATE_H
#define NATIVE_QUERY_STATE_H
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
/* GPU-thread-owned logical scope. Tokens distinguish reuse and object-ID ABA.
   Results only become publishable after End and every actual GPU segment. */
typedef struct {uint64_t token,sum;uint32_t pending,sequence,type;bool active,ended,published;} QueryState;
static bool query_begin_state(QueryState *q,uint64_t token){
 if(!q||q->active||!token||(q->type!=1&&q->type!=5))return false;
 *q=(QueryState){.type=q->type,.token=token,.active=true};return true;
}
static bool query_reserve_state(QueryState *q){if(!q||!q->active||q->pending==UINT32_MAX)return false;q->pending++;return true;}
static bool query_end_state(QueryState *q,uint32_t sequence){if(!q||!q->active)return false;q->active=false;q->ended=true;q->sequence=sequence;return true;}
/* 0: stale token (discard), 1: accepted, -1: impossible completion/overflow. */
static int query_accept_state(QueryState *q,uint64_t token,uint64_t samples){
 if(!q||q->token!=token)return 0;if(!q->pending||samples>UINT64_MAX-q->sum)return -1;
 q->pending--;q->sum+=samples;return 1;
}
static bool query_ready_state(const QueryState *q){return q&&q->ended&&!q->pending&&!q->published;}
static uint64_t query_value_state(const QueryState *q){return q->type==5?(q->sum!=0):q->sum;}
#endif
