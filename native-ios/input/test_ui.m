/* UI-only simulator test: production App.m and controls, with fail-closed
   engine entry stubs. Never presented as an engine/gameplay test. */
#define main NativeProductionMain
#define NATIVE_INPUT_UI_TEST 1
#include "../App.m"
#undef main
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static float testVolume=1;static bool testMute;
bool native_audio_set_volume(float volume,bool mute){testVolume=volume;testMute=mute;return true;}
void native_audio_summary(char *text,size_t n){snprintf(text,n,"UI-only audio status stub: no playback");}
int native_scene_probe_settings(const char *g,const char *s,void *l,NativeSettings settings,native_progress p,void *c,char *r,size_t n){abort();}
int native_engine_probe(char *r,size_t n){abort();}
int native_runtime_probe(const char *g,const char *s,native_progress p,void *c,char *r,size_t n){abort();}
int native_renderer_probe(char *r,size_t n,uint8_t *p,size_t pn){abort();}
int native_gpu_replay_probe(char *r,size_t n){abort();}
int native_graphics_checkpoint(const char *g,const char *s,native_progress p,void *c,char *r,size_t n){abort();}
int native_scene_probe_core(const char *g,const char *s,void *l,bool b,bool v,bool core,unsigned m,unsigned q,native_progress p,void *c,char *r,size_t n){abort();}
int native_scene_probe_config(const char *g,const char *s,void *l,bool b,bool v,unsigned m,unsigned q,native_progress p,void *c,char *r,size_t n){abort();}
int native_scene_probe_execution(const char *g,const char *s,void *l,bool b,bool v,native_progress p,void *c,char *r,size_t n){abort();}
int native_scene_probe_options(const char *g,const char *s,void *l,bool b,native_progress p,void *c,char *r,size_t n){abort();}
void native_scene_request_stop(void){}
void native_scene_reset_stop(void){}
static bool testSessionSuspended;
void native_scene_suspend(bool value){testSessionSuspended=value;}
void native_scene_status(char *s,size_t n){snprintf(s,n,"UI test only; no engine");}
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x))[NSException raise:@"InputUITest" format:@"%s line %d",#x,__LINE__];}while(0)

static BOOL UIActive(void){return UIApplication.sharedApplication.applicationState==UIApplicationStateActive;}
static void UISnapshot(UIWindow *window,NSString *name){[window layoutIfNeeded];UIGraphicsImageRenderer *renderer=[[UIGraphicsImageRenderer alloc]initWithSize:window.bounds.size];UIImage *image=[renderer imageWithActions:^(UIGraphicsImageRendererContext *context){[window drawViewHierarchyInRect:window.bounds afterScreenUpdates:YES];}];NSString *file=[NSHomeDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"Documents/ui-%@.png",name]];REQUIRE([UIImagePNGRepresentation(image) writeToFile:file atomically:YES]);}
/* Test-only asynchronous driver. Never hold the main run-loop callback while
   UIKit is committing a presentation/dismissal transaction. */
@interface UITestRunner : NSObject
@property(nonatomic,strong) ProbeController *probe;
@property(nonatomic,strong) UIWindow *window;
@property(nonatomic,strong) NSMutableDictionary *result;
@property(nonatomic,copy) NSArray<dispatch_block_t> *steps;
@property(nonatomic,assign) NSUInteger index;
@property(nonatomic,copy) BOOL (^condition)(void);
@property(nonatomic,assign) CFTimeInterval deadline;
@property(nonatomic,assign) BOOL finished;
- (void)next;
- (void)waitFor:(BOOL (^)(void))condition;
@end
@implementation UITestRunner
- (void)waitFor:(BOOL (^)(void))condition {self.condition=condition;self.deadline=CACurrentMediaTime()+5;}
- (void)finish {
 if(self.finished)return;self.finished=YES;self.result[@"checks"]=@(checks);self.result[@"applicationState"]=@(UIApplication.sharedApplication.applicationState);self.result[@"optionsPresented"]=@(self.probe.optionsPresented);self.result[@"sessionSuspended"]=@(testSessionSuspended);self.result[@"completedSteps"]=@(self.index);
 NSString *path=[NSHomeDirectory() stringByAppendingPathComponent:@"Documents/input-ui-result.json"];NSData *data=[NSJSONSerialization dataWithJSONObject:self.result options:NSJSONWritingPrettyPrinted error:nil];[data writeToFile:path atomically:YES];NSLog(@"INPUT_UI_RESULT %@",self.result);self.steps=nil;self.condition=nil;
}
- (void)next {
 if(self.finished)return;
 @try {
  if(self.condition){if(!self.condition()){if(CACurrentMediaTime()>=self.deadline)[NSException raise:@"InputUITest" format:@"UIKit transition timeout at step %lu",(unsigned long)self.index];dispatch_after(dispatch_time(DISPATCH_TIME_NOW,20*NSEC_PER_MSEC),dispatch_get_main_queue(),^{[self next];});return;}self.condition=nil;}
  if(self.index==self.steps.count){self.result[@"result"]=@"PASS";[self finish];return;}
  self.steps[self.index++]();dispatch_async(dispatch_get_main_queue(),^{[self next];});
 }@catch(NSException *error){self.result[@"error"]=error.reason?:@"unknown exception";[self finish];}
}
@end
static BOOL UIPageReady(ProbeController *p){return p.settingsNavigation.view.window!=nil&&!p.settingsNavigation.isBeingPresented;}
static void UILegacyInput(ProbeController *p,NSMutableDictionary *result){
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
        ni_begin();ni_connect(true);ni_controller((NIState){0});p.sceneRunning=YES;p.gameControls.hidden=NO;p.gameControls.playing=YES;[p.view layoutIfNeeded];REQUIRE(p.gameControls.art.alpha>0);REQUIRE(!p.gameControls.art.userInteractionEnabled);REQUIRE(![p.mainScroll touchesShouldCancelInContentView:p.gameControls]);REQUIRE(![p.mainScroll touchesShouldCancelInContentView:p.gameControls.settingsButton]);REQUIRE([p.mainScroll touchesShouldCancelInContentView:p.deadzoneSlider]);REQUIRE([p.mainScroll touchesShouldCancelInContentView:p.stopButton]);
        ni_controller((NIState){.buttons=NI_CROSS});[p.gameControls updateSource];REQUIRE(p.gameControls.art.alpha==0);REQUIRE(!p.gameControls.art.userInteractionEnabled);CGPoint point=CGPointMake(p.gameControls.bounds.size.width*.2,p.gameControls.bounds.size.height*.5);REQUIRE([p.gameControls hitTest:point withEvent:nil]==p.gameControls);
        ni_touch((NIState){0},true);[p.gameControls updateSource];REQUIRE(p.gameControls.art.alpha>0);REQUIRE(ni_source()==NI_TOUCH);
        ni_controller((NIState){0});ni_controller((NIState){.buttons=NI_CROSS});p.activeController=snapshot;REQUIRE(ni_source()==NI_CONTROLLER);[p controllerChanged:[NSNotification notificationWithName:GCControllerDidDisconnectNotification object:snapshot]];REQUIRE(ni_source()==NI_TOUCH);uint8_t released[56]={0};ni_consume(released);uint32_t releasedMask;memcpy(&releasedMask,released+12,4);REQUIRE(releasedMask==0);REQUIRE(p.gameControls.art.alpha>0);
        for(unsigned i=0;i<3;i++){
          p.fullscreenSwitch.on=YES;[p fullscreenChanged];[p.view layoutIfNeeded];REQUIRE(p.immersive);REQUIRE(p.gameControls.superview==p.view);REQUIRE(p.prefersStatusBarHidden&&p.prefersHomeIndicatorAutoHidden);REQUIRE(!p.gameHeight.active);REQUIRE(p.mainScroll.hidden);REQUIRE(p.settingsStore.value.fullscreen);CGRect f=p.surfaceView.frame;REQUIRE(f.size.height>0&&fabs(f.size.width/f.size.height-16./9)<.001);
          p.fullscreenSwitch.on=NO;[p fullscreenChanged];[p.view layoutIfNeeded];REQUIRE(!p.immersive&&p.gameControls.superview==p.mainStack);REQUIRE(p.gameHeight.active&&!p.mainScroll.hidden);REQUIRE(!p.settingsStore.value.fullscreen);REQUIRE(p.mainScroll.scrollEnabled&&p.mainScroll.canCancelContentTouches);REQUIRE([p.mainScroll touchesShouldCancelInContentView:p.filesButton]);
        }
        REQUIRE(!p.gameControls.performanceLabel.userInteractionEnabled);REQUIRE(p.gameControls.settingsButton.bounds.size.height>=44);p.optionsPresented=YES;[p inputInterrupted:nil];[p inputResumed:nil];REQUIRE(testSessionSuspended);uint8_t menuRaw[56]={0};ni_consume(menuRaw);uint32_t menuMask;memcpy(&menuMask,menuRaw+12,4);REQUIRE(menuMask==0);p.optionsPresented=NO;[p inputResumed:nil];REQUIRE(testSessionSuspended!=UIActive());
        p.deadzoneSlider.value=.2;p.sensitivitySlider.value=1.25;p.opacitySlider.value=.4;p.sizeSlider.value=1.1;[p inputSettingsChanged];REQUIRE(fabsf(p.settingsStore.value.deadzone-.2f)<.001);REQUIRE(fabs(p.gameControls.art.alpha-.4)<.001);
        [p inputInterrupted:nil];REQUIRE(testSessionSuspended);uint8_t raw[56]={0};ni_consume(raw);uint32_t mask;memcpy(&mask,raw+12,4);REQUIRE(mask==0);[p inputResumed:nil];REQUIRE(testSessionSuspended!=UIActive());REQUIRE(ni_source()==NI_TOUCH);
        ni_connect(false);[p.gameControls updateSource];REQUIRE(p.gameControls.art.alpha>0);p.sceneRunning=NO;p.gameControls.playing=NO;ni_end();[p applyFullscreen];REQUIRE(p.gameControls.art.alpha==0);
        ProbeController *reloaded=[[ProbeController alloc]init];[reloaded loadViewIfNeeded];REQUIRE(fabsf(reloaded.sensitivitySlider.value-1.25f)<.001);REQUIRE(fabsf(reloaded.sizeSlider.value-1.1f)<.001);p.timingSwitch.on=NO;[p preferenceSwitchChanged:p.timingSwitch];p.shadowCompatibilitySwitch.on=YES;[p preferenceSwitchChanged:p.shadowCompatibilitySwitch];ProbeController *saved=[[ProbeController alloc]init];[saved loadViewIfNeeded];REQUIRE(!saved.timingSwitch.isOn&&saved.shadowCompatibilitySwitch.isOn);p.profileSwitch.on=YES;[p preferenceSwitchChanged:p.profileSwitch];ProbeController *profileSaved=[[ProbeController alloc]init];[profileSaved loadViewIfNeeded];REQUIRE(profileSaved.profileSwitch.on);[p setWorking:YES];REQUIRE(!p.profileSwitch.enabled);[p setWorking:NO];REQUIRE(p.profileSwitch.enabled);REQUIRE(p.executionSwitch.enabled);REQUIRE(!p.cpuCoreSwitch.on);p.cpuCoreSwitch.on=YES;[p preferenceSwitchChanged:p.cpuCoreSwitch];ProbeController *coreSaved=[[ProbeController alloc]init];[coreSaved loadViewIfNeeded];REQUIRE(coreSaved.cpuCoreSwitch.on);[p setWorking:YES];REQUIRE(!p.cpuCoreSwitch.enabled);[p setWorking:NO];REQUIRE(p.cpuCoreSwitch.enabled);[p applyDefaultSettings];REQUIRE(!p.cpuCoreSwitch.on&&!p.settingsStore.value.coreV2);REQUIRE(p.gameModeControl.numberOfSegments==2&&p.qualityControl.numberOfSegments==6);p.gameModeControl.selectedSegmentIndex=1;[p launchChoiceChanged:p.gameModeControl];p.qualityControl.selectedSegmentIndex=2;[p launchChoiceChanged:p.qualityControl];ProbeController *launchSaved=[[ProbeController alloc]init];[launchSaved loadViewIfNeeded];REQUIRE(launchSaved.gameModeControl.selectedSegmentIndex==1&&launchSaved.qualityControl.selectedSegmentIndex==2);[p setWorking:YES];REQUIRE(!p.gameModeControl.enabled&&!p.qualityControl.enabled);[p setWorking:NO];REQUIRE(p.gameModeControl.enabled&&p.qualityControl.enabled);p.executionSwitch.on=YES;[p preferenceSwitchChanged:p.executionSwitch];ProbeController *backendSaved=[[ProbeController alloc]init];[backendSaved loadViewIfNeeded];REQUIRE(backendSaved.executionSwitch.on);[p setWorking:YES];REQUIRE(!p.executionSwitch.enabled);[p setWorking:NO];[p applyDefaultSettings];REQUIRE(!p.executionSwitch.on&&!p.settingsStore.value.rendererV2);[profileSaved applyDefaultSettings];REQUIRE(!profileSaved.profileSwitch.on);REQUIRE(!saved.report.isFirstResponder);REQUIRE(!((NativeReportView *)saved.report).displaying);
        NSURL *folder=[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];NSData *bookmark=[folder bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:nil];REQUIRE(bookmark!=nil);[NSUserDefaults.standardUserDefaults setObject:bookmark forKey:@"gameFolderBookmark"];ProbeController *remembered=[[ProbeController alloc]init];[remembered loadViewIfNeeded];REQUIRE(remembered.gameFolder!=nil&&!remembered.folderValidated);[remembered applyDefaultSettings];REQUIRE(remembered.gameFolder!=nil);REQUIRE(remembered.timingSwitch.isOn&&!remembered.shadowCompatibilitySwitch.isOn&&!remembered.fullscreenSwitch.isOn);REQUIRE(fabsf(remembered.deadzoneSlider.value-.15)<.001);REQUIRE(fabsf(remembered.sensitivitySlider.value-1)<.001);REQUIRE([NSUserDefaults.standardUserDefaults dataForKey:@"gameFolderBookmark"]!=nil);
        [p append:@"Native startup stopped (trap 5): UI test fatal, not a real engine crash"];REQUIRE(p.hasFatal&&[p.runtimeStatus containsString:@"UI test fatal"]);[p append:@"Session transition: camera scene event"];REQUIRE([p.runtimeStatus containsString:@"UI test fatal"]);[p setWorking:YES];REQUIRE(!p.hasFatal&&!p.shadowCompatibilitySwitch.enabled&&!p.timingSwitch.enabled);p.shadowCompatibilitySwitch.on=YES;[p applyDefaultSettings];REQUIRE(p.shadowCompatibilitySwitch.isOn);[p setWorking:NO];REQUIRE(p.shadowCompatibilitySwitch.enabled&&p.timingSwitch.enabled);
}
@interface InputTestDelegate : UIResponder<UIApplicationDelegate>
@property(nonatomic,strong) UIWindow *window;
@property(nonatomic,strong) UITestRunner *runner;
@end
@implementation InputTestDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
 self.window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];ProbeController *p=[[ProbeController alloc]init];self.window.rootViewController=p;[self.window makeKeyAndVisible];
 UITestRunner *runner=self.runner=[[UITestRunner alloc]init];runner.probe=p;runner.window=self.window;NSMutableDictionary *result=runner.result=[@{@"result":@"FAIL",@"gameplayVerified":@NO,@"scope":@"Production UIKit/controller mapping and real modal lifecycle in iOS simulator; engine launch stubs abort"} mutableCopy];__weak UITestRunner *weak=runner;UIWindow *window=self.window;NSMutableArray<dispatch_block_t> *steps=[NSMutableArray array];
 [steps addObject:^{[weak waitFor:^BOOL{return UIActive();}];}];
 [steps addObject:^{
        [p.view layoutIfNeeded];REQUIRE([p.mainScroll isKindOfClass:NativeMenuScrollView.class]);REQUIRE(p.mainScroll.scrollEnabled&&p.mainScroll.canCancelContentTouches&&p.mainScroll.alwaysBounceVertical);REQUIRE(!p.mainScroll.delaysContentTouches);REQUIRE(!p.gameControls.clipsToBounds);
        REQUIRE([p.mainScroll touchesShouldCancelInContentView:p.filesButton]);REQUIRE([p.mainScroll touchesShouldCancelInContentView:p.deadzoneSlider]);REQUIRE([p.mainScroll touchesShouldCancelInContentView:p.fullscreenSwitch]);REQUIRE([p.mainScroll touchesShouldCancelInContentView:p.sceneButton]);REQUIRE([p.mainScroll touchesShouldCancelInContentView:p.gameControls]);
        REQUIRE([p.filesButton isDescendantOfView:p.mainStack]);REQUIRE(![p.report isDescendantOfView:p.mainStack]);REQUIRE(![p.deadzoneSlider isDescendantOfView:p.mainStack]);REQUIRE(![p.shadowCompatibilitySwitch isDescendantOfView:p.mainStack]);REQUIRE([p.sceneButton.accessibilityIdentifier isEqualToString:@"startGame"]);REQUIRE(p.filesButton.bounds.size.height>=44);REQUIRE(p.sceneButton.bounds.size.height>=44);REQUIRE(p.mainScroll.contentSize.height>=p.mainStack.bounds.size.height);
        CGFloat bottom=MAX(0,p.mainScroll.contentSize.height-p.mainScroll.bounds.size.height+p.mainScroll.adjustedContentInset.bottom);[p.mainScroll setContentOffset:CGPointMake(0,bottom) animated:NO];REQUIRE(fabs(p.mainScroll.contentOffset.y-bottom)<.01);[p.mainScroll setContentOffset:CGPointZero animated:NO];UISnapshot(window,@"dashboard");
 // Real UIKit hierarchy at compact/wide landscape bounds; preview only, not a device rotation claim.
 for(NSNumber *width in @[@667,@932]){CGRect old=window.frame;window.frame=CGRectMake(0,0,width.doubleValue,430);[p.view setNeedsLayout];[p.view layoutIfNeeded];REQUIRE(p.libraryLandscape);CGRect play=[p.sceneButton convertRect:p.sceneButton.bounds toView:p.view];REQUIRE(play.size.height>=44&&CGRectGetMinY(play)>=0&&CGRectGetMaxY(play)<=p.view.bounds.size.height);UISnapshot(window,[NSString stringWithFormat:@"library-landscape-%@",width]);window.frame=old;[p.view setNeedsLayout];[p.view layoutIfNeeded];}
 
        REQUIRE(!((NativeReportView *)p.report).displaying);NativeReportView *bounded=(NativeReportView *)p.report;NSUInteger beforeWrites=bounded.layoutWrites;for(unsigned i=0;i<200;i++)[p append:@"UI regression log only"];REQUIRE([p.report.storedText containsString:@"UI regression log only"]);REQUIRE(bounded.layoutWrites==beforeWrites);REQUIRE(p.report.storedText.length<=32768);result[@"hiddenLogAssignmentsAvoided"]=@200;

        NSMutableArray<NSString *> *reports=[NSMutableArray arrayWithCapacity:200];NSString *body=[@"0123456789abcdef" stringByPaddingToLength:8192 withString:@"0123456789abcdef" startingAtIndex:0];for(unsigned i=0;i<200;i++)[reports addObject:[NSString stringWithFormat:@"%u:%@",i,body]];UITextView *eager=[[UITextView alloc]initWithFrame:CGRectMake(0,0,320,300)];NativeReportView *lazy=[[NativeReportView alloc]initWithFrame:eager.frame];CFTimeInterval t0=CACurrentMediaTime();for(NSString *s in reports)eager.text=s;double eagerMs=(CACurrentMediaTime()-t0)*1000;t0=CACurrentMediaTime();for(NSString *s in reports)lazy.storedText=s;double hiddenMs=(CACurrentMediaTime()-t0)*1000;REQUIRE([eager.text isEqualToString:lazy.storedText]);REQUIRE(lazy.layoutWrites==0);lazy.displaying=YES;REQUIRE(lazy.layoutWrites==1);REQUIRE([lazy.text isEqualToString:lazy.storedText]);result[@"logAssignmentBenchmark"]=@{@"samples":@200,@"approximateBytesPerText":@8196,@"UITextViewMs":@(eagerMs),@"hiddenReportMs":@(hiddenMs),@"scope":@"Simulator text-assignment API wall only, no layout forcing or iPhone FPS claim"};

 }];
 for(NSUInteger page=1;page<=8;page++){
  [steps addObject:^{[p showOptionsPage:page];[weak waitFor:^BOOL{return UIPageReady(p);}];}];
  [steps addObject:^{REQUIRE(p.optionsPresented&&testSessionSuspended);REQUIRE(((NativeOptionsController *)p.settingsNavigation.topViewController).page==page);UIView *view=p.settingsNavigation.topViewController.view;[view layoutIfNeeded];REQUIRE(view.safeAreaLayoutGuide.layoutFrame.size.width>0);if(page==1){REQUIRE([p.settingControls[@"fullscreen"] isDescendantOfView:view]);REQUIRE([p.settingControls[@"showFPS"] isDescendantOfView:view]);}if(page==2){REQUIRE([p.settingControls[@"deadzone"] isDescendantOfView:view]);REQUIRE([p.settingControls[@"opacity"] isDescendantOfView:view]);}if(page==3){REQUIRE([p.filesStatusLabel isDescendantOfView:view]);REQUIRE([p.filesPageButton isDescendantOfView:view]);[p setWorking:YES];REQUIRE(!p.filesPageButton.enabled&&[p.filesStatusLabel.text containsString:@"Checking"]);[p setWorking:NO];REQUIRE(p.filesPageButton.enabled);}if(page==4){REQUIRE(!p.report.selectable);REQUIRE(![p respondsToSelector:NSSelectorFromString(@"copyReport")]);REQUIRE([p.report isDescendantOfView:view]);REQUIRE(p.report.displaying);REQUIRE([p.report.text isEqualToString:p.report.storedText]);}UISnapshot(window,NativePageNames()[page]);[p closeOptions];[weak waitFor:^BOOL{return !p.optionsPresented;}];}];
  [steps addObject:^{REQUIRE(!p.optionsPresented&&(testSessionSuspended!=UIActive()));REQUIRE(!p.report.displaying);}];
 }
 [steps addObject:^{[p showOptionsPage:0];[weak waitFor:^BOOL{return UIPageReady(p);}];}];
 [steps addObject:^{[p inputResumed:nil];REQUIRE(testSessionSuspended);[p dismissViewControllerAnimated:YES completion:nil];[weak waitFor:^BOOL{return p.presentedViewController==nil;}];}];
 [steps addObject:^{[p presentationControllerDidDismiss:p.settingsNavigation.presentationController];REQUIRE(!p.optionsPresented&&(testSessionSuspended!=UIActive()));p.traitOverrides.preferredContentSizeCategory=UIContentSizeCategoryAccessibilityExtraExtraExtraLarge;[p.view setNeedsLayout];[p.view layoutIfNeeded];UISnapshot(window,@"dashboard-accessibility");[p showOptionsPage:2];p.settingsNavigation.traitOverrides.preferredContentSizeCategory=UIContentSizeCategoryAccessibilityExtraExtraExtraLarge;[weak waitFor:^BOOL{return UIPageReady(p);}];}];
 [steps addObject:^{REQUIRE([p.settingsNavigation.topViewController.traitCollection.preferredContentSizeCategory isEqualToString:UIContentSizeCategoryAccessibilityExtraExtraExtraLarge]);REQUIRE(p.controllerLabel.font.pointSize>17);UIScrollView *scroll=(UIScrollView *)p.settingsNavigation.topViewController.view.subviews.firstObject;REQUIRE(scroll.contentSize.height>scroll.bounds.size.height);REQUIRE([scroll touchesShouldCancelInContentView:p.deadzoneSlider]);UISnapshot(window,@"controls-accessibility");[p closeOptions];[weak waitFor:^BOOL{return !p.optionsPresented;}];}];
 [steps addObject:^{p.traitOverrides.preferredContentSizeCategory=UIContentSizeCategoryLarge;
 // Corrupt preferences fail visibly; legacy values cannot override v1.
 NSUserDefaults *preferences=NSUserDefaults.standardUserDefaults;NSDictionary *valid=[preferences dictionaryForKey:@"launcherSettings.v1"];
 [preferences setObject:@{@"schema":@99} forKey:@"launcherSettings.v1"];NativeSettingsStore *bad=[NativeSettingsStore new];REQUIRE(bad.validationError!=nil);REQUIRE([[preferences dictionaryForKey:@"launcherSettings.v1"][@"schema"] isEqual:@99]);
 [preferences setObject:@{@"schema":@1,@"fps":@60} forKey:@"launcherSettings.v1"];bad=[NativeSettingsStore new];REQUIRE(bad.validationError!=nil);
 [preferences setObject:valid forKey:@"launcherSettings.v1"];[preferences setInteger:2 forKey:@"launchMode"];NativeSettingsStore *authoritative=[NativeSettingsStore new];REQUIRE(authoritative.value.mode==p.settingsStore.value.mode);
 NativeSettings original=p.settingsStore.value;for(unsigned preset=0;preset<5;preset++){[p changeSetting:1 value:preset];NativeSettings v=p.settingsStore.value;REQUIRE(v.preset==preset);REQUIRE(!native_settings_validate(&v));ProbeController *copy=[ProbeController new];[copy loadViewIfNeeded];REQUIRE(copy.settingsStore.value.preset==preset);}
 [p setWorking:YES];NativeSettings held=p.settingsStore.value;[p changeSetting:3 value:70];REQUIRE(p.settingsStore.value.scale==held.scale);[p setWorking:NO];
 [p changeSetting:3 value:70];REQUIRE(p.settingsStore.value.preset==5);unsigned w,h;NativeSettings scaled=p.settingsStore.value;REQUIRE(native_settings_size(&scaled,&w,&h)&&w==448&&h==252);
 p.settingsStore.value=original;[p.settingsStore save];[p applyStoredSettings];
 for(size_t i=0;i<NATIVE_SETTING_COUNT;i++){const NativeSettingSpec *f=&native_setting_specs[i];if(!strcmp(f->key,"volume")){[p changeSetting:i value:.25];REQUIRE(fabs(testVolume-.25)<.001);}if(!strcmp(f->key,"mute")){[p changeSetting:i value:1];REQUIRE(testMute);}}
 [p applyDefaultSettings];REQUIRE(testVolume==1&&!testMute);UILegacyInput(p,result);p.gameFolder=nil;[p startGameTapped];[weak waitFor:^BOOL{return UIPageReady(p);}];}];
 [steps addObject:^{REQUIRE(p.optionsPresented&&((NativeOptionsController *)p.settingsNavigation.topViewController).page==3);[p closeOptions];[weak waitFor:^BOOL{return !p.optionsPresented;}];}];
 [steps addObject:^{REQUIRE(!p.busy);[p beginReport:@"UI full-log prefix"];REQUIRE(p.sessionLog!=nil);NSString *longLine=[@"complete-record-" stringByPaddingToLength:100000 withString:@"abcdefghijklmnopqrstuvwxyz" startingAtIndex:0];RuntimeProgress((__bridge void*)p,longLine.UTF8String);[p flushProgress];REQUIRE(p.report.storedText.length<=32768);NSError *journalError=nil;REQUIRE([p.sessionLog flush:&journalError]);NSString *full=[NSString stringWithContentsOfURL:p.sessionLog.URL encoding:NSUTF8StringEncoding error:&journalError];REQUIRE(full.length>100000&&[full containsString:longLine]);NSURL *savedURL=p.sessionLog.URL;UIActivityViewController *fileShare=NativeLogActivity(savedURL);REQUIRE([fileShare.excludedActivityTypes containsObject:UIActivityTypeCopyToPasteboard]);REQUIRE([savedURL.pathExtension isEqualToString:@"txt"]);[p setWorking:NO];ProbeController *logReloaded=[[ProbeController alloc]init];[logReloaded loadViewIfNeeded];REQUIRE([logReloaded.lastLogURL isEqual:savedURL]);result[@"fullLogStoredBytes"]=@([full lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);result[@"screenshots"]=@[@"dashboard",@"graphics",@"controls",@"files",@"advanced",@"dashboard-accessibility",@"controls-accessibility"];}];
 runner.steps=steps;dispatch_after(dispatch_time(DISPATCH_TIME_NOW,250*NSEC_PER_MSEC),dispatch_get_main_queue(),^{[runner next];});return YES;
}
@end
int main(int argc,char **argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(InputTestDelegate.class));}}
