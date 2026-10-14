#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <GameController/GameController.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <CommonCrypto/CommonDigest.h>
#import <mach/mach.h>
#import <QuartzCore/CAMetalLayer.h>
#include "probe.h"
#include "renderer/native_renderer.h"
#include "renderer/native_launch.h"

@interface NativeSurfaceView : UIView
@end
@implementation NativeSurfaceView
+ (Class)layerClass {return CAMetalLayer.class;}
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
@interface ProbeController : UIViewController <UIDocumentPickerDelegate>
@property(nonatomic,strong) UITextView *report;
@property(nonatomic,strong) UIButton *runButton;
@property(nonatomic,strong) UIButton *filesButton;
@property(nonatomic,strong) UIButton *runtimeButton;
@property(nonatomic,strong) UIButton *rendererButton;
@property(nonatomic,strong) UIButton *integrationButton;
@property(nonatomic,strong) UIButton *sceneButton;
@property(nonatomic,strong) UIButton *stopButton;
@property(nonatomic,strong) NativeSurfaceView *surfaceView;
@property(nonatomic,assign) BOOL sceneRunning;
@property(nonatomic,strong) NSTimer *startupTimer;
@property(nonatomic,strong) NSURL *gameFolder;
@property(nonatomic,strong) UILabel *controllerLabel;
@property(nonatomic,assign) BOOL busy;
@property(nonatomic,strong) NSLock *logLock;
@property(nonatomic,strong) NSMutableString *pendingLog;
@property(nonatomic,assign) BOOL logScheduled;
@property(nonatomic,assign) NSTimeInterval lastPersist;
- (void)append:(NSString *)text;
- (void)queueProgress:(NSString *)text;
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
    [scroll addSubview:stack];UILayoutGuide *safe=self.view.safeAreaLayoutGuide,*content=scroll.contentLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[[scroll.topAnchor constraintEqualToAnchor:safe.topAnchor],[scroll.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],[scroll.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],[scroll.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],[stack.topAnchor constraintEqualToAnchor:content.topAnchor constant:16],[stack.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-16],[stack.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:20],[stack.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-20],[stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor constant:-40]]];
    UILabel *title=[[UILabel alloc]init];title.text=@"Native engine prototype 0.19";title.font=[UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];title.adjustsFontForContentSizeCategory=YES;title.numberOfLines=0;[stack addArrangedSubview:title];
    UILabel *description=[[UILabel alloc]init];description.text=@"Select your extracted web folder to test real archive startup and the native GPU command queue. This diagnostic reports the next startup dependency. Scene draws, audio output and gameplay controls are still being ported.";description.numberOfLines=0;description.font=[UIFont preferredFontForTextStyle:UIFontTextStyleBody];description.adjustsFontForContentSizeCategory=YES;[stack addArrangedSubview:description];
    self.surfaceView=[[NativeSurfaceView alloc]init];self.surfaceView.backgroundColor=UIColor.blackColor;self.surfaceView.hidden=YES;[self.surfaceView.heightAnchor constraintEqualToAnchor:self.surfaceView.widthAnchor multiplier:9.0/16.0].active=YES;[stack addArrangedSubview:self.surfaceView];
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
    [self refreshControllers];
}
- (void)dealloc {[NSNotificationCenter.defaultCenter removeObserver:self];}
- (void)setWorking:(BOOL)working {self.busy=working;self.runButton.enabled=!working;self.filesButton.enabled=!working;self.runtimeButton.enabled=!working;self.rendererButton.enabled=!working;self.integrationButton.enabled=!working;self.sceneButton.enabled=!working;self.stopButton.enabled=working&&self.sceneRunning;if(!working)[NSUserDefaults.standardUserDefaults setObject:self.report.text forKey:@"lastReport"];}
- (void)flushProgress {
    [self.logLock lock];NSString *text=[self.pendingLog copy];[self.pendingLog setString:@""];self.logScheduled=NO;[self.logLock unlock];if(text.length)[self append:text];
}
- (void)queueProgress:(NSString *)text {
    [self.logLock lock];[self.pendingLog appendFormat:@"\n%@",text];if(self.pendingLog.length>16384)[self.pendingLog deleteCharactersInRange:NSMakeRange(0,self.pendingLog.length-12288)];BOOL schedule=!self.logScheduled;self.logScheduled=YES;[self.logLock unlock];
    if(schedule)dispatch_after(dispatch_time(DISPATCH_TIME_NOW,50*NSEC_PER_MSEC),dispatch_get_main_queue(),^{[self flushProgress];});
}
- (void)append:(NSString *)text {
    NSString *joined=[self.report.text stringByAppendingFormat:@"\n%@",text];if(joined.length>32768)joined=[@"Earlier startup messages omitted to bound log memory.\n" stringByAppendingString:[joined substringFromIndex:joined.length-24576]];self.report.text=joined;
    NSTimeInterval now=NSProcessInfo.processInfo.systemUptime;if(now-self.lastPersist>=1){[NSUserDefaults.standardUserDefaults setObject:joined forKey:@"lastReport"];self.lastPersist=now;}
}
- (void)controllerChanged:(NSNotification *)notification {dispatch_async(dispatch_get_main_queue(),^{[self refreshControllers];});}
- (void)refreshControllers {
    NSArray<GCController *> *controllers=GCController.controllers;
    if(controllers.count==0){self.controllerLabel.text=@"Controller: none. Pair in iPhone Settings → Bluetooth.";return;}
    GCController *controller=controllers.firstObject;self.controllerLabel.text=[NSString stringWithFormat:@"Controller: %@ — press a button to test",controller.vendorName?:@"connected"];
    __weak ProbeController *weakSelf=self;
    controller.extendedGamepad.buttonA.pressedChangedHandler=^(GCControllerButtonInput *button,float value,BOOL pressed){
        if(pressed)dispatch_async(dispatch_get_main_queue(),^{weakSelf.controllerLabel.text=@"Controller: primary face button received natively";});
    };
}
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
    if(!self.sceneRunning)return;char status[640]={0};native_scene_status(status,sizeof(status));
    [self append:[NSString stringWithFormat:@"%s; %@",status,Footprint()]];
}
- (void)stopScene {if(self.sceneRunning){native_scene_request_stop();self.stopButton.enabled=NO;[self startupStatus];[self append:@"Stopping native startup and joining workers…"];}}
- (void)runIntegration {
    if(self.busy)return;if(!self.gameFolder){[self append:@"Select the extracted web folder first."];return;}
    NSURL *support=[NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;NSURL *saves=[support URLByAppendingPathComponent:@"NativeRuntimeProbe" isDirectory:YES];NSError *error=nil;
    if(!saves||![NSFileManager.defaultManager createDirectoryAtURL:saves withIntermediateDirectories:YES attributes:nil error:&error]){[self append:@"FAIL: integration save folder unavailable"];return;}
    [self setWorking:YES];self.report.text=[NSString stringWithFormat:@"Native integration 0.19 — queue/startup checkpoint\niOS %@ / %@\n%@\nGame folder: %@\nChecking signed render targets, original startup shader draws, native BC sampling and the shared GPU stream…",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint(),self.gameFolder.lastPathComponent];[NSUserDefaults.standardUserDefaults setObject:self.report.text forKey:@"lastReport"];[NSUserDefaults.standardUserDefaults setBool:YES forKey:@"probeRunning"];NSURL *folder=self.gameFolder;
    NSThread *thread=[[NSThread alloc]initWithBlock:^{@autoreleasepool{
        char report[4096]={0};int code=native_gpu_replay_probe(report,sizeof(report));RuntimeProgress((__bridge void*)self,report);
        if(code==0){BOOL scoped=[folder startAccessingSecurityScopedResource];code=native_graphics_checkpoint(folder.fileSystemRepresentation,saves.fileSystemRepresentation,RuntimeProgress,(__bridge void*)self,report,sizeof(report));if(scoped)[folder stopAccessingSecurityScopedResource];}
        NSString *result=[NSString stringWithUTF8String:report]?:@"FAIL: invalid integration report";NSString *memory=Footprint();dispatch_async(dispatch_get_main_queue(),^{[self flushProgress];[self append:[NSString stringWithFormat:@"%@\nExit: %d\nAfter integration cleanup: %@",result,code,memory]];[NSUserDefaults.standardUserDefaults setBool:NO forKey:@"probeRunning"];[self setWorking:NO];});
    }}];thread.stackSize=4*1024*1024;thread.name=@"Native GPU integration checkpoint";thread.qualityOfService=NSQualityOfServiceUserInitiated;[thread start];
}
- (void)runScene {
    if(self.busy)return;if(!self.gameFolder){[self append:@"Select the extracted web folder first."];return;}
    native_scene_reset_stop();self.sceneRunning=YES;[self setWorking:YES];self.surfaceView.hidden=NO;[self.view layoutIfNeeded];
    self.startupTimer=[NSTimer scheduledTimerWithTimeInterval:5 target:self selector:@selector(startupStatus) userInfo:nil repeats:YES];
    CAMetalLayer *layer=(CAMetalLayer *)self.surfaceView.layer;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=YES;layer.drawableSize=CGSizeMake(640,360);layer.contentsGravity=kCAGravityResizeAspect;
    NSURL *support=[NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;NSURL *saves=[support URLByAppendingPathComponent:@"NativeRuntimeProbe" isDirectory:YES];NSError *error=nil;
    if(!saves||![NSFileManager.defaultManager createDirectoryAtURL:saves withIntermediateDirectories:YES attributes:nil error:&error]){self.sceneRunning=NO;[self.startupTimer invalidate];self.startupTimer=nil;[self setWorking:NO];[self append:@"FAIL: native save folder unavailable"];return;}
    self.report.text=[NSString stringWithFormat:@"Native integration 0.19 — stage 3 in progress\niOS %@ / %@\n%@\nGame folder: %@\nStarting real engine, archive reader and native graphics…",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint(),self.gameFolder.lastPathComponent];
    [NSUserDefaults.standardUserDefaults setObject:self.report.text forKey:@"lastReport"];[NSUserDefaults.standardUserDefaults setBool:YES forKey:@"probeRunning"];NSURL *folder=self.gameFolder;
    NSThread *thread=[[NSThread alloc]initWithBlock:^{@autoreleasepool{
        BOOL scoped=[folder startAccessingSecurityScopedResource];char report[4096]={0};int code=native_scene_probe(folder.fileSystemRepresentation,saves.fileSystemRepresentation,(__bridge void*)layer,RuntimeProgress,(__bridge void*)self,report,sizeof(report));if(scoped)[folder stopAccessingSecurityScopedResource];
        NSString *result=[NSString stringWithUTF8String:report]?:@"FAIL: invalid startup report";NSString *memory=Footprint();
        dispatch_async(dispatch_get_main_queue(),^{[self flushProgress];[self append:[NSString stringWithFormat:@"%@\nExit: %d\nAfter startup cleanup: %@",result,code,memory]];[NSUserDefaults.standardUserDefaults setBool:NO forKey:@"probeRunning"];self.sceneRunning=NO;[self.startupTimer invalidate];self.startupTimer=nil;[self setWorking:NO];});
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
- (void)sceneWillResignActive:(UIScene *)scene {native_scene_request_stop();}
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
