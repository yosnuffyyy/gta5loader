/* The drawable owns each callback until completion; callbacks retain only
   this independent monitor, never a NativeGpu or freed engine allocation. */
#include "display_timeline.h"
@interface NativeDisplayMetrics : NSObject {
 NSLock *_lock;DisplayTimeline _timeline,_scene;double _sceneStart;
}
- (void)presented:(double)time;
- (NSString *)summary;
- (NSString *)overlay;
- (void)markScene;
- (NSString *)sceneSummary;
@end
@implementation NativeDisplayMetrics
- (instancetype)init {if((self=[super init]))_lock=[[NSLock alloc]init];return self;}
- (void)presented:(double)time {[_lock lock];display_observe(&_timeline,time);if(_sceneStart)display_marked_observe(&_scene,time,_sceneStart);[_lock unlock];}
- (void)markScene {[_lock lock];memset(&_scene,0,sizeof(_scene));_sceneStart=CACurrentMediaTime();[_lock unlock];}
- (NSString *)sceneSummary {[_lock lock];DisplayTimeline d=_scene;double start=_sceneStart;[_lock unlock];if(!start)return @"No user-marked scene window. All-run timing includes loading.";char text[440];timing_text(&d.intervals,"User-marked actual display intervals",text,sizeof(text));return [NSString stringWithFormat:@"User-marked visible-scene window: %.2f seconds elapsed, %llu actual displays. This is a user marker, not automatic gameplay verification.\n%s",CACurrentMediaTime()-start,(unsigned long long)d.displayed,text];}
- (NSString *)overlay {[_lock lock];DisplayTimeline d=_sceneStart?_scene:_timeline;bool marked=_sceneStart!=0;[_lock unlock];double ms=timing_recent_ms(&d.intervals,16);return ms>0?[NSString stringWithFormat:@"Displayed %.1f FPS · %.1f ms mean (latest ≤16)\n%llu display callbacks · last %.1f s ago · %@",1000/ms,ms,(unsigned long long)d.displayed,CACurrentMediaTime()-d.last,marked?@"user-marked scene window":@"includes loading"]:@"Waiting for actual drawable display timestamps…";}
- (NSString *)summary {[_lock lock];DisplayTimeline d=_timeline;[_lock unlock];char text[440],first[440];timing_text(&d.intervals,"Actual display intervals",text,sizeof(text));timing_text(&d.cold,"First 64 actual display intervals",first,sizeof(first));double recent=timing_recent_ms(&d.intervals,16);double age=d.last?CACurrentMediaTime()-d.last:0;
 NSString *fps=recent>0?[NSString stringWithFormat:@"%.2f",1000.0/recent]:@"unavailable";return [[NSString stringWithFormat:@"Drawable display: monotonic completions %llu, zero/invalid-time callbacks %llu, nonmonotonic callbacks %llu; consecutive monotonic callbacks %llu (longest %llu); latest <=16 interval FPS %@, last display %.2f s ago (FPS is historical while stalled).\n%s\n%s\nDisplay timestamps include loading screens and do not prove scene correctness or gameplay.",(unsigned long long)d.displayed,(unsigned long long)d.dropped,(unsigned long long)d.out_of_order,(unsigned long long)d.consecutive,(unsigned long long)d.longest,fps,age,text,first] stringByAppendingFormat:@"\n%@",[self sceneSummary]];}
@end
@interface NativeMeasuredLayer : CAMetalLayer
@property(atomic,strong) NativeDisplayMetrics *displayMetrics;
@end
@implementation NativeMeasuredLayer
- (id<CAMetalDrawable>)nextDrawable {id<CAMetalDrawable> drawable=[super nextDrawable];NativeDisplayMetrics *metrics=self.displayMetrics;if(drawable&&metrics)[drawable addPresentedHandler:^(id<MTLDrawable> completed){[metrics presented:completed.presentedTime];}];return drawable;}
@end
