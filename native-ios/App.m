#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <GameController/GameController.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <CommonCrypto/CommonDigest.h>
#import <mach/mach.h>
#include "probe.h"

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
@property(nonatomic,strong) NSURL *gameFolder;
@property(nonatomic,strong) UILabel *controllerLabel;
@property(nonatomic,assign) BOOL busy;
- (void)append:(NSString *)text;
@end
static void RuntimeProgress(void *context,const char *line) {
    ProbeController *controller=(__bridge ProbeController *)context;
    NSString *text=[NSString stringWithUTF8String:line]?:@"Invalid runtime log";
    dispatch_sync(dispatch_get_main_queue(),^{[controller append:text];});
}
@implementation ProbeController
- (UIButton *)button:(NSString *)title action:(SEL)action {
    UIButton *button=[UIButton buttonWithType:UIButtonTypeSystem];
    UIButtonConfiguration *config=[UIButtonConfiguration filledButtonConfiguration];config.title=title;button.configuration=config;
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];return button;
}
- (void)viewDidLoad {
    [super viewDidLoad];self.view.backgroundColor=UIColor.systemBackgroundColor;
    UIScrollView *scroll=[[UIScrollView alloc]init];scroll.translatesAutoresizingMaskIntoConstraints=NO;[self.view addSubview:scroll];
    UIStackView *stack=[[UIStackView alloc]init];stack.axis=UILayoutConstraintAxisVertical;stack.spacing=14;stack.translatesAutoresizingMaskIntoConstraints=NO;
    [scroll addSubview:stack];UILayoutGuide *safe=self.view.safeAreaLayoutGuide,*content=scroll.contentLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[[scroll.topAnchor constraintEqualToAnchor:safe.topAnchor],[scroll.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],[scroll.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],[scroll.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],[stack.topAnchor constraintEqualToAnchor:content.topAnchor constant:16],[stack.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-16],[stack.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:20],[stack.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-20],[stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor constant:-40]]];
    UILabel *title=[[UILabel alloc]init];title.text=@"Native engine prototype 0.2";title.font=[UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];title.adjustsFontForContentSizeCategory=YES;title.numberOfLines=0;[stack addArrangedSubview:title];
    UILabel *description=[[UILabel alloc]init];description.text=@"Runtime checkpoint: initializes the real engine and tests its file functions and separate app saves. Select your web folder first to include game-file reads. Worker dispatch and the game renderer are pending; this build has no gameplay.";description.numberOfLines=0;description.font=[UIFont preferredFontForTextStyle:UIFontTextStyleBody];description.adjustsFontForContentSizeCategory=YES;[stack addArrangedSubview:description];
    self.runButton=[self button:@"Run native engine tests" action:@selector(runProbe)];[stack addArrangedSubview:self.runButton];
    self.filesButton=[self button:@"Check local web folder" action:@selector(selectFolder)];[stack addArrangedSubview:self.filesButton];
    self.runtimeButton=[self button:@"Run runtime + save tests" action:@selector(runRuntime)];[stack addArrangedSubview:self.runtimeButton];
    self.controllerLabel=[[UILabel alloc]init];self.controllerLabel.numberOfLines=0;self.controllerLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];[stack addArrangedSubview:self.controllerLabel];
    self.report=[[UITextView alloc]init];self.report.editable=NO;self.report.font=[UIFont monospacedSystemFontOfSize:13 weight:UIFontWeightRegular];self.report.backgroundColor=UIColor.secondarySystemBackgroundColor;self.report.layer.cornerRadius=10;[stack addArrangedSubview:self.report];
    [self.report.heightAnchor constraintEqualToConstant:240].active=YES;
    [stack addArrangedSubview:[self button:@"Copy report" action:@selector(copyReport)]];
    NSUserDefaults *defaults=NSUserDefaults.standardUserDefaults;
    self.report.text=[defaults stringForKey:@"lastReport"]?:@"Ready. Select the extracted web folder, then run runtime + save tests. Run it a second time to check that the probe save persisted. The basic engine test is also available.";
    if([defaults boolForKey:@"probeRunning"])self.report.text=[@"Previous test ended without a result. The app may have been closed or terminated.\n\n" stringByAppendingString:self.report.text];
    [defaults setBool:NO forKey:@"probeRunning"];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(controllerChanged:) name:GCControllerDidConnectNotification object:nil];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(controllerChanged:) name:GCControllerDidDisconnectNotification object:nil];
    [self refreshControllers];
}
- (void)dealloc {[NSNotificationCenter.defaultCenter removeObserver:self];}
- (void)setWorking:(BOOL)working {self.busy=working;self.runButton.enabled=!working;self.filesButton.enabled=!working;self.runtimeButton.enabled=!working;}
- (void)append:(NSString *)text {
    self.report.text=[self.report.text stringByAppendingFormat:@"\n%@",text];
    [NSUserDefaults.standardUserDefaults setObject:self.report.text forKey:@"lastReport"];
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
    self.report.text=[NSString stringWithFormat:@"Native prototype 0.2 — basic test\niOS %@ / %@\n%@\nStarting actual engine instance…",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint()];
    [NSUserDefaults.standardUserDefaults setObject:self.report.text forKey:@"lastReport"];
    [NSUserDefaults.standardUserDefaults setBool:YES forKey:@"probeRunning"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{@autoreleasepool{
        char report[2048]={0};int code=native_engine_probe(report,sizeof(report));
        NSString *result=[NSString stringWithUTF8String:report]?:@"FAIL: invalid report";
        NSString *memory=Footprint();NSString *metal=[self testMetal];
        dispatch_async(dispatch_get_main_queue(),^{
            [self append:[NSString stringWithFormat:@"%@\nExit: %d\n%@\n%@",result,code,memory,metal]];
            [NSUserDefaults.standardUserDefaults setBool:NO forKey:@"probeRunning"];[self setWorking:NO];
        });
    }});
}
- (void)runRuntime {
    if(self.busy)return;
    NSURL *support=[NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;
    NSURL *saves=[support URLByAppendingPathComponent:@"NativeRuntimeProbe" isDirectory:YES];
    NSError *error=nil;
    if(!saves||![NSFileManager.defaultManager createDirectoryAtURL:saves withIntermediateDirectories:YES attributes:nil error:&error]){[self append:[NSString stringWithFormat:@"FAIL: app save folder — %@",error.localizedDescription]];return;}
    [self setWorking:YES];
    self.report.text=[NSString stringWithFormat:@"Native prototype 0.2 — runtime checkpoint\niOS %@ / %@\n%@\nGame folder: %@",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint(),self.gameFolder.lastPathComponent?:@"not selected; asset checks will be skipped"];
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
            [self append:[NSString stringWithFormat:@"%@\nExit: %d\nAfter engine cleanup: %@",result,code,memory]];
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
        [report appendString:@"Sample check only; this does not validate or install the complete game.\nFolder selected for runtime tests. Tap Run runtime + save tests next."];
        dispatch_async(dispatch_get_main_queue(),^{[self append:report];[self setWorking:NO];});
    }});
}
@end
@interface ProbeSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic,strong) UIWindow *window;
@end
@implementation ProbeSceneDelegate
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
