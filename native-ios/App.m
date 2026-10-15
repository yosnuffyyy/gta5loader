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
#include "renderer/presentation_metrics.inc"

@interface NativeSurfaceView : UIView
@end
@implementation NativeSurfaceView
+ (Class)layerClass {return NativeMeasuredLayer.class;}
@end
#include "input/NativeControls.inc"

static NSString *Footprint(void) {
    task_vm_info_data_t info={0};mach_msg_type_number_t count=TASK_VM_INFO_COUNT;
    if(task_info(mach_task_self(),TASK_VM_INFO,(task_info_t)&info,&count)!=KERN_SUCCESS)return @"Memory measurement unavailable";
    return [NSString stringWithFormat:@"Process footprint: %.1f MiB",(double)info.phys_footprint/1048576.0];
}
static NSString *Digest(NSData *bytes) {
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256(bytes.bytes,(CC_LONG)bytes.length,digest);
    NSMutableString *result=[NSMutableString string];for(unsigned i=0;i<sizeof(digest);i++)[result appendFormat:@"%02x",digest[i]];return result;
}
@interface ProbeController : UIViewController <UIDocumentPickerDelegate>
@property(nonatomic,strong) UITextView *report;
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
@property(nonatomic,strong) UISwitch *shadowCompatibilitySwitch;
@property(nonatomic,strong) NSURL *gameFolder;
@property(nonatomic,strong) UILabel *controllerLabel;
@property(nonatomic,assign) BOOL busy;
@property(nonatomic,strong) NSLock *logLock;
@property(nonatomic,strong) NSMutableString *pendingLog;
@property(nonatomic,strong) NSMutableString *shadowFailureLog;
@property(nonatomic,assign) BOOL logScheduled;
@property(nonatomic,assign) NSTimeInterval lastPersist;
- (void)append:(NSString *)text;
- (void)queueProgress:(NSString *)text;
- (void)appendShadowFailure;
@end
static void RuntimeProgress(void *context,const char *line) {
    ProbeController *controller=(__bridge ProbeController *)context;
    NSString *text=[NSString stringWithUTF8String:line]?:@"Invalid runtime log";
    [controller queueProgress:text];
}
@implementation ProbeController
- (UIButton *)button:(NSString *)title action:(SEL)action {
    UIButton *button=[UIButton buttonWithType:UIButtonTypeSystem];
    UIButtonConfiguration *config=[UIButtonConfiguration filledButtonConfiguration];config.title=title;button.configuration=config;
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];return button;
}
- (void)viewDidLoad {
    [super viewDidLoad];self.logLock=[[NSLock alloc]init];self.pendingLog=[NSMutableString string];self.view.backgroundColor=UIColor.systemBackgroundColor;
    UIScrollView *scroll=[[UIScrollView alloc]init];scroll.translatesAutoresizingMaskIntoConstraints=NO;[self.view addSubview:scroll];
    UIStackView *stack=[[UIStackView alloc]init];stack.axis=UILayoutConstraintAxisVertical;stack.spacing=14;stack.translatesAutoresizingMaskIntoConstraints=NO;
    self.mainStack=stack;self.mainScroll=scroll;scroll.delaysContentTouches=NO;scroll.canCancelContentTouches=NO;
    [scroll addSubview:stack];UILayoutGuide *safe=self.view.safeAreaLayoutGuide,*content=scroll.contentLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[[scroll.topAnchor constraintEqualToAnchor:safe.topAnchor],[scroll.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],[scroll.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],[scroll.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],[stack.topAnchor constraintEqualToAnchor:content.topAnchor constant:16],[stack.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-16],[stack.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:20],[stack.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-20],[stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor constant:-40]]];
    UILabel *title=[[UILabel alloc]init];title.text=@"Native engine prototype 0.29";title.font=[UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];title.adjustsFontForContentSizeCategory=YES;title.numberOfLines=0;[stack addArrangedSubview:title];
    UILabel *description=[[UILabel alloc]init];description.text=@"Select your extracted web folder to test real archive startup and the native GPU command queue. This diagnostic reports the next startup dependency. Short Prologue scenes have rendered on iPhone. Continuous correct animation, audio output and gameplay controls are still under development.";description.numberOfLines=0;description.font=[UIFont preferredFontForTextStyle:UIFontTextStyleBody];description.adjustsFontForContentSizeCategory=YES;[stack addArrangedSubview:description];
    self.surfaceView=[[NativeSurfaceView alloc]init];self.surfaceView.backgroundColor=UIColor.blackColor;self.gameControls=[[NativeGameControls alloc]initWithSurface:self.surfaceView];self.gameControls.hidden=YES;self.gameHeight=[self.gameControls.heightAnchor constraintEqualToAnchor:self.gameControls.widthAnchor multiplier:9.0/16.0];self.gameHeight.active=YES;[stack addArrangedSubview:self.gameControls];[self.gameControls.settingsButton addTarget:self action:@selector(gameSettings) forControlEvents:UIControlEventTouchUpInside];
    UILabel *fullscreenLabel=[[UILabel alloc]init];fullscreenLabel.text=@"Fullscreen";[stack addArrangedSubview:fullscreenLabel];self.fullscreenSwitch=[[UISwitch alloc]init];self.fullscreenSwitch.on=[NSUserDefaults.standardUserDefaults boolForKey:@"inputFullscreen"];self.fullscreenSwitch.accessibilityLabel=@"Fullscreen";[self.fullscreenSwitch addTarget:self action:@selector(fullscreenChanged) forControlEvents:UIControlEventValueChanged];[stack addArrangedSubview:self.fullscreenSwitch];
    self.deadzoneSlider=[self inputSlider:@"Stick deadzone" key:@"inputDeadzone" initial:.15 min:.05 max:.4 stack:stack];
    self.sensitivitySlider=[self inputSlider:@"Camera sensitivity" key:@"inputSensitivity" initial:1 min:.25 max:2 stack:stack];
    self.opacitySlider=[self inputSlider:@"Touch opacity" key:@"inputOpacity" initial:.55 min:.15 max:1 stack:stack];
    self.sizeSlider=[self inputSlider:@"Joystick size" key:@"inputSize" initial:1 min:.75 max:1.2 stack:stack];[self inputSettingsChanged];
    self.performanceLabel=[[UILabel alloc]init];self.performanceLabel.numberOfLines=0;self.performanceLabel.font=[UIFont monospacedSystemFontOfSize:11 weight:UIFontWeightRegular];[stack addArrangedSubview:self.performanceLabel];
    self.timingSwitch=[[UISwitch alloc]init];self.timingSwitch.on=YES;self.timingSwitch.accessibilityLabel=@"Actual displayed-frame diagnostics";UILabel *timingLabel=[[UILabel alloc]init];timingLabel.text=@"Measure actual displayed frames (optional)";timingLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];[stack addArrangedSubview:timingLabel];[stack addArrangedSubview:self.timingSwitch];
    self.sceneTimingButton=[self button:@"Begin visible-scene measurement" action:@selector(markSceneTiming)];self.sceneTimingButton.enabled=NO;[stack addArrangedSubview:self.sceneTimingButton];
    self.shadowCompatibilitySwitch=[[UISwitch alloc]init];self.shadowCompatibilitySwitch.on=NO;self.shadowCompatibilitySwitch.accessibilityLabel=@"Optional reference empty-shadow compatibility";UILabel *shadowLabel=[[UILabel alloc]init];shadowLabel.text=@"Reference empty-shadow compatibility (experimental; off keeps strict validation)";shadowLabel.numberOfLines=0;shadowLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];[stack addArrangedSubview:shadowLabel];[stack addArrangedSubview:self.shadowCompatibilitySwitch];
    self.sceneButton=[self button:@"Trace Prologue startup (experimental)" action:@selector(runScene)];[stack addArrangedSubview:self.sceneButton];
    self.stopButton=[self button:@"Stop native startup" action:@selector(stopScene)];self.stopButton.enabled=NO;[stack addArrangedSubview:self.stopButton];
    self.integrationButton=[self button:@"Run GPU queue + startup tests" action:@selector(runIntegration)];[stack addArrangedSubview:self.integrationButton];
    self.rendererButton=[self button:@"Run renderer feasibility" action:@selector(runRenderer)];[stack addArrangedSubview:self.rendererButton];
    self.runButton=[self button:@"Run native engine tests" action:@selector(runProbe)];[stack addArrangedSubview:self.runButton];
    self.filesButton=[self button:@"Check local web folder" action:@selector(selectFolder)];[stack addArrangedSubview:self.filesButton];
    self.runtimeButton=[self button:@"Run asset + worker tests" action:@selector(runRuntime)];[stack addArrangedSubview:self.runtimeButton];
    self.controllerLabel=[[UILabel alloc]init];self.controllerLabel.numberOfLines=0;self.controllerLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];[stack addArrangedSubview:self.controllerLabel];
    self.report=[[UITextView alloc]init];self.report.editable=NO;self.report.font=[UIFont monospacedSystemFontOfSize:13 weight:UIFontWeightRegular];self.report.backgroundColor=UIColor.secondarySystemBackgroundColor;self.report.layer.cornerRadius=10;[stack addArrangedSubview:self.report];
    [self.report.heightAnchor constraintEqualToConstant:240].active=YES;
    [stack addArrangedSubview:[self button:@"Copy report" action:@selector(copyReport)]];
    NSUserDefaults *defaults=NSUserDefaults.standardUserDefaults;
    self.report.text=[defaults stringForKey:@"lastReport"]?:@"Ready. Run renderer feasibility to test the native graphics backend. The asset/worker checkpoint needs the extracted web folder selected first.";
    if([defaults boolForKey:@"probeRunning"])self.report.text=[@"Previous test ended without a result. The app may have been closed or terminated.\n\n" stringByAppendingString:self.report.text];
    [defaults setBool:NO forKey:@"probeRunning"];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(controllerChanged:) name:GCControllerDidConnectNotification object:nil];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(controllerChanged:) name:GCControllerDidDisconnectNotification object:nil];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(inputInterrupted:) name:UIApplicationWillResignActiveNotification object:nil];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(inputResumed:) name:UIApplicationDidBecomeActiveNotification object:nil];
    [self refreshControllers];
}
- (void)dealloc {self.activeController.extendedGamepad.valueChangedHandler=nil;[NSNotificationCenter.defaultCenter removeObserver:self];ni_end();}
- (void)setWorking:(BOOL)working {if(working){[self.logLock lock];self.shadowFailureLog=nil;[self.logLock unlock];}if(!working&&!self.sceneRunning){ni_end();self.gameControls.playing=NO;[self applyFullscreen];}self.busy=working;self.shadowCompatibilitySwitch.enabled=!working;self.runButton.enabled=!working;self.filesButton.enabled=!working;self.runtimeButton.enabled=!working;self.rendererButton.enabled=!working;self.integrationButton.enabled=!working;self.sceneButton.enabled=!working;self.stopButton.enabled=working&&self.sceneRunning;self.sceneTimingButton.enabled=working&&self.sceneRunning&&self.timingSwitch.isOn;if(!working)[NSUserDefaults.standardUserDefaults setObject:self.report.text forKey:@"lastReport"];}
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
    NSString *joined=[self.report.text stringByAppendingFormat:@"\n%@",text];if(joined.length>32768)joined=[@"Earlier startup messages omitted to bound log memory.\n" stringByAppendingString:[joined substringFromIndex:joined.length-24576]];self.report.text=joined;
    NSTimeInterval now=NSProcessInfo.processInfo.systemUptime;if(now-self.lastPersist>=1){[NSUserDefaults.standardUserDefaults setObject:joined forKey:@"lastReport"];self.lastPersist=now;}
}
- (void)controllerChanged:(NSNotification *)notification {dispatch_async(dispatch_get_main_queue(),^{[self refreshControllers];});}
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
- (void)inputInterrupted:(NSNotification *)note {ni_focus(false);[self.gameControls clearTouches];[self.gameControls updateSource];}
- (void)inputResumed:(NSNotification *)note {if(self.sceneRunning){ni_focus(true);if(self.activeController)ni_controller(NativeControllerState(self.activeController.extendedGamepad));[self.gameControls updateSource];}}
- (BOOL)prefersStatusBarHidden {return self.immersive;}
- (BOOL)prefersHomeIndicatorAutoHidden {return self.immersive;}
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {return self.immersive?UIRectEdgeBottom:UIRectEdgeNone;}
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {return self.immersive?UIInterfaceOrientationMaskLandscape:UIInterfaceOrientationMaskAllButUpsideDown;}
- (void)fullscreenChanged {[NSUserDefaults.standardUserDefaults setBool:self.fullscreenSwitch.isOn forKey:@"inputFullscreen"];[self applyFullscreen];}
- (void)applyFullscreen {
    BOOL full=self.sceneRunning&&self.fullscreenSwitch.isOn;if(full==self.immersive)return;[self.gameControls clearTouches];self.immersive=full;
    if(full){[self.mainStack removeArrangedSubview:self.gameControls];[self.gameControls removeFromSuperview];self.gameHeight.active=NO;[self.view addSubview:self.gameControls];self.gameControls.translatesAutoresizingMaskIntoConstraints=NO;self.immersiveConstraints=@[[self.gameControls.topAnchor constraintEqualToAnchor:self.view.topAnchor],[self.gameControls.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],[self.gameControls.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],[self.gameControls.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor]];[NSLayoutConstraint activateConstraints:self.immersiveConstraints];self.mainScroll.hidden=YES;}
    else{[NSLayoutConstraint deactivateConstraints:self.immersiveConstraints];self.immersiveConstraints=nil;[self.gameControls removeFromSuperview];[self.mainStack insertArrangedSubview:self.gameControls atIndex:2];self.gameHeight.active=YES;self.mainScroll.hidden=NO;}
    [self setNeedsStatusBarAppearanceUpdate];[self setNeedsUpdateOfHomeIndicatorAutoHidden];[self setNeedsUpdateOfScreenEdgesDeferringSystemGestures];[self setNeedsUpdateOfSupportedInterfaceOrientations];[self.view setNeedsLayout];
    if(full){UIWindowScene *scene=self.view.window.windowScene;UIWindowSceneGeometryPreferencesIOS *p=[[UIWindowSceneGeometryPreferencesIOS alloc]initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscape];__weak ProbeController *weakSelf=self;[scene requestGeometryUpdateWithPreferences:p errorHandler:^(NSError *error){[weakSelf append:[NSString stringWithFormat:@"Landscape request: %@. Aspect-fit viewport remains active.",error.localizedDescription]];}];}
}
- (void)gameSettings {
    if(!self.sceneRunning)return;ni_focus(false);[self.gameControls clearTouches];UIAlertController *menu=[UIAlertController alertControllerWithTitle:@"Game controls" message:@"Touch the game for touch controls; use a controller action to switch back. Pad symbols keep the game's contextual mappings. Exit fullscreen to adjust sensitivity and layout." preferredStyle:UIAlertControllerStyleActionSheet];__weak ProbeController *weakSelf=self;
    [menu addAction:[UIAlertAction actionWithTitle:self.immersive?@"Fullscreen: Off":@"Fullscreen: On" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){ProbeController *owner=weakSelf;owner.fullscreenSwitch.on=!owner.fullscreenSwitch.isOn;[owner fullscreenChanged];[owner inputResumed:nil];}]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Mark visible-scene measurement" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[weakSelf markSceneTiming];[weakSelf inputResumed:nil];}]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Stop game" style:UIAlertActionStyleDestructive handler:^(UIAlertAction *a){[weakSelf stopScene];}]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Resume controls" style:UIAlertActionStyleCancel handler:^(UIAlertAction *a){[weakSelf inputResumed:nil];}]];menu.popoverPresentationController.sourceView=self.gameControls.settingsButton;menu.popoverPresentationController.sourceRect=self.gameControls.settingsButton.bounds;[self presentViewController:menu animated:YES completion:nil];
}
- (void)markSceneTiming {NativeDisplayMetrics *metrics=((NativeMeasuredLayer *)self.surfaceView.layer).displayMetrics;if(self.sceneRunning&&metrics){[metrics markScene];[self append:@"User marked visible 3D scene: displayed-frame window restarted. Loading is excluded only by this user marker; correctness and controls still need observation."];}}
- (void)copyReport {UIPasteboard.generalPasteboard.string=self.report.text;}
- (void)runProbe {
    if(self.busy)return;[self setWorking:YES];
    self.report.text=[NSString stringWithFormat:@"Native prototype 0.4 — basic test\niOS %@ / %@\n%@\nStarting actual engine instance…",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint()];
    [NSUserDefaults.standardUserDefaults setObject:self.report.text forKey:@"lastReport"];
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
    if(!self.sceneRunning)return;NIMetrics input;ni_metrics(&input);[self append:[NSString stringWithFormat:@"Input: %@; events %llu, engine updates %llu, switches %llu, queued %u, overflows %llu; event-to-hook mean/max %.3f/%.3f ms (%llu samples; excludes Bluetooth/UI latency).",input.source==NI_CONTROLLER?@"controller":@"touch",(unsigned long long)input.events,(unsigned long long)input.consumed,(unsigned long long)input.switches,input.queued,(unsigned long long)input.overflows,input.mean_ms,input.max_ms,(unsigned long long)input.samples]];char status[640]={0};native_scene_status(status,sizeof(status));
    [self append:[NSString stringWithFormat:@"%s; %@",status,Footprint()]];NativeDisplayMetrics *metrics=((NativeMeasuredLayer *)self.surfaceView.layer).displayMetrics;if(metrics){self.performanceLabel.text=[metrics overlay];[self append:[metrics summary]];}
}
- (void)stopScene {if(self.sceneRunning){ni_end();self.gameControls.playing=NO;native_scene_request_stop();self.stopButton.enabled=NO;[self startupStatus];[self append:@"Stopping native startup and joining workers…"];}}
- (void)runIntegration {
    if(self.busy)return;if(!self.gameFolder){[self append:@"Select the extracted web folder first."];return;}
    NSURL *support=[NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;NSURL *saves=[support URLByAppendingPathComponent:@"NativeRuntimeProbe" isDirectory:YES];NSError *error=nil;
    if(!saves||![NSFileManager.defaultManager createDirectoryAtURL:saves withIntermediateDirectories:YES attributes:nil error:&error]){[self append:@"FAIL: integration save folder unavailable"];return;}
    [self setWorking:YES];self.report.text=[NSString stringWithFormat:@"Native integration 0.29 — queue/startup checkpoint\niOS %@ / %@\n%@\nGame folder: %@\nChecking signed render targets, original startup shader draws, native BC sampling and the shared GPU stream…",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint(),self.gameFolder.lastPathComponent];[NSUserDefaults.standardUserDefaults setObject:self.report.text forKey:@"lastReport"];[NSUserDefaults.standardUserDefaults setBool:YES forKey:@"probeRunning"];NSURL *folder=self.gameFolder;
    NSThread *thread=[[NSThread alloc]initWithBlock:^{@autoreleasepool{
        char report[16384]={0};int code=native_gpu_replay_probe(report,sizeof(report));RuntimeProgress((__bridge void*)self,report);
        if(code==0){BOOL scoped=[folder startAccessingSecurityScopedResource];code=native_graphics_checkpoint(folder.fileSystemRepresentation,saves.fileSystemRepresentation,RuntimeProgress,(__bridge void*)self,report,sizeof(report));if(scoped)[folder stopAccessingSecurityScopedResource];}
        NSString *result=[NSString stringWithUTF8String:report]?:@"FAIL: invalid integration report";NSString *memory=Footprint();dispatch_async(dispatch_get_main_queue(),^{[self flushProgress];[self append:[NSString stringWithFormat:@"%@\nExit: %d\nAfter integration cleanup: %@",result,code,memory]];[NSUserDefaults.standardUserDefaults setBool:NO forKey:@"probeRunning"];[self setWorking:NO];});
    }}];thread.stackSize=4*1024*1024;thread.name=@"Native GPU integration checkpoint";thread.qualityOfService=NSQualityOfServiceUserInitiated;[thread start];
}
- (void)runScene {
    if(self.busy)return;if(!self.gameFolder){[self append:@"Select the extracted web folder first."];return;}
    BOOL shadowCompatibility=self.shadowCompatibilitySwitch.isOn;
    native_scene_reset_stop();self.sceneRunning=YES;ni_begin();if(self.activeController)ni_controller(NativeControllerState(self.activeController.extendedGamepad));self.gameControls.playing=YES;[self setWorking:YES];self.gameControls.hidden=NO;[self applyFullscreen];[self.view layoutIfNeeded];
    self.startupTimer=[NSTimer scheduledTimerWithTimeInterval:5 target:self selector:@selector(startupStatus) userInfo:nil repeats:YES];
    NativeMeasuredLayer *layer=(NativeMeasuredLayer *)self.surfaceView.layer;layer.displayMetrics=self.timingSwitch.isOn?[[NativeDisplayMetrics alloc]init]:nil;self.timingSwitch.enabled=NO;self.performanceLabel.text=self.timingSwitch.isOn?@"Waiting for actual displayed drawables…":@"Displayed-frame diagnostics disabled.";layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=YES;layer.drawableSize=CGSizeMake(640,360);layer.contentsGravity=kCAGravityResizeAspect;
    NSURL *support=[NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;NSURL *saves=[support URLByAppendingPathComponent:@"NativeRuntimeProbe" isDirectory:YES];NSError *error=nil;
    if(!saves||![NSFileManager.defaultManager createDirectoryAtURL:saves withIntermediateDirectories:YES attributes:nil error:&error]){self.sceneRunning=NO;self.timingSwitch.enabled=YES;[self.startupTimer invalidate];self.startupTimer=nil;[self setWorking:NO];[self append:@"FAIL: native save folder unavailable"];return;}
    self.report.text=[NSString stringWithFormat:@"Native integration 0.29 — stage 3 in progress\niOS %@ / %@\n%@\nGame folder: %@\nStarting real engine, archive reader and native graphics…\nOptional reference empty-shadow compatibility: %@",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint(),self.gameFolder.lastPathComponent,shadowCompatibility?@"ON (producer intent and iPhone shadows unverified)":@"OFF (strict)"];
    [NSUserDefaults.standardUserDefaults setObject:self.report.text forKey:@"lastReport"];[NSUserDefaults.standardUserDefaults setBool:YES forKey:@"probeRunning"];NSURL *folder=self.gameFolder;
    NSThread *thread=[[NSThread alloc]initWithBlock:^{@autoreleasepool{
        BOOL scoped=[folder startAccessingSecurityScopedResource];char report[4096]={0};int code=native_scene_probe_options(folder.fileSystemRepresentation,saves.fileSystemRepresentation,(__bridge void*)layer,shadowCompatibility,RuntimeProgress,(__bridge void*)self,report,sizeof(report));if(scoped)[folder stopAccessingSecurityScopedResource];
        NSString *result=[NSString stringWithUTF8String:report]?:@"FAIL: invalid startup report";NSString *memory=Footprint();
        dispatch_async(dispatch_get_main_queue(),^{[self flushProgress];if(layer.displayMetrics){self.performanceLabel.text=[layer.displayMetrics overlay];[self append:[layer.displayMetrics summary]];}[self startupStatus];ni_end();self.gameControls.playing=NO;self.timingSwitch.enabled=YES;if(code)[self appendShadowFailure];[self append:[NSString stringWithFormat:@"%@\nExit: %d\nAfter startup cleanup: %@",result,code,memory]];[NSUserDefaults.standardUserDefaults setBool:NO forKey:@"probeRunning"];self.sceneRunning=NO;[self.startupTimer invalidate];self.startupTimer=nil;[self setWorking:NO];});
    }}];thread.stackSize=4*1024*1024;thread.name=@"Native scene startup";thread.qualityOfService=NSQualityOfServiceUserInitiated;[thread start];
}
- (void)runRenderer {
    if(self.busy)return;[self setWorking:YES];
    self.report.text=[NSString stringWithFormat:@"Native renderer feasibility 0.5\niOS %@ / %@\n%@\nCompiling actual game shaders and checking offscreen draws…",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint()];
    [NSUserDefaults.standardUserDefaults setObject:self.report.text forKey:@"lastReport"];
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
    self.report.text=[NSString stringWithFormat:@"Native prototype 0.4 — asset/worker checkpoint\niOS %@ / %@\n%@\nGame folder: %@",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint(),self.gameFolder.lastPathComponent?:@"not selected; asset checks will be skipped"];
    [NSUserDefaults.standardUserDefaults setObject:self.report.text forKey:@"lastReport"];
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
    UIDocumentPickerViewController *picker=[[UIDocumentPickerViewController alloc]initForOpeningContentTypes:@[UTTypeFolder] asCopy:NO];picker.delegate=self;picker.allowsMultipleSelection=NO;[self presentViewController:picker animated:YES completion:nil];
}
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *folder=urls.firstObject;if(!folder||self.busy)return;self.gameFolder=folder;[self setWorking:YES];
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
        [report appendString:@"Sample check only; this does not validate or install the complete game.\nFolder selected for runtime tests. Tap Run asset + worker tests next."];
        dispatch_async(dispatch_get_main_queue(),^{[self append:report];[self setWorking:NO];});
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
