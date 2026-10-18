#include "options.h"
#include <assert.h>
#include <stdio.h>
#include "../threading/native_session.h"
int main(int argc,char **argv){
 _Alignas(4) uint8_t block[448],before[448];NativeLaunchOptions o;unsigned checks=0;
 FILE *file=argc==2?fopen(argv[1],"wb"):NULL;assert(argc==1||file);
 const unsigned modes[]={1,2,1,2};
 for(unsigned i=0;i<4;i++)for(unsigned q=0;q<3;q++){
  memset(block,0xa5,sizeof(block));memcpy(before,block,sizeof(block));assert(native_launch_options(modes[i],q,&o));assert(native_launch_publish(block,444,o));
  assert(atomic_load((_Atomic uint32_t*)(block+440))==modes[i]);uint32_t focus;memcpy(&focus,block+280,4);assert(focus==1);
  for(unsigned j=0;j<sizeof(block);j++){if((j>=280&&j<284)||(j>=424&&j<428)||(j>=432&&j<444))continue;assert(block[j]==before[j]);checks++;}
  if(file)assert(fwrite(block,1,444,file)==444);
 }
 memcpy(before,block,sizeof(block));assert(!native_launch_publish(block,443,o));assert(!native_launch_publish(block+1,444,o));o.mode=3;assert(!native_launch_publish(block,444,o));assert(!memcmp(block,before,sizeof(block)));
 assert(native_launch_noautoload(1)&&!native_launch_noautoload(2));
 assert(!native_launch_city_spawn(1));assert(!strcmp(native_launch_city_spawn(2),"-playercoords=94.95,-742.96,44.75"));
 assert(native_launch_commit("[wasm] start mode: story")==1);assert(native_launch_commit("[wasm] start mode: sandbox")==2);assert(!native_launch_commit("[wasm] start mode: waiting"));
 assert(native_launch_story_script("[Script] >>>SCRIPT[Prologue1]+4821(pc = 4821)"));assert(!native_launch_story_script("Loading Prologue1 script asset"));
 assert(native_launch_player_event("[wasm] spawn loadout: 17 weapons restocked, 9999 rounds each: pistol"));assert(!native_launch_player_event("[wasm] spawn loadout: 0 weapons restocked, 9999 rounds each:"));assert(!native_launch_player_event("[wasm] spawn loadout: pistol is taken away again at every check, no longer given to this ped"));
 NativeSession session;session_begin(&session,0);session_scene_event(&session);assert(!session_check(&session,SESSION_SECOND,1,1,0));assert(session.phase==SESSION_STARTUP);session_hook(&session,2*SESSION_SECOND);assert(!session_check(&session,2*SESSION_SECOND,2,2,0));assert(session.phase==SESSION_LIVE);for(unsigned i=3;i<=1200;i++){session_hook(&session,i*SESSION_SECOND);assert(!session_check(&session,i*SESSION_SECOND,i,i,0));}assert(session_check(&session,1261*SESSION_SECOND,1200,1200,0)==SESSION_ENGINE_STALL);
 if(file)assert(!fclose(file));printf("PASS: production launch publication: 12 alternating mode/preset blocks, %u untouched-byte checks; bounds, focus, mode commit and noautoload policy.\n",checks);
}
