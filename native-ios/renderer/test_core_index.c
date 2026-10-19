#include <stdlib.h>
static unsigned fail_next;
static void *page_allocate(size_t n,size_t size){if(fail_next){fail_next--;return NULL;}return calloc(n,size);}
#define CORE_INDEX_ALLOC(n,size) page_allocate(n,size)
#include "core_index.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 CoreIndex *index=calloc(1,sizeof(*index));assert(index);uintptr_t owners[2048];
 for(unsigned round=0;round<8;round++){
  for(unsigned i=1;i<2048;i++){owners[i]=i+round;core_index_put(index,i,&owners[i]);}
  for(unsigned i=1;i<2048;i++){void *v=NULL;assert(core_index_get(index,i,&v)&&v==&owners[i]);}
  for(unsigned i=1;i<2048;i++){void *v=(void*)1;core_index_put(index,i,NULL);assert(core_index_get(index,i,&v)&&v==NULL);}
 }
 void *v=NULL;assert(!core_index_get(index,0,&v));assert(!core_index_get(index,UINT32_MAX,&v));
 for(unsigned page=0;page<CORE_MAX_PAGES;page++)core_index_put(index,page*256+1,&owners[1]);
 assert(index->count==CORE_MAX_PAGES);core_index_put(index,CORE_MAX_PAGES*256+1,&owners[1]);assert(!core_index_get(index,CORE_MAX_PAGES*256+1,&v));
 /* Budget exhaustion never changes existing entries or allocates on removal. */
 core_index_put(index,UINT32_MAX,&owners[1]);assert(index->count==CORE_MAX_PAGES);
 core_index_clear(index);assert(index->count==0);assert(!core_index_get(index,1,&v));
 fail_next=1;core_index_put(index,1,&owners[1]);assert(!core_index_get(index,1,&v));
 core_index_put(index,2,&owners[2]);assert(!core_index_get(index,2,&v));assert(index->count==0);
 /* Allocation recovery on a different page is safe; the failed page remains
    dictionary-only, rather than incorrectly declaring the first ID absent. */
 core_index_put(index,257,&owners[1]);assert(core_index_get(index,257,&v)&&v==&owners[1]);core_index_clear(index);
 core_index_put(index,1,&owners[2]);assert(core_index_get(index,1,&v)&&v==&owners[2]);core_index_clear(index);free(index);
 puts("PASS: CPU core paged identity index: exact IDs, absent entries, repeated reuse, budget, sparse/high IDs, allocation failure/recovery and cleanup.");
}
