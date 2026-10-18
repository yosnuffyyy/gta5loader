#import "diagnostics/native_journal.h"
#include "diagnostics/native_profile.h"
#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <GameController/GameController.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <CommonCrypto/CommonDigest.h>
#import <mach/mach.h>
#import <QuartzCore/CAMetalLayer.h>
#include "probe.h"
#include "renderer/shadow_failure_log.h"
#include "renderer/native_renderer.h"
#include "renderer/native_launch.h"
#include <math.h>
#if defined(NATIVE_INPUT_UI_TEST)
#include "input/ui_presentation_stub.inc"
#else
#include "renderer/presentation_metrics.inc"
#endif

@interface NativeSurfaceView : UIView
@end
@implementation NativeSurfaceView
+ (Class)layerClass {return NativeMeasuredLayer.class;}
@end
#include "input/NativeControls.inc"

/* Menus must be able to turn a button/slider touch into a scroll. Only
   the live game surface owns its gesture until release (virtual sticks). */
@interface NativeMenuScrollView : UIScrollView
@end
@implementation NativeMenuScrollView
- (BOOL)touchesShouldCancelInContentView:(UIView *)view {
    for(UIView *owner=view;owner&&owner!=self;owner=owner.superview)
        if([owner isKindOfClass:NativeGameControls.class]&&((NativeGameControls *)owner).playing)return NO;
    return YES;
}
@end

static NSString *Footprint(void) {
    task_vm_info_data_t info={0};mach_msg_type_number_t count=TASK_VM_INFO_COUNT;
    if(task_info(mach_task_self(),TASK_VM_INFO,(task_info_t)&info,&count)!=KERN_SUCCESS)return @"Memory measurement unavailable";
    return [NSString stringWithFormat:@"Process footprint: %.1f MiB",(double)info.phys_footprint/1048576.0];
}
static NSString *Digest(NSData *bytes) {
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256(bytes.bytes,(CC_LONG)bytes.length,digest);
    NSMutableString *result=[NSMutableString string];for(unsigned i=0;i<sizeof(digest);i++)[result appendFormat:@"%02x",digest[i]];return result;
}
@class NativeReportView;
@interface ProbeController : UIViewController <UIDocumentPickerDelegate, UIAdaptivePresentationControllerDelegate>
@property(nonatomic,strong) NativeReportView *report;
@property(nonatomic,strong) NativeSessionLog *sessionLog;
@property(nonatomic,strong) NSURL *lastLogURL;
@property(nonatomic,assign) BOOL sharingLog;
@property(nonatomic,strong) UIButton *runButton;
@property(nonatomic,strong) UIButton *filesButton;
@property(nonatomic,strong) UIButton *runtimeButton;
@property(nonatomic,strong) UIButton *rendererButton;
@property(nonatomic,strong) UIButton *integrationButton;
@property(nonatomic,strong) UIButton *sceneButton;
@property(nonatomic,strong) UIButton *stopButton;
@property(nonatomic,strong) UIButton *sceneTimingButton;
@property(nonatomic,strong) NativeSurfaceView *surfaceView;
@property(nonatomic,strong) NativeGameControls *gameControls;
@property(nonatomic,strong) GCController *activeController;
@property(nonatomic,strong) UIStackView *mainStack;
@property(nonatomic,strong) UIScrollView *mainScroll;
@property(nonatomic,strong) NSLayoutConstraint *gameHeight;
@property(nonatomic,strong) NSLayoutConstraint *gameMaxHeight;
@property(nonatomic,strong) NSArray<NSLayoutConstraint *> *immersiveConstraints;
@property(nonatomic,strong) UISwitch *fullscreenSwitch;
@property(nonatomic,strong) UISlider *deadzoneSlider;
@property(nonatomic,strong) UISlider *sensitivitySlider;
@property(nonatomic,strong) UISlider *opacitySlider;
@property(nonatomic,strong) UISlider *sizeSlider;
@property(nonatomic,assign) BOOL immersive;
@property(nonatomic,assign) BOOL sceneRunning;
@property(nonatomic,strong) NSTimer *startupTimer;
@property(nonatomic,strong) UILabel *performanceLabel;
@property(nonatomic,strong) UISwitch *timingSwitch;
@property(nonatomic,strong) UISwitch *profileSwitch;
@property(nonatomic,strong) UISwitch *shadowCompatibilitySwitch;
@property(nonatomic,strong) NSURL *gameFolder;
@property(nonatomic,strong) UILabel *controllerLabel;
@property(nonatomic,assign) BOOL busy;
@property(nonatomic,strong) NSLock *logLock;
@property(nonatomic,strong) NSMutableString *pendingLog;
@property(nonatomic,strong) NSMutableString *shadowFailureLog;
@property(nonatomic,assign) BOOL logScheduled;
@property(nonatomic,assign) NSTimeInterval lastPersist;
@property(nonatomic,strong) UILabel *statusLabel;
@property(nonatomic,strong) UILabel *folderLabel;
@property(nonatomic,weak) UILabel *filesStatusLabel;
@property(nonatomic,weak) UIButton *filesPageButton;
@property(nonatomic,copy) NSString *runtimeStatus;
@property(nonatomic,assign) BOOL folderValidated;
@property(nonatomic,assign) BOOL optionsPresented;
@property(nonatomic,assign) BOOL sessionLive;
@property(nonatomic,assign) BOOL hasFatal;
@property(nonatomic,strong) UINavigationController *settingsNavigation;
@property(nonatomic,strong) UIButton *defaultsButton;
- (void)beginReport:(NSString *)text;
- (void)inputResumed:(NSNotification *)note;
- (void)runScene;
- (UIButton *)button:(NSString *)title action:(SEL)action;
- (UISlider *)inputSlider:(NSString *)label key:(NSString *)key initial:(float)value min:(float)minimum max:(float)maximum stack:(UIStackView *)stack;
- (void)inputSettingsChanged;
- (void)refreshControllers;
- (void)fullscreenChanged;
- (void)stopScene;
- (void)append:(NSString *)text;
- (void)queueProgress:(NSString *)text;
- (void)appendShadowFailure;
@end
@interface ProbeController (NativeDashboard)
- (void)exportLogRecovery:(BOOL)recovery;
- (void)exportRecovery;
- (void)buildDashboard;
- (void)refreshDashboard;
- (void)populateOptions:(NSUInteger)page stack:(UIStackView *)stack;
- (void)showOptionsPage:(NSUInteger)page;
- (void)closeOptions;
- (void)applyDefaultSettings;
- (void)preferenceSwitchChanged:(UISwitch *)sender;
@end
#include "ui/NativeDashboard.inc"
static void RuntimeProgress(void *context,const char *line) {@autoreleasepool {
    ProbeController *controller=(__bridge ProbeController *)context;
    NSString *text=[NSString stringWithUTF8String:line]?:@"Invalid runtime log";
    native_journal_capture("report",line,strlen(line));
    [controller queueProgress:text];
}}
@implementation ProbeController
- (UIButton *)button:(NSString *)title action:(SEL)action {
    UIButton *button=[UIButton buttonWithType:UIButtonTypeSystem];
    UIButtonConfiguration *config=[UIButtonConfiguration filledButtonConfiguration];config.title=title;button.configuration=config;
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];return button;
}
- (void)viewDidLoad {[super viewDidLoad];[self buildDashboard];}
- (void)dealloc {self.activeController.extendedGamepad.valueChangedHandler=nil;[NSNotificationCenter.defaultCenter removeObserver:self];ni_end();}
- (void)setWorking:(BOOL)working {if(working){native_profile_set_enabled(false);self.hasFatal=NO;self.sessionLive=NO;self.runtimeStatus=self.sceneRunning?@"Preparing engine…":@"Checking…";[self.logLock lock];self.shadowFailureLog=nil;[self.logLock unlock];}else if(!self.hasFatal)self.runtimeStatus=@"Ready";if(!working&&!self.sceneRunning){ni_end();self.gameControls.playing=NO;[self applyFullscreen];}self.busy=working;self.shadowCompatibilitySwitch.enabled=!working;self.timingSwitch.enabled=!working;self.profileSwitch.enabled=!working;self.runButton.enabled=!working;self.filesButton.enabled=!working;self.runtimeButton.enabled=!working;self.rendererButton.enabled=!working;self.integrationButton.enabled=!working;self.sceneButton.enabled=!working;self.stopButton.enabled=working&&self.sceneRunning;self.sceneTimingButton.enabled=working&&self.sceneRunning&&self.timingSwitch.isOn;if(!working){[NSUserDefaults.standardUserDefaults setObject:self.report.storedText forKey:@"lastReport"];if(self.sessionLog){NativeSessionLog *log=self.sessionLog;NativeJournalActivate(nil);dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{[log finish];});}}[self refreshDashboard];}
- (void)beginReport:(NSString *)text {
    NativeJournalActivate(nil);NativeSessionLog *old=self.sessionLog;if(old)dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{[old finish];});self.sessionLog=nil;self.lastLogURL=nil;[NSUserDefaults.standardUserDefaults removeObjectForKey:@"lastSessionLogFile"];
    NSURL *documents=[NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;NSURL *directory=[documents URLByAppendingPathComponent:@"NativeLogs" isDirectory:YES];NSError *error=nil;
    if([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:@{NSFileProtectionKey:NSFileProtectionCompleteUntilFirstUserAuthentication} error:&error]){
        [directory setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];NSString *name=[NSString stringWithFormat:@"GTA-Native-0.38-%@.txt",NSUUID.UUID.UUIDString];NSURL *URL=[directory URLByAppendingPathComponent:name];self.sessionLog=[[NativeSessionLog alloc]initWithURL:URL error:&error];if(self.sessionLog){self.lastLogURL=URL;[NSUserDefaults.standardUserDefaults setObject:name forKey:@"lastSessionLogFile"];NativeJournalActivate(self.sessionLog);[self.sessionLog appendText:text category:"session"];}
    }
    self.report.storedText=error?[text stringByAppendingFormat:@"\nLog storage unavailable: %@. The preview is incomplete.",error.localizedDescription]:text;
}
- (void)flushProgress {
    [self.logLock lock];NSString *text=[self.pendingLog copy];[self.pendingLog setString:@""];self.logScheduled=NO;[self.logLock unlock];if(text.length)[self append:text];
}
- (void)queueProgress:(NSString *)text {
    [self.logLock lock];if(NativeShadowFailureLine(text)){if(!self.shadowFailureLog)self.shadowFailureLog=[NSMutableString string];NativeShadowFailureAppend(self.shadowFailureLog,text);}[self.pendingLog appendFormat:@"\n%@",text];if(self.pendingLog.length>16384)[self.pendingLog deleteCharactersInRange:NSMakeRange(0,self.pendingLog.length-16384)];BOOL schedule=!self.logScheduled;self.logScheduled=YES;[self.logLock unlock];
    if(schedule)dispatch_after(dispatch_time(DISPATCH_TIME_NOW,50*NSEC_PER_MSEC),dispatch_get_main_queue(),^{[self flushProgress];});
}
- (void)appendShadowFailure {
    [self.logLock lock];NSString *text=NativeShadowFailureReport(self.shadowFailureLog);self.shadowFailureLog=nil;[self.logLock unlock];if(text.length)[self append:text];
}
- (void)append:(NSString *)text {
    const char *logBytes=text.UTF8String;if(logBytes)native_journal_capture("preview/status",logBytes,strlen(logBytes));
    if(!self.hasFatal){NSRange fatal=[text rangeOfString:@"Native startup stopped (trap"];NSRange fail=[text rangeOfString:@"FAIL:"];NSRange range=fatal.location!=NSNotFound?fatal:fail;if(range.location!=NSNotFound){self.hasFatal=YES;NSString *reason=[[text substringFromIndex:range.location] componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet].firstObject;self.runtimeStatus=[NSString stringWithFormat:@"%@ · See Diagnostics",[reason substringToIndex:MIN((NSUInteger)220,reason.length)]];[self refreshDashboard];}}
    if(self.sceneRunning&&!self.hasFatal){NSString *status=nil;if([text containsString:@"Session transition: camera scene event"]){self.sessionLive=YES;status=@"Engine session active";}else if(!self.sessionLive&&[text containsString:@"Entering actual graphics-enabled main"])status=@"Starting game systems…";else if(!self.sessionLive&&[text containsString:@"Local asset index:"])status=@"Reading game files…";if(status&&![status isEqualToString:self.runtimeStatus]){self.runtimeStatus=status;[self refreshDashboard];}}
    NSString *joined=[self.report.storedText stringByAppendingFormat:@"\n%@",text];if(joined.length>32768)joined=[@"Recent preview only. Share Full Log saves the complete file.\n" stringByAppendingString:[joined substringFromIndex:joined.length-24576]];self.report.storedText=joined;
    NSTimeInterval now=NSProcessInfo.processInfo.systemUptime;if(now-self.lastPersist>=1){[NSUserDefaults.standardUserDefaults setObject:joined forKey:@"lastReport"];self.lastPersist=now;}
}
- (void)controllerChanged:(NSNotification *)notification {
    void (^change)(void)=^{
        /* A rapid reconnect can already be in GCController.controllers when
           a queued disconnect is delivered. Release the old source first,
           even if the framework reuses the controller object. */
        if([notification.name isEqualToString:GCControllerDidDisconnectNotification]&&notification.object==self.activeController){self.activeController.extendedGamepad.valueChangedHandler=nil;self.activeController=nil;ni_connect(false);[self.gameControls updateSource];}
        [self refreshControllers];
    };if(NSThread.isMainThread)change();else dispatch_async(dispatch_get_main_queue(),change);
}
- (void)refreshControllers {
    NSArray<GCController *> *controllers=GCController.controllers;GCController *controller=nil;
    if(self.activeController&&[controllers containsObject:self.activeController])controller=self.activeController;
    if(!controller)for(GCController *candidate in controllers)if(candidate.extendedGamepad){controller=candidate;break;}
    if(controller==self.activeController){if(!controller)self.controllerLabel.text=@"Controller: none. Pair in iPhone Settings → Bluetooth.";[self.gameControls updateSource];return;}
    self.activeController.extendedGamepad.valueChangedHandler=nil;ni_connect(false);self.activeController=controller;ni_connect(controller!=nil);
    self.controllerLabel.text=controller?[NSString stringWithFormat:@"Controller: %@. Use a control to switch; touch the game to return to touch controls.",controller.vendorName?:@"connected"]:@"Controller: none. Pair in iPhone Settings → Bluetooth.";
    if(controller){controller.handlerQueue=dispatch_get_main_queue();__weak ProbeController *weakSelf=self;
        controller.extendedGamepad.valueChangedHandler=^(GCExtendedGamepad *pad,GCControllerElement *element){ProbeController *owner=weakSelf;if(!owner||owner.activeController.extendedGamepad!=pad)return;NISource before=ni_source();NISource after=ni_controller(NativeControllerState(pad));if(before!=after)[owner.gameControls updateSource];};
        ni_controller(NativeControllerState(controller.extendedGamepad));
    }[self.gameControls updateSource];
}
- (UISlider *)inputSlider:(NSString *)label key:(NSString *)key initial:(float)value min:(float)minimum max:(float)maximum stack:(UIStackView *)stack {
    UILabel *text=[[UILabel alloc]init];text.text=label;text.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];[stack addArrangedSubview:text];UISlider *slider=[[UISlider alloc]init];slider.minimumValue=minimum;slider.maximumValue=maximum;NSNumber *saved=[NSUserDefaults.standardUserDefaults objectForKey:key];slider.value=saved?saved.floatValue:value;slider.accessibilityLabel=label;[slider addTarget:self action:@selector(inputSettingsChanged) forControlEvents:UIControlEventValueChanged];[stack addArrangedSubview:slider];return slider;
}
- (void)inputSettingsChanged {
    ni_configure(self.deadzoneSlider.value,self.sensitivitySlider.value);self.gameControls.overlayOpacity=self.opacitySlider.value;self.gameControls.stickScale=self.sizeSlider.value;
    NSUserDefaults *d=NSUserDefaults.standardUserDefaults;[d setFloat:self.deadzoneSlider.value forKey:@"inputDeadzone"];[d setFloat:self.sensitivitySlider.value forKey:@"inputSensitivity"];[d setFloat:self.opacitySlider.value forKey:@"inputOpacity"];[d setFloat:self.sizeSlider.value forKey:@"inputSize"];
    [self.gameControls clearTouches];[self.gameControls setNeedsLayout];[self.gameControls updateSource];
}
- (void)inputInterrupted:(NSNotification *)note {native_scene_suspend(true);ni_focus(false);[self.gameControls clearTouches];[self.gameControls updateSource];}
- (void)inputResumed:(NSNotification *)note {if(self.optionsPresented||UIApplication.sharedApplication.applicationState!=UIApplicationStateActive)return;native_scene_suspend(false);if(self.sceneRunning){ni_focus(true);if(self.activeController)ni_controller(NativeControllerState(self.activeController.extendedGamepad));[self.gameControls updateSource];}}
- (BOOL)prefersStatusBarHidden {return self.immersive;}
- (BOOL)prefersHomeIndicatorAutoHidden {return self.immersive;}
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {return self.immersive?UIRectEdgeBottom:UIRectEdgeNone;}
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {return self.immersive?UIInterfaceOrientationMaskLandscape:UIInterfaceOrientationMaskAllButUpsideDown;}
- (void)fullscreenChanged {[NSUserDefaults.standardUserDefaults setBool:self.fullscreenSwitch.isOn forKey:@"inputFullscreen"];[self applyFullscreen];}
- (void)applyFullscreen {
    BOOL full=self.sceneRunning&&self.fullscreenSwitch.isOn;if(full==self.immersive)return;[self.gameControls clearTouches];self.immersive=full;self.gameControls.layer.cornerRadius=full?0:16;
    if(full){[self.mainStack removeArrangedSubview:self.gameControls];self.gameMaxHeight.active=NO;[self.gameControls removeFromSuperview];self.gameHeight.active=NO;[self.view addSubview:self.gameControls];self.gameControls.translatesAutoresizingMaskIntoConstraints=NO;self.immersiveConstraints=@[[self.gameControls.topAnchor constraintEqualToAnchor:self.view.topAnchor],[self.gameControls.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],[self.gameControls.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],[self.gameControls.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor]];[NSLayoutConstraint activateConstraints:self.immersiveConstraints];self.mainScroll.hidden=YES;}
    else{[NSLayoutConstraint deactivateConstraints:self.immersiveConstraints];self.immersiveConstraints=nil;[self.gameControls removeFromSuperview];[self.mainStack insertArrangedSubview:self.gameControls atIndex:2];self.gameHeight.active=YES;self.gameMaxHeight.active=YES;self.mainScroll.hidden=NO;}
    [self setNeedsStatusBarAppearanceUpdate];[self setNeedsUpdateOfHomeIndicatorAutoHidden];[self setNeedsUpdateOfScreenEdgesDeferringSystemGestures];[self setNeedsUpdateOfSupportedInterfaceOrientations];[self.view setNeedsLayout];
    if(full){UIWindowScene *scene=self.view.window.windowScene;UIWindowSceneGeometryPreferencesIOS *p=[[UIWindowSceneGeometryPreferencesIOS alloc]initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscape];__weak ProbeController *weakSelf=self;[scene requestGeometryUpdateWithPreferences:p errorHandler:^(NSError *error){dispatch_async(dispatch_get_main_queue(),^{[weakSelf append:[NSString stringWithFormat:@"Landscape request: %@. Aspect-fit viewport remains active.",error.localizedDescription]];});}];}
}
- (void)gameSettings {if(self.sceneRunning)[self showOptionsPage:0];}
- (void)markSceneTiming {NativeDisplayMetrics *metrics=((NativeMeasuredLayer *)self.surfaceView.layer).displayMetrics;if(self.sceneRunning&&metrics){[metrics markScene];[self append:@"User marked visible 3D scene: displayed-frame window restarted. Loading is excluded only by this user marker; correctness and controls still need observation."];}}

- (void)runProbe {
    if(self.busy)return;[self setWorking:YES];
    [self beginReport:[NSString stringWithFormat:@"Native prototype 0.4 — basic test\niOS %@ / %@\n%@\nStarting actual engine instance…",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint()]];
    [NSUserDefaults.standardUserDefaults setObject:self.report.storedText forKey:@"lastReport"];
    [NSUserDefaults.standardUserDefaults setBool:YES forKey:@"probeRunning"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{@autoreleasepool{
        char report[2048]={0};int code=native_engine_probe(report,sizeof(report));
        NSString *result=[NSString stringWithUTF8String:report]?:@"FAIL: invalid report";
        NSString *memory=Footprint();NSString *metal=[self testMetal];
        dispatch_async(dispatch_get_main_queue(),^{
            [self flushProgress];[self append:[NSString stringWithFormat:@"%@\nExit: %d\n%@\n%@",result,code,memory,metal]];
            [NSUserDefaults.standardUserDefaults setBool:NO forKey:@"probeRunning"];[self setWorking:NO];
        });
    }});
}
- (void)startupStatus {
    if(!self.sceneRunning)return;NSProcessInfoThermalState thermal=NSProcessInfo.processInfo.thermalState;NSString *thermalName=thermal==NSProcessInfoThermalStateNominal?@"nominal":thermal==NSProcessInfoThermalStateFair?@"fair":thermal==NSProcessInfoThermalStateSerious?@"serious":thermal==NSProcessInfoThermalStateCritical?@"critical":@"unknown";[self append:[NSString stringWithFormat:@"Thermal state: %@; diagnostic only, no quality or clock override. Production frame cap remains 30 FPS.",thermalName]];NIMetrics input;ni_metrics(&input);[self append:[NSString stringWithFormat:@"Input: %@; events %llu, engine updates %llu, switches %llu, queued %u, overflows %llu; oldest-coalesced event-to-hook mean/max %.3f/%.3f ms (%llu samples; excludes Bluetooth/UI latency).",input.source==NI_CONTROLLER?@"controller":@"touch",(unsigned long long)input.events,(unsigned long long)input.consumed,(unsigned long long)input.switches,input.queued,(unsigned long long)input.overflows,input.mean_ms,input.max_ms,(unsigned long long)input.samples]];[self append:[NSString stringWithFormat:@"Input queue: %llu obsolete analog-only snapshots folded into later states; digital press/release edges retained. Render/simulation stalls still delay the engine hook.",(unsigned long long)input.analog_skipped]];char status[1100]={0};native_scene_status(status,sizeof(status));
    [self append:[NSString stringWithFormat:@"%s; %@",status,Footprint()]];if(self.sessionLog)[self append:[self.sessionLog summary]];NativeDisplayMetrics *metrics=((NativeMeasuredLayer *)self.surfaceView.layer).displayMetrics;if(metrics){self.performanceLabel.text=[metrics overlay];[self append:[metrics summary]];}
}
- (void)stopScene {if(self.sceneRunning){self.runtimeStatus=@"Stopping…";[self refreshDashboard];ni_end();self.gameControls.playing=NO;native_scene_request_stop();self.stopButton.enabled=NO;[self startupStatus];[self append:@"Stopping native startup and joining workers…"];}}
- (void)runIntegration {
    if(self.busy)return;if(!self.gameFolder){[self append:@"Select the extracted web folder first."];return;}
    NSURL *support=[NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;NSURL *saves=[support URLByAppendingPathComponent:@"NativeRuntimeProbe" isDirectory:YES];NSError *error=nil;
    if(!saves||![NSFileManager.defaultManager createDirectoryAtURL:saves withIntermediateDirectories:YES attributes:nil error:&error]){[self append:@"FAIL: integration save folder unavailable"];return;}
    [self setWorking:YES];[self beginReport:[NSString stringWithFormat:@"Native integration 0.38 — queue/startup checkpoint\niOS %@ / %@\n%@\nGame folder: %@\nChecking signed render targets, original startup shader draws, native BC sampling and the shared GPU stream…",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint(),self.gameFolder.lastPathComponent]];[NSUserDefaults.standardUserDefaults setObject:self.report.storedText forKey:@"lastReport"];[NSUserDefaults.standardUserDefaults setBool:YES forKey:@"probeRunning"];NSURL *folder=self.gameFolder;
    NSThread *thread=[[NSThread alloc]initWithBlock:^{@autoreleasepool{
        char report[16384]={0};int code=native_gpu_replay_probe(report,sizeof(report));RuntimeProgress((__bridge void*)self,report);
        if(code==0){BOOL scoped=[folder startAccessingSecurityScopedResource];code=native_graphics_checkpoint(folder.fileSystemRepresentation,saves.fileSystemRepresentation,RuntimeProgress,(__bridge void*)self,report,sizeof(report));if(scoped)[folder stopAccessingSecurityScopedResource];}
        NSString *result=[NSString stringWithUTF8String:report]?:@"FAIL: invalid integration report";NSString *memory=Footprint();dispatch_async(dispatch_get_main_queue(),^{[self flushProgress];[self append:[NSString stringWithFormat:@"%@\nExit: %d\nAfter integration cleanup: %@",result,code,memory]];[NSUserDefaults.standardUserDefaults setBool:NO forKey:@"probeRunning"];[self setWorking:NO];});
    }}];thread.stackSize=4*1024*1024;thread.name=@"Native GPU integration checkpoint";thread.qualityOfService=NSQualityOfServiceUserInitiated;[thread start];
}
- (void)runScene {
    if(self.busy)return;if(!self.gameFolder){[self append:@"Select the extracted web folder first."];return;}
    BOOL shadowCompatibility=self.shadowCompatibilitySwitch.isOn;
    native_scene_reset_stop();self.sceneRunning=YES;ni_begin();if(self.activeController)ni_controller(NativeControllerState(self.activeController.extendedGamepad));self.gameControls.playing=YES;[self setWorking:YES];native_profile_set_enabled(self.profileSwitch.isOn);self.gameControls.hidden=NO;[self applyFullscreen];[self.view layoutIfNeeded];if(!self.immersive)[self.mainScroll scrollRectToVisible:[self.gameControls convertRect:self.gameControls.bounds toView:self.mainScroll] animated:NO];
    self.startupTimer=[NSTimer scheduledTimerWithTimeInterval:5 target:self selector:@selector(startupStatus) userInfo:nil repeats:YES];
    NativeMeasuredLayer *layer=(NativeMeasuredLayer *)self.surfaceView.layer;layer.displayMetrics=self.timingSwitch.isOn?[[NativeDisplayMetrics alloc]init]:nil;self.timingSwitch.enabled=NO;self.performanceLabel.text=self.timingSwitch.isOn?@"Waiting for actual displayed drawables…":@"Displayed-frame diagnostics disabled.";layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=YES;layer.drawableSize=CGSizeMake(640,360);layer.contentsGravity=kCAGravityResizeAspect;
    NSURL *support=[NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;NSURL *saves=[support URLByAppendingPathComponent:@"NativeRuntimeProbe" isDirectory:YES];NSError *error=nil;
    if(!saves||![NSFileManager.defaultManager createDirectoryAtURL:saves withIntermediateDirectories:YES attributes:nil error:&error]){self.sceneRunning=NO;self.timingSwitch.enabled=YES;[self.startupTimer invalidate];self.startupTimer=nil;[self setWorking:NO];[self append:@"FAIL: native save folder unavailable"];return;}
    [self beginReport:[NSString stringWithFormat:@"Native integration 0.38 — stage 3 in progress\niOS %@ / %@\n%@\nGame folder: %@\nStarting real engine, archive reader and native graphics…\nOptional reference empty-shadow compatibility: %@\nOptional service CPU profiling: %@ (no GPU timestamps or simulation CPU hook)",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint(),self.gameFolder.lastPathComponent,shadowCompatibility?@"ON (producer intent and iPhone shadows unverified)":@"OFF (strict)",self.profileSwitch.isOn?@"ON":@"OFF"]];
    [NSUserDefaults.standardUserDefaults setObject:self.report.storedText forKey:@"lastReport"];[NSUserDefaults.standardUserDefaults setBool:YES forKey:@"probeRunning"];NSURL *folder=self.gameFolder;
    NSThread *thread=[[NSThread alloc]initWithBlock:^{@autoreleasepool{
        BOOL scoped=[folder startAccessingSecurityScopedResource];char report[4096]={0};int code=native_scene_probe_options(folder.fileSystemRepresentation,saves.fileSystemRepresentation,(__bridge void*)layer,shadowCompatibility,RuntimeProgress,(__bridge void*)self,report,sizeof(report));if(scoped)[folder stopAccessingSecurityScopedResource];
        NSString *result=[NSString stringWithUTF8String:report]?:@"FAIL: invalid startup report";NSString *memory=Footprint();
        dispatch_async(dispatch_get_main_queue(),^{[self flushProgress];if(layer.displayMetrics){self.performanceLabel.text=[layer.displayMetrics overlay];[self append:[layer.displayMetrics summary]];}[self startupStatus];ni_end();self.gameControls.playing=NO;self.timingSwitch.enabled=YES;if(code)[self appendShadowFailure];[self append:[NSString stringWithFormat:@"%@\nExit: %d\nAfter startup cleanup: %@",result,code,memory]];[NSUserDefaults.standardUserDefaults setBool:NO forKey:@"probeRunning"];self.sceneRunning=NO;[self.startupTimer invalidate];self.startupTimer=nil;[self setWorking:NO];});
    }}];thread.stackSize=4*1024*1024;thread.name=@"Native scene startup";thread.qualityOfService=NSQualityOfServiceUserInitiated;[thread start];
}
- (void)runRenderer {
    if(self.busy)return;[self setWorking:YES];
    [self beginReport:[NSString stringWithFormat:@"Native renderer feasibility 0.5\niOS %@ / %@\n%@\nCompiling actual game shaders and checking offscreen draws…",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint()]];
    [NSUserDefaults.standardUserDefaults setObject:self.report.storedText forKey:@"lastReport"];
    [NSUserDefaults.standardUserDefaults setBool:YES forKey:@"probeRunning"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{@autoreleasepool{
        char report[4096]={0};int code=native_renderer_probe(report,sizeof(report),NULL,0);
        NSString *result=[NSString stringWithUTF8String:report]?:@"FAIL: invalid renderer report";NSString *memory=Footprint();
        dispatch_async(dispatch_get_main_queue(),^{[self flushProgress];[self append:[NSString stringWithFormat:@"%@\nExit: %d\nAfter renderer cleanup: %@",result,code,memory]];[NSUserDefaults.standardUserDefaults setBool:NO forKey:@"probeRunning"];[self setWorking:NO];});
    }});
}
- (void)runRuntime {
    if(self.busy)return;
    if(!self.gameFolder){[self append:@"Select the extracted web folder before running this checkpoint."];return;}
    NSURL *support=[NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;
    NSURL *saves=[support URLByAppendingPathComponent:@"NativeRuntimeProbe" isDirectory:YES];
    NSError *error=nil;
    if(!saves||![NSFileManager.defaultManager createDirectoryAtURL:saves withIntermediateDirectories:YES attributes:nil error:&error]){[self flushProgress];[self append:[NSString stringWithFormat:@"FAIL: app save folder — %@",error.localizedDescription]];return;}
    [self setWorking:YES];
    [self beginReport:[NSString stringWithFormat:@"Native prototype 0.4 — asset/worker checkpoint\niOS %@ / %@\n%@\nGame folder: %@",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint(),self.gameFolder.lastPathComponent?:@"not selected; asset checks will be skipped"]];
    [NSUserDefaults.standardUserDefaults setObject:self.report.storedText forKey:@"lastReport"];
    [NSUserDefaults.standardUserDefaults setBool:YES forKey:@"probeRunning"];
    NSURL *folder=self.gameFolder;
    NSThread *thread=[[NSThread alloc]initWithBlock:^{@autoreleasepool{
        BOOL scoped=folder?[folder startAccessingSecurityScopedResource]:NO;
        char report[4096]={0};
        int code=native_runtime_probe(folder?folder.fileSystemRepresentation:NULL,saves.fileSystemRepresentation,RuntimeProgress,(__bridge void *)self,report,sizeof(report));
        if(scoped)[folder stopAccessingSecurityScopedResource];
        NSString *result=[NSString stringWithUTF8String:report]?:@"FAIL: invalid runtime report";
        NSString *memory=Footprint();
        dispatch_async(dispatch_get_main_queue(),^{
            [self flushProgress];[self append:[NSString stringWithFormat:@"%@\nExit: %d\nAfter engine cleanup: %@",result,code,memory]];
            [NSUserDefaults.standardUserDefaults setBool:NO forKey:@"probeRunning"];[self setWorking:NO];
        });
    }}];
    // Use the same explicit C stack budget as the Mac runtime checkpoint.
    thread.stackSize=4*1024*1024;thread.name=@"Native runtime checkpoint";
    thread.qualityOfService=NSQualityOfServiceUserInitiated;[thread start];
}
- (NSString *)testMetal {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();if(!device)return @"FAIL: no Metal device";
    id<MTLCommandQueue> queue=[device newCommandQueue];id<MTLBuffer> buffer=[device newBufferWithLength:65536 options:MTLResourceStorageModeShared];
    if(!queue||!buffer)return @"FAIL: Metal resource allocation";
    id<MTLCommandBuffer> command=[queue commandBuffer];id<MTLBlitCommandEncoder> encoder=[command blitCommandEncoder];
    if(!command||!encoder)return @"FAIL: Metal command creation";
    [encoder fillBuffer:buffer range:NSMakeRange(0,65536) value:0x5a];[encoder endEncoding];[command commit];[command waitUntilCompleted];
    if(command.status!=MTLCommandBufferStatusCompleted)return [NSString stringWithFormat:@"FAIL: Metal %@",command.error.localizedDescription];
    const unsigned char *p=buffer.contents;for(NSUInteger i=0;i<65536;i++)if(p[i]!=0x5a)return @"FAIL: Metal buffer readback differs";
    return [NSString stringWithFormat:@"PASS: Metal GPU fill and CPU readback (%@). This checks GPU access, not game rendering.",device.name];
}
- (void)selectFolder {
    if(self.busy)return;
    UIDocumentPickerViewController *picker=[[UIDocumentPickerViewController alloc]initForOpeningContentTypes:@[UTTypeFolder] asCopy:NO];picker.delegate=self;picker.allowsMultipleSelection=NO;[(self.settingsNavigation.topViewController?:self) presentViewController:picker animated:YES completion:nil];
}
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *folder=urls.firstObject;if(!folder||self.busy)return;self.gameFolder=folder;self.folderValidated=NO;NSError *bookmarkError=nil;NSData *bookmark=[folder bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:&bookmarkError];if(bookmark)[NSUserDefaults.standardUserDefaults setObject:bookmark forKey:@"gameFolderBookmark"];else [NSUserDefaults.standardUserDefaults removeObjectForKey:@"gameFolderBookmark"];[self setWorking:YES];
    [self append:@"Checking three local game files with 64 KiB reads. Files stored in iCloud may need to download first."];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{@autoreleasepool{
        BOOL scoped=[folder startAccessingSecurityScopedResource];NSMutableString *report=[NSMutableString string];
        NSData *metadata=[NSData dataWithContentsOfURL:[NSBundle.mainBundle URLForResource:@"file-probes" withExtension:@"json"]];
        NSArray *samples=metadata?[NSJSONSerialization JSONObjectWithData:metadata options:0 error:nil]:nil;
        if(samples.count!=3)[report appendString:@"FAIL: bundled sample manifest\n"];
        for(NSDictionary *sample in samples){
            NSURL *file=[folder URLByAppendingPathComponent:sample[@"path"]];
            NSFileCoordinator *coordinator=[[NSFileCoordinator alloc]initWithFilePresenter:nil];NSError *coordError=nil;
            [coordinator coordinateReadingItemAtURL:file options:0 error:&coordError byAccessor:^(NSURL *available){
                NSError *error=nil;NSNumber *size=nil;[available getResourceValue:&size forKey:NSURLFileSizeKey error:&error];
                if(error||size.unsignedLongLongValue!=[sample[@"size"] unsignedLongLongValue]){[report appendFormat:@"FAIL: size/missing %@\n",sample[@"path"]];return;}
                NSFileHandle *handle=[NSFileHandle fileHandleForReadingFromURL:available error:&error];
                NSMutableData *bytes=[NSMutableData dataWithCapacity:65536];
                while(handle&&!error&&bytes.length<65536){NSData *part=[handle readDataUpToLength:65536-bytes.length error:&error];if(part.length==0)break;[bytes appendData:part];}
                [handle closeAndReturnError:nil];
                BOOL valid=bytes.length==65536&&!error&&[Digest(bytes) isEqualToString:sample[@"prefixSHA256"]];
                [report appendFormat:@"%@: %@\n",valid?@"PASS":@"FAIL: read/hash",sample[@"path"]];
            }];
            if(coordError)[report appendFormat:@"FAIL: %@ — %@\n",sample[@"path"],coordError.localizedDescription];
        }
        if(scoped)[folder stopAccessingSecurityScopedResource];
        [report appendString:@"Sample check only; this does not validate or install the complete game.\nFolder selected. Return to Start Game, or open Advanced for more checks."];
        dispatch_async(dispatch_get_main_queue(),^{self.folderValidated=![report containsString:@"FAIL:"];[self append:report];[self setWorking:NO];});
    }});
}
@end
@interface ProbeSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic,strong) UIWindow *window;
@end
@implementation ProbeSceneDelegate
- (void)sceneWillResignActive:(UIScene *)scene {ni_focus(false);}
- (void)sceneDidEnterBackground:(UIScene *)scene {ni_end();native_scene_request_stop();}
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options {
    if(![scene isKindOfClass:UIWindowScene.class])return;
    self.window=[[UIWindow alloc]initWithWindowScene:(UIWindowScene *)scene];self.window.rootViewController=[[ProbeController alloc]init];[self.window makeKeyAndVisible];
}
@end
@interface AppDelegate : UIResponder <UIApplicationDelegate>
@end
@implementation AppDelegate
@end
int main(int argc,char **argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(AppDelegate.class));}}
