/* Simulator SDK lacks Metal drawable presentation callbacks. This UI-only
   harness never runs a renderer or reports presentation/FPS results. Device
   builds always use the unchanged production presentation_metrics.inc. */
#include <stdlib.h>
@interface NativeDisplayMetrics : NSObject
- (void)markScene;
- (NSString *)overlay;
- (NSString *)summary;
@end
@implementation NativeDisplayMetrics
- (void)markScene {abort();}
- (NSString *)overlay {abort();}
- (NSString *)summary {abort();}
@end
@interface NativeMeasuredLayer : CAMetalLayer
@property(nonatomic,strong) NativeDisplayMetrics *displayMetrics;
@end
@implementation NativeMeasuredLayer
@end
