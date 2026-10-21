/* Main-thread preferences adapter. Dictionary serialization is explicit; never
   persist raw C padding or let legacy keys override the versioned snapshot. */
@interface NativeSettingsStore : NSObject
@property(nonatomic,assign) NativeSettings value;
@property(nonatomic,copy) NSString *validationError;
- (BOOL)save;
@end
@implementation NativeSettingsStore
- (instancetype)init {
 if((self=[super init])){
  NativeSettings v=native_settings_defaults();NSUserDefaults *d=NSUserDefaults.standardUserDefaults;id stored=[d objectForKey:@"launcherSettings.v1"];
  if(stored){
   if(![stored isKindOfClass:NSDictionary.class]||![stored[@"schema"] isEqual:@1])self.validationError=@"Saved settings have an unsupported format. Restore defaults before launching.";
   else for(size_t i=0;i<NATIVE_SETTING_COUNT;i++){const NativeSettingSpec *f=&native_setting_specs[i];id n=stored[@(f->key)];if(n&&(![n isKindOfClass:NSNumber.class]||!native_setting_set(&v,f,[n doubleValue]))){self.validationError=[NSString stringWithFormat:@"Invalid saved value: %s. Restore defaults before launching.",f->title];break;}}
  }else{
   unsigned q=(unsigned)[d integerForKey:@"launchQuality"];native_settings_preset(&v,q<=2?q:0);v.mode=[d integerForKey:@"launchMode"]==2?2:1;
   const char *old[]={"inputDeadzone","inputSensitivity","inputSensitivity","inputOpacity","inputSize","inputFullscreen","displayDiagnostics","rendererV2","cpuExecutionCoreV2","serviceCPUProfile","referenceShadowCompatibility"};
   const char *now[]={"deadzone","touchSensitivity","controllerSensitivity","opacity","stickSize","fullscreen","showFPS","rendererV2","coreV2","profile","shadowCompatibility"};
   for(unsigned j=0;j<sizeof(old)/sizeof(old[0]);j++){id n=[d objectForKey:@(old[j])];if([n isKindOfClass:NSNumber.class])for(size_t i=0;i<NATIVE_SETTING_COUNT;i++)if(!strcmp(native_setting_specs[i].key,now[j]))native_setting_set(&v,&native_setting_specs[i],[n doubleValue]);}
  }
  const char *error=native_settings_validate(&v);if(error)self.validationError=[NSString stringWithFormat:@"Invalid saved settings: %s. Restore defaults before launching.",error];
  self.value=v;if(!self.validationError)[self save];
 }return self;
}
- (BOOL)save {
 const char *error=native_settings_validate(&_value);if(error){self.validationError=@(error);return NO;}
 NSMutableDictionary *d=[@{@"schema":@1} mutableCopy];for(size_t i=0;i<NATIVE_SETTING_COUNT;i++)d[@(native_setting_specs[i].key)]=@(native_setting_get(&_value,&native_setting_specs[i]));
 [NSUserDefaults.standardUserDefaults setObject:d forKey:@"launcherSettings.v1"];self.validationError=nil;return YES;
}
@end
