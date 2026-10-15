/* UI-only simulator test: production App.m and controls, with fail-closed
   engine entry stubs. Never presented as an engine/gameplay test. */
#define main NativeProductionMain
#define NATIVE_INPUT_UI_TEST 1
#include "../App.m"
#undef main
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
int native_engine_probe(char *r,size_t n){abort();}
int native_runtime_probe(const char *g,const char *s,native_progress p,void *c,char *r,size_t n){abort();}
int native_renderer_probe(char *r,size_t n,uint8_t *p,size_t pn){abort();}
int native_gpu_replay_probe(char *r,size_t n){abort();}
int native_graphics_checkpoint(const char *g,const char *s,native_progress p,void *c,char *r,size_t n){abort();}
int native_scene_probe_options(const char *g,const char *s,void *l,bool b,native_progress p,void *c,char *r,size_t n){abort();}
void native_scene_request_stop(void){}
void native_scene_reset_stop(void){}
void native_scene_status(char *s,size_t n){snprintf(s,n,"UI test only; no engine");}
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x))[NSException raise:@"InputUITest" format:@"%s line %d",#x,__LINE__];}while(0)
@interface InputTestDelegate : UIResponder<UIApplicationDelegate>
@property(nonatomic,strong) UIWindow *window;
@end
@implementation InputTestDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    self.window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];ProbeController *p=[[ProbeController alloc]init];self.window.rootViewController=p;[self.window makeKeyAndVisible];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,250*NSEC_PER_MSEC),dispatch_get_main_queue(),^{
      NSMutableDictionary *result=[@{@"result":@"FAIL",@"gameplayVerified":@NO,@"scope":@"Production UIKit/controller mapping in iOS simulator; engine launch stubs abort"} mutableCopy];
      @try {
        GCController *snapshot=[GCController controllerWithExtendedGamepad];REQUIRE(snapshot.extendedGamepad!=nil);GCExtendedGamepad *pad=snapshot.extendedGamepad;
        NSArray<GCControllerButtonInput *> *buttons=@[pad.buttonA,pad.buttonB,pad.buttonX,pad.buttonY,pad.leftShoulder,pad.rightShoulder,pad.buttonMenu];
        const uint32_t bits[]={NI_CROSS,NI_CIRCLE,NI_SQUARE,NI_TRIANGLE,NI_L1,NI_R1,NI_START};
        for(unsigned i=0;i<buttons.count;i++){[buttons[i] setValue:1];REQUIRE(NativeControllerState(pad).buttons==bits[i]);[buttons[i] setValue:0];REQUIRE(NativeControllerState(pad).buttons==0);}
        const float dx[]={0,1,0,-1},dy[]={1,0,-1,0};const uint32_t db[]={NI_UP,NI_RIGHT,NI_DOWN,NI_LEFT};
        for(unsigned i=0;i<4;i++){[pad.dpad setValueForXAxis:dx[i] yAxis:dy[i]];REQUIRE(NativeControllerState(pad).buttons==db[i]);[pad.dpad setValueForXAxis:0 yAxis:0];REQUIRE(NativeControllerState(pad).buttons==0);}
        if(pad.buttonOptions){[pad.buttonOptions setValue:1];REQUIRE(NativeControllerState(pad).buttons==NI_SELECT);[pad.buttonOptions setValue:0];}
        if(pad.leftThumbstickButton){[pad.leftThumbstickButton setValue:1];REQUIRE(NativeControllerState(pad).buttons==NI_L3);[pad.leftThumbstickButton setValue:0];}
        if(pad.rightThumbstickButton){[pad.rightThumbstickButton setValue:1];REQUIRE(NativeControllerState(pad).buttons==NI_R3);[pad.rightThumbstickButton setValue:0];}
        [pad.leftThumbstick setValueForXAxis:.7f yAxis:-.6f];[pad.rightThumbstick setValueForXAxis:-.9f yAxis:.5f];[pad.leftTrigger setValue:.4f];[pad.rightTrigger setValue:.8f];NIState state=NativeControllerState(pad);REQUIRE(fabsf(state.lx-.7f)<.001f&&fabsf(state.ly+.6f)<.001f);REQUIRE(fabsf(state.rx+.9f)<.001f&&fabsf(state.ry-.5f)<.001f);REQUIRE(fabsf(state.lt-.4f)<.001f&&fabsf(state.rt-.8f)<.001f);
        ni_begin();ni_connect(true);ni_controller((NIState){0});p.sceneRunning=YES;p.gameControls.hidden=NO;p.gameControls.playing=YES;[p.view layoutIfNeeded];REQUIRE(p.gameControls.art.alpha>0);REQUIRE(!p.gameControls.art.userInteractionEnabled);
        ni_controller((NIState){.buttons=NI_CROSS});[p.gameControls updateSource];REQUIRE(p.gameControls.art.alpha==0);REQUIRE(!p.gameControls.art.userInteractionEnabled);CGPoint point=CGPointMake(p.gameControls.bounds.size.width*.2,p.gameControls.bounds.size.height*.5);REQUIRE([p.gameControls hitTest:point withEvent:nil]==p.gameControls);
        ni_touch((NIState){0},true);[p.gameControls updateSource];REQUIRE(p.gameControls.art.alpha>0);REQUIRE(ni_source()==NI_TOUCH);
        for(unsigned i=0;i<3;i++){
          p.fullscreenSwitch.on=YES;[p fullscreenChanged];[p.view layoutIfNeeded];REQUIRE(p.immersive);REQUIRE(p.gameControls.superview==p.view);REQUIRE(p.prefersStatusBarHidden&&p.prefersHomeIndicatorAutoHidden);REQUIRE(!p.gameHeight.active);REQUIRE(p.mainScroll.hidden);REQUIRE([NSUserDefaults.standardUserDefaults boolForKey:@"inputFullscreen"]);CGRect f=p.surfaceView.frame;REQUIRE(f.size.height>0&&fabs(f.size.width/f.size.height-16./9)<.001);
          p.fullscreenSwitch.on=NO;[p fullscreenChanged];[p.view layoutIfNeeded];REQUIRE(!p.immersive&&p.gameControls.superview==p.mainStack);REQUIRE(p.gameHeight.active&&!p.mainScroll.hidden);REQUIRE(![NSUserDefaults.standardUserDefaults boolForKey:@"inputFullscreen"]);
        }
        p.deadzoneSlider.value=.2;p.sensitivitySlider.value=1.25;p.opacitySlider.value=.4;p.sizeSlider.value=1.1;[p inputSettingsChanged];REQUIRE(fabsf([NSUserDefaults.standardUserDefaults floatForKey:@"inputDeadzone"]-.2f)<.001);REQUIRE(fabs(p.gameControls.art.alpha-.4)<.001);
        [p inputInterrupted:nil];uint8_t raw[56]={0};ni_consume(raw);uint32_t mask;memcpy(&mask,raw+12,4);REQUIRE(mask==0);[p inputResumed:nil];REQUIRE(ni_source()==NI_TOUCH);
        ni_connect(false);[p.gameControls updateSource];REQUIRE(p.gameControls.art.alpha>0);p.sceneRunning=NO;p.gameControls.playing=NO;ni_end();[p applyFullscreen];REQUIRE(p.gameControls.art.alpha==0);
        ProbeController *reloaded=[[ProbeController alloc]init];[reloaded loadViewIfNeeded];REQUIRE(fabsf(reloaded.sensitivitySlider.value-1.25f)<.001);REQUIRE(fabsf(reloaded.sizeSlider.value-1.1f)<.001);result[@"result"]=@"PASS";
      } @catch(NSException *error){result[@"error"]=error.reason?:@"unknown exception";}
      result[@"checks"]=@(checks);NSString *path=[NSHomeDirectory() stringByAppendingPathComponent:@"Documents/input-ui-result.json"];NSData *data=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:nil];[data writeToFile:path atomically:YES];NSLog(@"INPUT_UI_RESULT %@",result);
    });return YES;
}
@end
int main(int argc,char **argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(InputTestDelegate.class));}}
