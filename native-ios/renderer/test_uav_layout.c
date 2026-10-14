#include "uav_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void){uint64_t at=0,bytes=0;assert(uav_hidden_counter(1,2));assert(uav_hidden_counter(1,4));assert(!uav_hidden_counter(0,6));assert(!uav_hidden_counter(1,1));
 assert(uav_buffer_range(1024,16,3,4,&at,&bytes)&&at==48&&bytes==64);assert(uav_buffer_range(1024,16,3,UINT32_MAX,&at,&bytes)&&bytes==976);
 assert(!uav_buffer_range(1024,16,UINT32_MAX,4,&at,&bytes));assert(!uav_buffer_range(1024,16,3,UINT32_MAX-1,&at,&bytes));assert(!uav_buffer_range(1024,16,64,0,&at,&bytes));assert(!uav_buffer_range(1024,0,0,1,&at,&bytes));
 assert(uav_component_mask(8)==255&&uav_component_mask(16)==65535&&uav_component_mask(32)==UINT32_MAX);
 puts("PASS: UAV counter flags, raw/structured ranges, invalid offsets and widened arithmetic.");return 0;
}
