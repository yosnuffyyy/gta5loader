#ifndef NATIVE_CORE_INDEX_H
#define NATIVE_CORE_INDEX_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
/* Service-thread-owned acceleration index, never an owner of an Object.
   A present page is authoritative even for NULL entries. All insert/remove
   operations update it before objects can be observed or freed. Missing pages,
   large IDs, budget/allocation failures use the original dictionary. */
enum { CORE_PAGE_BITS=8, CORE_PAGE_SIZE=256, CORE_DIRECTORY=4096, CORE_MAX_PAGES=512 };
typedef struct {void **pages[CORE_DIRECTORY];uint64_t fallback[CORE_DIRECTORY/64];unsigned count;} CoreIndex;
#ifndef CORE_INDEX_ALLOC
#define CORE_INDEX_ALLOC(n,size) calloc(n,size)
#endif
static inline bool core_index_get(const CoreIndex *index,uint32_t id,void **value){
 unsigned page=id>>CORE_PAGE_BITS;
 if(!id||page>=CORE_DIRECTORY||!index->pages[page])return false;
 *value=index->pages[page][id&(CORE_PAGE_SIZE-1)];return true;
}
static inline void core_index_put(CoreIndex *index,uint32_t id,void *value){
 unsigned page=id>>CORE_PAGE_BITS;if(!id||page>=CORE_DIRECTORY)return;
 if(!index->pages[page]){
  if(!value||(index->fallback[page/64]&(UINT64_C(1)<<(page%64))))return;
  if(index->count==CORE_MAX_PAGES){index->fallback[page/64]|=UINT64_C(1)<<(page%64);return;}
  void **p=CORE_INDEX_ALLOC(CORE_PAGE_SIZE,sizeof(void*));
  /* Once an ID on this page has escaped indexing, never publish a partial
     authoritative page on a later allocation retry. Keep all its IDs on the
     canonical dictionary until clear; this includes allocation recovery. */
  if(!p){index->fallback[page/64]|=UINT64_C(1)<<(page%64);return;}
  index->pages[page]=p;index->count++;
 }
 index->pages[page][id&(CORE_PAGE_SIZE-1)]=value;
}
static inline void core_index_clear(CoreIndex *index){
 for(unsigned i=0;i<CORE_DIRECTORY;i++){free(index->pages[i]);index->pages[i]=NULL;}for(unsigned i=0;i<CORE_DIRECTORY/64;i++)index->fallback[i]=0;index->count=0;
}
#endif
