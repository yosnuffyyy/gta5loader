#include "device_settings.h"
#include <assert.h>
int main(void){@autoreleasepool{
 id<MTLDevice> device=MTLCreateSystemDefaultDevice();if(!device){fprintf(stderr,"UNAVAILABLE: no Metal device; launch target validation not run.\n");return 2;}
 unsigned checks=0;for(unsigned base=0;base<3;base++)for(unsigned scale=50;scale<=100;scale+=10){@autoreleasepool{NativeSettings s=native_settings_defaults();s.base=base;s.scale=scale;NSString *detail=nil;assert(!native_settings_device_check(s,device,UINT64_MAX,&detail));assert(detail.length>0);checks++;}}
 NativeSettings s=native_settings_defaults();assert(native_settings_device_check(s,device,0,NULL));assert(native_settings_device_check(s,device,1,NULL));assert(native_settings_device_check(s,nil,UINT64_MAX,NULL));s.base=3;assert(native_settings_device_check(s,device,UINT64_MAX,NULL));
 printf("PASS: %u scale/base allocations of real host Metal BGRA8/depth32 targets, nil device, invalid config and insufficient minimum payload headroom rejection. Representative targets only, not full game budgets or iPhone rendering.\n",checks);
}return 0;}
