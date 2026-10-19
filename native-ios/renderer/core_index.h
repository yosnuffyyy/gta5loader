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
typedef struct {void **pages[CORE_DIRECTORY];unsigned count;} CoreIndex;
static inline bool core_index_get(const CoreIndex *index,uint32_t id,void **value){
 unsigned page=id>>CORE_PAGE_BITS;
 if(!id||page>=CORE_DIRECTORY||!index->pages[page])return false;
 *value=index->pages[page][id&(CORE_PAGE_SIZE-1)];return true;
}
static inline void core_index_put(CoreIndex *index,uint32_t id,void *value){
 unsigned page=id>>CORE_PAGE_BITS;if(!id||page>=CORE_DIRECTORY)return;
 if(!index->pages[page]){
  if(!value||index->count==CORE_MAX_PAGES)return;
  void **p=calloc(CORE_PAGE_SIZE,sizeof(void*));if(!p)return;
  index->pages[page]=p;index->count++;
 }
 index->pages[page][id&(CORE_PAGE_SIZE-1)]=value;
}
static inline void core_index_clear(CoreIndex *index){
 for(unsigned i=0;i<CORE_DIRECTORY;i++){free(index->pages[i]);index->pages[i]=NULL;}index->count=0;
}
#endif
