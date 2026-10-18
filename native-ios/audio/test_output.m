#include "native_audio.m"
#include <assert.h>
/* Offline AVAudioEngine executes the production source callback and mixer.
   Samples are fixture PCM, not a claim that asset decoding or speakers work. */
int main(void){@autoreleasepool{
 static _Atomic unsigned char data[64+16384*8];atomic_store((_Atomic uint32_t*)(data+8),16384);
 for(unsigned i=0;i<2048;i++){float values[2]={.125f,-.25f};for(unsigned c=0;c<2;c++){unsigned char *b=(unsigned char*)&values[c];for(unsigned j=0;j<4;j++)atomic_store(data+64+i*8+c*4+j,b[j]);}}atomic_store((_Atomic uint32_t*)data,2048);
 setup();__block bool ok=true;dispatch_sync(control,^{
  assert(na_attach(&pcm,data,sizeof(data),0,16384));make_graph();NSError *error=nil;
  AVAudioFormat *f=[[AVAudioFormat alloc]initStandardFormatWithSampleRate:48000 channels:2];
  if(![engine enableManualRenderingMode:AVAudioEngineManualRenderingModeOffline format:f maximumFrameCount:128 error:&error]){fprintf(stderr,"Manual audio configuration: %s\n",error.localizedDescription.UTF8String);ok=false;return;}
  atomic_store(&pcm.enabled,true);if(![engine startAndReturnError:&error]){fprintf(stderr,"Manual audio start: %s\n",error.localizedDescription.UTF8String);ok=false;return;}
  AVAudioPCMBuffer *buffer=[[AVAudioPCMBuffer alloc]initWithPCMFormat:f frameCapacity:128];
  for(unsigned i=0;i<8;i++){assert([engine renderOffline:128 toBuffer:buffer error:&error]==AVAudioEngineManualRenderingStatusSuccess);assert(buffer.frameLength==128);for(unsigned j=0;j<128;j++){assert(fabsf(buffer.floatChannelData[0][j]-.125f)<1e-6f);assert(fabsf(buffer.floatChannelData[1][j]+.25f)<1e-6f);}}
  disable();assert(!atomic_load(&pcm.readers));engine=nil;source=nil;pcm.base=NULL;
 });assert(ok);printf("PASS: native AVAudioEngine production source node, 48-kHz stereo connection, 1024 exact output frames, L/R mapping and stopped graph teardown. Offline fixture PCM; physical game sound pending.\n");
}return 0;}
