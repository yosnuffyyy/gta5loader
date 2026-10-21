#ifndef NATIVE_DEVICE_SETTINGS_H
#define NATIVE_DEVICE_SETTINGS_H
#import <Metal/Metal.h>
#include "settings.h"
/* Startup-only, representative attachment allocation on the selected device.
   It does not estimate all streamed textures or guarantee session headroom. */
static NSString *native_settings_device_check(NativeSettings s,id<MTLDevice> device,uint64_t headroom,NSString **detail){
 unsigned w,h;if(!native_settings_size(&s,&w,&h))return @"Invalid render configuration.";
 if(!device||![device supportsTextureSampleCount:1])return @"The selected Metal device cannot create the required single-sample target.";
 uint64_t minimum=(uint64_t)w*h*8; // BGRA8 plus Depth32; excludes every other engine allocation.
 if(headroom<minimum)return @"Available app memory is below even the selected color/depth target payload. Choose a lower base/scale or restart the app.";
 NSString *summary=nil;
 @autoreleasepool{
  MTLTextureDescriptor *d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:w height:h mipmapped:NO];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
  id<MTLTexture> color=[device newTextureWithDescriptor:d];d.pixelFormat=MTLPixelFormatDepth32Float;id<MTLTexture> depth=[device newTextureWithDescriptor:d];
  if(!color||!depth)return @"Metal could not allocate the selected representative render targets. The game was not launched.";
  if(detail)summary=[NSString stringWithFormat:@"Device preflight: %u × %u single-sample BGRA8/depth32 allocated on %@ (%llu bytes). Released before engine launch. This checks target support, not the full scene/texture memory budget. OS headroom %llu bytes is a changing snapshot.",w,h,device.name,(unsigned long long)(color.allocatedSize+depth.allocatedSize),(unsigned long long)headroom];
 }
 /* Write the autoreleasing out parameter after the nested pool is drained. */
 if(detail)*detail=summary;
 return nil;
}
#endif
