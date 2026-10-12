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
@property(nonatomic,strong) UILabel *controllerLabel;
@property(nonatomic,assign) BOOL busy;
@end
@implementation ProbeController
- (UIButton *)button:(NSString *)title action:(SEL)action {
    UIButton *button=[UIButton buttonWithType:UIButtonTypeSystem];
    UIButtonConfiguration *config=[UIButtonConfiguration filledButtonConfiguration];config.title=title;button.configuration=config;
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];return button;
}
- (void)viewDidLoad {
    [super viewDidLoad];self.view.backgroundColor=UIColor.systemBackgroundColor;
    UIStackView *stack=[[UIStackView alloc]init];stack.axis=UILayoutConstraintAxisVertical;stack.spacing=14;stack.translatesAutoresizingMaskIntoConstraints=NO;
    [self.view addSubview:stack];UILayoutGuide *safe=self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[[stack.topAnchor constraintEqualToAnchor:safe.topAnchor constant:16],[stack.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor constant:-16],[stack.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:20],[stack.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-20]]];
    UILabel *title=[[UILabel alloc]init];title.text=@"Native engine prototype 0.1";title.font=[UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];title.adjustsFontForContentSizeCategory=YES;title.numberOfLines=0;[stack addArrangedSubview:title];
    UILabel *description=[[UILabel alloc]init];description.text=@"Tests the converted engine on your iPhone. Game startup and the Metal game renderer are still pending. No gameplay in this build.";description.numberOfLines=0;description.font=[UIFont preferredFontForTextStyle:UIFontTextStyleBody];description.adjustsFontForContentSizeCategory=YES;[stack addArrangedSubview:description];
    self.runButton=[self button:@"Run native engine tests" action:@selector(runProbe)];[stack addArrangedSubview:self.runButton];
    self.filesButton=[self button:@"Check local web folder" action:@selector(selectFolder)];[stack addArrangedSubview:self.filesButton];
    self.controllerLabel=[[UILabel alloc]init];self.controllerLabel.numberOfLines=0;self.controllerLabel.font=[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];[stack addArrangedSubview:self.controllerLabel];
    self.report=[[UITextView alloc]init];self.report.editable=NO;self.report.font=[UIFont monospacedSystemFontOfSize:13 weight:UIFontWeightRegular];self.report.backgroundColor=UIColor.secondarySystemBackgroundColor;self.report.layer.cornerRadius=10;[stack addArrangedSubview:self.report];
    [self.report.heightAnchor constraintGreaterThanOrEqualToConstant:90].active=YES;
    [stack addArrangedSubview:[self button:@"Copy report" action:@selector(copyReport)]];
    NSUserDefaults *defaults=NSUserDefaults.standardUserDefaults;
    self.report.text=[defaults stringForKey:@"lastReport"]?:@"Ready. No game files are needed for the native engine test. The optional file check reads samples from your existing web folder.";
    if([defaults boolForKey:@"probeRunning"])self.report.text=[@"Previous test ended without a result. The app may have been closed or terminated.\n\n" stringByAppendingString:self.report.text];
    [defaults setBool:NO forKey:@"probeRunning"];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(controllerChanged:) name:GCControllerDidConnectNotification object:nil];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(controllerChanged:) name:GCControllerDidDisconnectNotification object:nil];
    [self refreshControllers];
}
- (void)dealloc {[NSNotificationCenter.defaultCenter removeObserver:self];}
- (void)setWorking:(BOOL)working {self.busy=working;self.runButton.enabled=!working;self.filesButton.enabled=!working;}
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
    self.report.text=[NSString stringWithFormat:@"Native prototype 0.1\niOS %@ / %@\n%@\nStarting actual engine instance…",UIDevice.currentDevice.systemVersion,UIDevice.currentDevice.model,Footprint()];
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
    NSURL *folder=urls.firstObject;if(!folder||self.busy)return;[self setWorking:YES];
    [self append:@"Checking three local game files with 64 KiB reads. Files stored in iCloud may need to download first."];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{@autoreleasepool{
        BOOL scoped=[folder startAccessingSecurityScopedResource];NSMutableString *report=[NSMutableString string];
        NSData *metadata=[NSData dataWithContentsOfURL:[NSBundle.mainBundle URLForResource:@"file-probes" withExtension:@"json"]];
        NSArray *samples=[NSJSONSerialization JSONObjectWithData:metadata options:0 error:nil];
        if(samples.count!=3)[report appendString:@"FAIL: bundled sample manifest\n"];
        for(NSDictionary *sample in samples){
            NSURL *file=[folder URLByAppendingPathComponent:sample[@"path"]];
            NSFileCoordinator *coordinator=[[NSFileCoordinator alloc]initWithFilePresenter:nil];NSError *coordError=nil;
            [coordinator coordinateReadingItemAtURL:file options:0 error:&coordError byAccessor:^(NSURL *available){
                NSError *error=nil;NSNumber *size=nil;[available getResourceValue:&size forKey:NSURLFileSizeKey error:&error];
                if(error||size.unsignedLongLongValue!=[sample[@"size"] unsignedLongLongValue]){[report appendFormat:@"FAIL: size/missing %@\n",sample[@"path"]];return;}
                NSFileHandle *handle=[NSFileHandle fileHandleForReadingFromURL:available error:&error];
                NSData *bytes=[handle readDataUpToLength:65536 error:&error];[handle closeAndReturnError:nil];
                BOOL valid=bytes&&!error&&[Digest(bytes) isEqualToString:sample[@"prefixSHA256"]];
                [report appendFormat:@"%@: %@\n",valid?@"PASS":@"FAIL: read/hash",sample[@"path"]];
            }];
            if(coordError)[report appendFormat:@"FAIL: %@ — %@\n",sample[@"path"],coordError.localizedDescription];
        }
        if(scoped)[folder stopAccessingSecurityScopedResource];
        [report appendString:@"Sample check only; this does not validate or install the complete game."];
        dispatch_async(dispatch_get_main_queue(),^{[self append:report];[self setWorking:NO];});
    }});
}
@end
@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic,strong) UIWindow *window;
@end
@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    self.window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];self.window.rootViewController=[[ProbeController alloc]init];[self.window makeKeyAndVisible];return YES;
}
@end
int main(int argc,char **argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(AppDelegate.class));}}
