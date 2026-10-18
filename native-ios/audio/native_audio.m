#import <AVFoundation/AVFoundation.h>
#include <TargetConditionals.h>
#include <time.h>
#include <stdio.h>
#include "native_audio.h"
#include "native_pcm.h"
/* Control work is serialized away from engine, UI and realtime callbacks. The
   singleton PCM storage has process lifetime. A stopped graph cannot retain a
   dangling context; the engine heap is detached before its reservation dies. */
static NativePCM pcm;
static dispatch_queue_t control;
static AVAudioEngine *engine;
static AVAudioSourceNode *source;
static bool wanted,suspended,interrupted;
static char failure[256];
static unsigned restarts;
static void disable(void){
 atomic_store_explicit(&pcm.enabled,false,memory_order_seq_cst);
 [engine stop];
 while(atomic_load_explicit(&pcm.readers,memory_order_seq_cst)){struct timespec t={0,100000};nanosleep(&t,NULL);}
}
static void record(NSError *error){snprintf(failure,sizeof(failure),"%s",error.localizedDescription.UTF8String?:"Native audio setup failed");}
static void make_graph(void){
 if(!engine){
  engine=[AVAudioEngine new];AVAudioFormat *format=[[AVAudioFormat alloc]initStandardFormatWithSampleRate:48000 channels:2];
  source=[[AVAudioSourceNode alloc]initWithFormat:format renderBlock:^OSStatus(BOOL *silence,const AudioTimeStamp *time,AVAudioFrameCount count,AudioBufferList *list){
   (void)time;
   if(list->mNumberBuffers!=2||!list->mBuffers[0].mData||!list->mBuffers[1].mData||list->mBuffers[0].mNumberChannels!=1||list->mBuffers[1].mNumberChannels!=1||list->mBuffers[0].mDataByteSize<(uint64_t)count*4||list->mBuffers[1].mDataByteSize<(uint64_t)count*4){
    for(unsigned i=0;i<list->mNumberBuffers;i++)if(list->mBuffers[i].mData)memset(list->mBuffers[i].mData,0,list->mBuffers[i].mDataByteSize);atomic_fetch_add(&pcm.invalid,1);*silence=YES;return noErr;
   }
   *silence=na_read(&pcm,list->mBuffers[0].mData,list->mBuffers[1].mData,count)==0;return noErr;
  }];
  [engine attachNode:source];[engine connect:source to:engine.mainMixerNode format:format];
 }
}
static bool activate(void){
 if(!wanted||suspended||interrupted||!pcm.base)return true;
 @try {
 NSError *error=nil;
#if TARGET_OS_IOS
 AVAudioSession *session=AVAudioSession.sharedInstance;
 if(![session setCategory:AVAudioSessionCategoryPlayback mode:AVAudioSessionModeDefault options:0 error:&error]||![session setPreferredSampleRate:48000 error:&error]||![session setPreferredIOBufferDuration:256.0/48000 error:&error]||![session setActive:YES error:&error]){record(error);return false;}
#endif
 make_graph();
 [engine prepare];atomic_store_explicit(&pcm.enabled,true,memory_order_seq_cst);
 if(![engine startAndReturnError:&error]){disable();record(error);return false;}
 failure[0]=0;restarts++;return true;
 } @catch(NSException *exception) {
  disable();snprintf(failure,sizeof(failure),"Audio graph exception: %s",exception.reason.UTF8String?:"unknown");return false;
 }
}
static void setup(void){static dispatch_once_t once;dispatch_once(&once,^{
 control=dispatch_queue_create("native.audio.control",DISPATCH_QUEUE_SERIAL);
 NSNotificationCenter *center=NSNotificationCenter.defaultCenter;
 [center addObserverForName:AVAudioEngineConfigurationChangeNotification object:nil queue:nil usingBlock:^(NSNotification *n){
  id changed=n.object;dispatch_async(control,^{if(changed!=engine||!wanted)return;disable();if(!suspended&&!interrupted)activate();});
 }];
#if TARGET_OS_IOS
 [center addObserverForName:AVAudioSessionInterruptionNotification object:nil queue:nil usingBlock:^(NSNotification *n){
  NSUInteger type=[n.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue],options=[n.userInfo[AVAudioSessionInterruptionOptionKey] unsignedIntegerValue];
  dispatch_async(control,^{if(type==AVAudioSessionInterruptionTypeBegan){interrupted=true;disable();}else{interrupted=false;if(options&AVAudioSessionInterruptionOptionShouldResume)activate();}});
 }];
 [center addObserverForName:AVAudioSessionMediaServicesWereResetNotification object:nil queue:nil usingBlock:^(NSNotification *n){(void)n;dispatch_async(control,^{disable();engine=nil;source=nil;activate();});}];
#endif
 });}
bool native_audio_start(_Atomic unsigned char *base,uint64_t size,uint64_t address,uint64_t capacity,char *error,size_t n){
 setup();__block bool ok=false;dispatch_sync(control,^{
  disable();engine=nil;source=nil;wanted=false;memset(&pcm,0,sizeof(pcm));failure[0]=0;restarts=0;
  if(!na_attach(&pcm,base,size,address,capacity)){snprintf(failure,sizeof(failure),"Invalid original stereo PCM ring extent/header");}
  else{wanted=true;ok=activate();}
  if(error&&n)snprintf(error,n,"%s",failure);
 });return ok;
}
void native_audio_stop(void){setup();dispatch_sync(control,^{wanted=false;disable();engine=nil;source=nil;pcm.base=NULL;
#if TARGET_OS_IOS
 NSError *error=nil;if(![AVAudioSession.sharedInstance setActive:NO withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation error:&error])record(error);
#endif
 });}
void native_audio_suspend(bool value){setup();dispatch_async(control,^{suspended=value;if(value)disable();else activate();});}
void native_audio_summary(char *text,size_t n){setup();dispatch_sync(control,^{
 snprintf(text,n,"Native PCM output: 48000 Hz stereo; running %u, starts %u; callbacks/frames/nonzero %llu/%llu/%llu; underrun frames %llu, invalid/nonfinite %llu/%llu; peak %.6f; maximum queued %u frames (%.1f ms); %s. Consumed PCM is not proof of speaker audibility or complete bank coverage.",atomic_load(&pcm.enabled),restarts,(unsigned long long)atomic_load(&pcm.calls),(unsigned long long)atomic_load(&pcm.frames),(unsigned long long)atomic_load(&pcm.nonzero),(unsigned long long)atomic_load(&pcm.underrun_frames),(unsigned long long)atomic_load(&pcm.invalid),(unsigned long long)atomic_load(&pcm.nonfinite),atomic_load(&pcm.peak)/1e6,atomic_load(&pcm.available_max),atomic_load(&pcm.available_max)/48.0,failure[0]?failure:"no output API error");
 });}
