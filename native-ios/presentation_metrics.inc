/* The drawable owns each callback until completion; callbacks retain only
   this independent monitor, never a NativeGpu or freed engine allocation. */
#include "renderer/display_timeline.h"
@interface NativeDisplayMetrics : NSObject {
 NSLock *_lock;DisplayTimeline _timeline;
}
- (void)presented:(double)time;
- (NSString *)summary;
@end
@implementation NativeDisplayMetrics
- (instancetype)init {if((self=[super init]))_lock=[[NSLock alloc]init];return self;}
- (void)presented:(double)time {[_lock lock];display_observe(&_timeline,time);[_lock unlock];}
- (NSString *)summary {[_lock lock];DisplayTimeline d=_timeline;[_lock unlock];char text[440],first[440];timing_text(&d.intervals,"Actual display intervals",text,sizeof(text));timing_text(&d.cold,"First 64 actual display intervals",first,sizeof(first));double recent=timing_recent_ms(&d.intervals,16);double age=d.last?CACurrentMediaTime()-d.last:0;
 NSString *fps=recent>0?[NSString stringWithFormat:@"%.2f",1000.0/recent]:@"unavailable";return [NSString stringWithFormat:@"Drawable display: monotonic completions %llu, dropped/unpresented %llu, nonmonotonic callbacks %llu; consecutive %llu (longest %llu); latest <=16 interval FPS %@, last display %.2f s ago (FPS is historical while stalled).\n%s\n%s\nDisplay timestamps include loading screens and do not prove scene correctness or gameplay.",(unsigned long long)d.displayed,(unsigned long long)d.dropped,(unsigned long long)d.out_of_order,(unsigned long long)d.consecutive,(unsigned long long)d.longest,fps,age,text,first];}
@end
@interface NativeMeasuredLayer : CAMetalLayer
@property(atomic,strong) NativeDisplayMetrics *displayMetrics;
@end
@implementation NativeMeasuredLayer
- (id<CAMetalDrawable>)nextDrawable {id<CAMetalDrawable> drawable=[super nextDrawable];NativeDisplayMetrics *metrics=self.displayMetrics;if(drawable&&metrics)[drawable addPresentedHandler:^(id<MTLDrawable> completed){[metrics presented:completed.presentedTime];}];return drawable;}
@end
