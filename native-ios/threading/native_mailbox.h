#ifndef NATIVE_MAILBOX_PENDING_H
#define NATIVE_MAILBOX_PENDING_H
#include <stdatomic.h>
#include <stdbool.h>
/* One destination thread consumes; arbitrary sender threads publish. A false
   acquire load cannot clear a concurrent notification. Recheck before sleeping
   through the existing native waiter protocol; never change wake ownership. */
static inline bool native_mailbox_claim(_Atomic bool *pending,bool core_v2){
 if(core_v2&&!atomic_load_explicit(pending,memory_order_acquire))return false;
 return atomic_exchange_explicit(pending,false,memory_order_acquire);
}
#endif
