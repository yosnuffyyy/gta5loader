#ifndef NATIVE_LAUNCHER_SETTINGS_H
#define NATIVE_LAUNCHER_SETTINGS_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

/* One versioned value object. UIKit owns the editable copy; engine launch takes
   a value snapshot. No preferences or UI objects are read by engine threads. */
typedef struct {
 uint32_t schema,mode,preset,base,scale,fps,texture,aa;
 int32_t shadow;
 uint32_t reflection,water,particles,grass,postfx,anisotropy,ssao;
 float lod,pedLod,vehicleLod,cityDensity,pedVariety,vehicleVariety,motionBlur;
 float volume,deadzone,touchSensitivity,controllerSensitivity,opacity,stickSize;
 bool fullscreen,showFPS,rendererV2,coreV2,profile,shadowCompatibility,mute,touchEnabled,invertY;
} NativeSettings;
typedef enum {NS_U32,NS_I32,NS_FLOAT,NS_BOOL} NativeSettingType;
typedef struct {const char *key,*title,*argument;NativeSettingType type;size_t offset;double minimum,maximum;unsigned page;bool startup;} NativeSettingSpec;
enum {NS_HOME=0,NS_GRAPHICS=1,NS_CONTROLS=2,NS_FILES=3,NS_DIAGNOSTICS=4,NS_GAMEPLAY=5,NS_AUDIO=6,NS_ADVANCED=7,NS_GAME=8};
#define NS_SPEC(field,title,arg,type,lo,hi,page,start) {#field,title,arg,type,offsetof(NativeSettings,field),lo,hi,page,start}
static const NativeSettingSpec native_setting_specs[]={
 NS_SPEC(mode,"Game mode",NULL,NS_U32,1,2,NS_GAME,true),
 NS_SPEC(preset,"Graphics preset",NULL,NS_U32,0,5,NS_GRAPHICS,true),
 NS_SPEC(base,"Base resolution",NULL,NS_U32,0,2,NS_GRAPHICS,true),
 NS_SPEC(scale,"Render scale",NULL,NS_U32,50,100,NS_GRAPHICS,true),
 NS_SPEC(fps,"Frame-rate limit",NULL,NS_U32,15,30,NS_GRAPHICS,true),
 NS_SPEC(texture,"Texture quality","textureQuality",NS_U32,0,2,NS_GRAPHICS,true),
 NS_SPEC(aa,"Anti-aliasing","fxaa",NS_U32,0,1,NS_GRAPHICS,true),
 NS_SPEC(shadow,"Shadow quality","shadowQuality",NS_I32,-1,2,NS_GRAPHICS,true),
 NS_SPEC(reflection,"Reflection quality","reflectionQuality",NS_U32,0,3,NS_GRAPHICS,true),
 NS_SPEC(water,"Water quality","waterQuality",NS_U32,0,1,NS_GRAPHICS,true),
 NS_SPEC(particles,"Particle quality","particleQuality",NS_U32,0,2,NS_GRAPHICS,true),
 NS_SPEC(grass,"Grass quality","grassQuality",NS_U32,0,3,NS_GRAPHICS,true),
 NS_SPEC(postfx,"Post-processing","postFX",NS_U32,0,3,NS_GRAPHICS,true),
 NS_SPEC(anisotropy,"Anisotropic filtering","anisotropicQualityLevel",NS_U32,0,16,NS_GRAPHICS,true),
 NS_SPEC(ssao,"Ambient occlusion","SSAO",NS_U32,0,2,NS_GRAPHICS,true),
 NS_SPEC(lod,"Distance scaling","lodScale",NS_FLOAT,0,1,NS_GRAPHICS,true),
 NS_SPEC(pedLod,"Pedestrian LOD","pedLodBias",NS_FLOAT,0,1,NS_GRAPHICS,true),
 NS_SPEC(vehicleLod,"Vehicle LOD","vehicleLodBias",NS_FLOAT,0,1,NS_GRAPHICS,true),
 NS_SPEC(motionBlur,"Motion blur","MotionBlurStrength",NS_FLOAT,0,1,NS_GRAPHICS,true),
 NS_SPEC(cityDensity,"Ambient city density","cityDensity",NS_FLOAT,0,1,NS_GAMEPLAY,true),
 NS_SPEC(pedVariety,"Pedestrian variety","pedVariety",NS_FLOAT,0,1,NS_GAMEPLAY,true),
 NS_SPEC(vehicleVariety,"Vehicle variety","vehicleVariety",NS_FLOAT,0,1,NS_GAMEPLAY,true),
 NS_SPEC(volume,"Master volume",NULL,NS_FLOAT,0,1,NS_AUDIO,false),
 NS_SPEC(mute,"Mute",NULL,NS_BOOL,0,1,NS_AUDIO,false),
 NS_SPEC(touchEnabled,"Touch gameplay controls",NULL,NS_BOOL,0,1,NS_CONTROLS,false),
 NS_SPEC(deadzone,"Controller deadzone",NULL,NS_FLOAT,.05,.4,NS_CONTROLS,false),
 NS_SPEC(touchSensitivity,"Touch look sensitivity",NULL,NS_FLOAT,.25,2,NS_CONTROLS,false),
 NS_SPEC(controllerSensitivity,"Controller look sensitivity",NULL,NS_FLOAT,.25,2,NS_CONTROLS,false),
 NS_SPEC(invertY,"Invert look Y",NULL,NS_BOOL,0,1,NS_CONTROLS,false),
 NS_SPEC(opacity,"Touch overlay opacity",NULL,NS_FLOAT,.15,1,NS_CONTROLS,false),
 NS_SPEC(stickSize,"Joystick size",NULL,NS_FLOAT,.75,1.2,NS_CONTROLS,false),
 NS_SPEC(fullscreen,"Fullscreen",NULL,NS_BOOL,0,1,NS_GRAPHICS,false),
 NS_SPEC(showFPS,"Displayed FPS overlay",NULL,NS_BOOL,0,1,NS_GRAPHICS,true),
 NS_SPEC(rendererV2,"Renderer V2 · experimental",NULL,NS_BOOL,0,1,NS_ADVANCED,true),
 NS_SPEC(coreV2,"CPU Execution Core V2 · experimental",NULL,NS_BOOL,0,1,NS_ADVANCED,true),
 NS_SPEC(shadowCompatibility,"Reference shadow compatibility",NULL,NS_BOOL,0,1,NS_ADVANCED,true),
 NS_SPEC(profile,"Detailed CPU profiling",NULL,NS_BOOL,0,1,NS_ADVANCED,true),
};
#undef NS_SPEC
#define NATIVE_SETTING_COUNT (sizeof(native_setting_specs)/sizeof(native_setting_specs[0]))
static inline NativeSettings native_settings_defaults(void){return (NativeSettings){.schema=1,.mode=1,.scale=100,.fps=30,.shadow=-1,.volume=1,.deadzone=.15f,.touchSensitivity=1,.controllerSensitivity=1,.opacity=.55f,.stickSize=1,.showFPS=true,.touchEnabled=true};}
static inline double native_setting_get(const NativeSettings *s,const NativeSettingSpec *f){const char *p=(const char*)s+f->offset;switch(f->type){case NS_U32:return *(const uint32_t*)p;case NS_I32:return *(const int32_t*)p;case NS_FLOAT:return *(const float*)p;case NS_BOOL:return *(const bool*)p;}return 0;}
static inline bool native_setting_set(NativeSettings *s,const NativeSettingSpec *f,double v){
 if(!s||!isfinite(v)||v<f->minimum-1e-7||v>f->maximum+1e-7||(f->type!=NS_FLOAT&&floor(v)!=v))return false;
 char *p=(char*)s+f->offset;switch(f->type){case NS_U32:*(uint32_t*)p=(uint32_t)v;break;case NS_I32:*(int32_t*)p=(int32_t)v;break;case NS_FLOAT:*(float*)p=(float)v;break;case NS_BOOL:*(bool*)p=v!=0;break;}return true;
}
static inline const char *native_settings_validate(const NativeSettings *s){
 if(!s||s->schema!=1)return "Unsupported settings schema. Restore defaults to continue.";
 for(size_t i=0;i<NATIVE_SETTING_COUNT;i++){const NativeSettingSpec *f=&native_setting_specs[i];double v=native_setting_get(s,f);if(!isfinite(v)||v<f->minimum-1e-7||v>f->maximum+1e-7)return f->title;}
 if(s->scale!=50&&s->scale!=60&&s->scale!=70&&s->scale!=75&&s->scale!=80&&s->scale!=90&&s->scale!=100)return "Unsupported render scale";
 if(s->fps!=15&&s->fps!=20&&s->fps!=30)return "This runtime supports verified caps of 15, 20 and 30 FPS";
 if(s->anisotropy!=0&&s->anisotropy!=2&&s->anisotropy!=4&&s->anisotropy!=8&&s->anisotropy!=16)return "Unsupported anisotropic filtering level";
 return NULL;
}
static inline bool native_settings_size(const NativeSettings *s,unsigned *w,unsigned *h){
 if(native_settings_validate(s)||!w||!h)return false;
 /* These bases are exact 16:9. Round the scale to a 32x18 lattice: both
    dimensions even, no aspect distortion. UI always displays actual pixels. */
 const unsigned widths[]={640,960,1280};unsigned units=(widths[s->base]*s->scale+1600)/3200;
 *w=units*32;*h=units*18;return true;
}
static inline const char *native_settings_preset_name(unsigned p){const char *n[]={"Low","Potato","Ultra Potato","Balanced","Quality","Custom"};return p<6?n[p]:"Invalid";}
static inline void native_settings_preset(NativeSettings *s,unsigned p){
 if(p>4)return;
 NativeSettings d=native_settings_defaults();
 /* Presets change graphics/population only; mode, audio, input, renderer,
    runtime, diagnostics and safe-area preferences are independent. */
 for(size_t i=0;i<NATIVE_SETTING_COUNT;i++){const NativeSettingSpec *f=&native_setting_specs[i];if(f->argument||!strcmp(f->key,"base")||!strcmp(f->key,"scale"))native_setting_set(s,f,native_setting_get(&d,f));}
 s->preset=p;s->scale=p==2?50:p==1?75:100;
 if(p>=3){s->aa=1;s->shadow=0;s->reflection=1;s->anisotropy=4;s->lod=.25f;s->cityDensity=.25f;s->pedVariety=.25f;s->vehicleVariety=.25f;}
 if(p==4){s->texture=1;s->shadow=1;s->water=1;s->particles=1;s->postfx=1;s->ssao=1;s->anisotropy=8;s->lod=.5f;}
}
/* Bounded, owned argv: no dangling stack strings or duplicate flags. */
typedef struct {char text[48][80];const char *argv[49];unsigned count;} NativeSettingArguments;
static inline bool native_settings_arguments(const NativeSettings *s,NativeSettingArguments *out){
 unsigned w,h;if(!out||!native_settings_size(s,&w,&h))return false;memset(out,0,sizeof(*out));
 snprintf(out->text[out->count++],80,"-width=%u",w);snprintf(out->text[out->count++],80,"-height=%u",h);
 snprintf(out->text[out->count++],80,"-frameLimit=%u",60/s->fps);
 /* WebGPU does not support sampleCount=2; complete game MSAA resolves remain
    unvalidated. Explicit 1x/quality0 prevents a saved desktop XML override. */
 strcpy(out->text[out->count++],"-multiSample=1");strcpy(out->text[out->count++],"-multiSampleQuality=0");
 strcpy(out->text[out->count++],"-reflectionMSAA=0");strcpy(out->text[out->count++],"-txaa=0");
 strcpy(out->text[out->count++],"-tessellation=0");strcpy(out->text[out->count++],"-shaderQuality=1");
 for(size_t i=0;i<NATIVE_SETTING_COUNT;i++){const NativeSettingSpec *f=&native_setting_specs[i];if(f->argument){if(out->count>=48)return false;snprintf(out->text[out->count++],80,"-%s=%.6g",f->argument,native_setting_get(s,f));}}
 for(unsigned i=0;i<out->count;i++)out->argv[i]=out->text[i];return true;
}
#endif
