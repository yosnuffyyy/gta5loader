#include "options.h"
#include "../audio/native_pcm.h"
#include <assert.h>
#include <time.h>
int main(int argc,char **argv){
 unsigned checks=0;NativeSettings s=native_settings_defaults();NativeLaunchOptions launch;NativeSettingArguments args;
 assert(!native_settings_validate(&s)&&native_launch_config(s,&launch));assert(launch.width==640&&launch.height==360);
 for(unsigned base=0;base<3;base++)for(unsigned scale=50;scale<=100;scale+=10){s.base=base;s.scale=scale;unsigned w,h;assert(native_settings_size(&s,&w,&h));assert(w*9==h*16&&w%2==0&&h%2==0);checks++;}
 s=native_settings_defaults();s.scale=70;assert(native_launch_config(s,&launch)&&launch.width==448&&launch.height==252);s.scale=50;assert(native_launch_config(s,&launch)&&launch.width==320&&launch.height==180);
 FILE *out=argc>1?fopen(argv[1],"wb"):NULL;assert(argc==1||out);if(out)fputs("[",out);
 for(unsigned p=0;p<5;p++)for(unsigned mode=1;mode<=2;mode++){
  s=native_settings_defaults();s.mode=mode;s.rendererV2=true;s.volume=.6f;native_settings_preset(&s,p);assert(s.mode==mode&&s.rendererV2&&s.volume==.6f);assert(native_launch_config(s,&launch));
  assert(native_settings_arguments(&s,&args));assert(args.count<48&&!args.argv[args.count]);
  for(unsigned a=0;a<args.count;a++)for(unsigned b=a+1;b<args.count;b++){size_t n=strcspn(args.argv[a],"=");assert(strncmp(args.argv[a],args.argv[b],n)||args.argv[b][n]!='=');}
  _Alignas(4) unsigned char block[444]={0};assert(native_launch_publish(block,sizeof(block),launch));uint32_t w,h;memcpy(&w,block+432,4);memcpy(&h,block+436,4);assert(w==launch.width&&h==launch.height);
  NativeSettings frozen=s;s.texture=2;s.scale=70;assert(launch.settings.texture==frozen.texture&&launch.settings.scale==frozen.scale);checks+=6;
  if(out){if(p||mode==2)fputs(",",out);fprintf(out,"{\"mode\":%u,\"preset\":%u,\"width\":%u,\"height\":%u,\"args\":[",mode,p,w,h);for(unsigned i=0;i<args.count;i++)fprintf(out,"%s\"%s\"",i?",":"",args.argv[i]);fputs("]}",out);}
 }
 for(unsigned cap=15;cap<=30;cap+=5){if(cap==25)continue;for(unsigned scaleIndex=0;scaleIndex<3;scaleIndex++){
  const unsigned scales[]={50,70,100};s=native_settings_defaults();s.fps=cap;s.scale=scales[scaleIndex];s.preset=5;s.base=scaleIndex;s.texture=2;s.aa=1;s.shadow=2;s.reflection=3;s.water=1;s.particles=2;s.grass=3;s.postfx=3;s.ssao=2;s.anisotropy=16;s.lod=.75f;s.pedLod=.5f;s.vehicleLod=.25f;s.motionBlur=.35f;s.cityDensity=.8f;s.pedVariety=.4f;s.vehicleVariety=.6f;
  assert(native_launch_config(s,&launch)&&native_settings_arguments(&s,&args));checks++;
  if(out){fprintf(out,",{\"mode\":1,\"preset\":5,\"width\":%u,\"height\":%u,\"args\":[",launch.width,launch.height);for(unsigned i=0;i<args.count;i++)fprintf(out,"%s\"%s\"",i?",":"",args.argv[i]);fputs("]}",out);}
 }}
 if(out){fputs("]\n",out);assert(!fclose(out));}
 s=native_settings_defaults();for(size_t i=0;i<NATIVE_SETTING_COUNT;i++){const NativeSettingSpec *f=&native_setting_specs[i];NativeSettings before=s;assert(!native_setting_set(&s,f,NAN));assert(!memcmp(&s,&before,sizeof(s)));assert(!native_setting_set(&s,f,f->maximum+1));checks+=3;}
 s.scale=69;assert(native_settings_validate(&s));s.scale=100;s.fps=60;assert(native_settings_validate(&s));s.fps=30;s.texture=3;assert(native_settings_validate(&s));s.texture=0;s.base=3;assert(native_settings_validate(&s));
 float l[]={1,-.5f,0,.75f},r[]={-.25f,.25f,1,-1};na_gain(l,r,4,.5f);assert(l[0]==.5f&&l[1]==-.25f&&r[2]==.5f);na_gain(l,r,4,0);for(unsigned i=0;i<4;i++)assert(l[i]==0&&r[i]==0);
 s=native_settings_defaults();clock_t start=clock();for(unsigned i=0;i<100000;i++){assert(native_settings_arguments(&s,&args));checks++;}double ms=(double)(clock()-start)*1000/CLOCKS_PER_SEC;
 printf("PASS: %u settings assertions, 19 real argv fixtures (10 publication fixtures), immutable snapshots, all field bounds/NaN, presets, exact aspect and PCM gain/mute. 100000 startup-only serializations %.3f ms; settings size %zu bytes. Host CPU benchmark, not iPhone.\n",checks,ms,sizeof(s));
}
