static UIColor *NativeGold(void){return [UIColor colorWithRed:.94 green:.75 blue:.30 alpha:1];}
static NSArray<NSString *> *NativePageNames(void){return @[@"Settings",@"Graphics",@"Controls",@"Game Files",@"Diagnostics",@"Gameplay",@"Audio",@"Advanced",@"GTA V"];}
static NSArray<NSNumber *> *NativeChoices(const char *key){
 if(!strcmp(key,"preset"))return @[@0,@1,@2,@3,@4,@5];
 if(!strcmp(key,"scale"))return @[@50,@60,@70,@75,@80,@90,@100];
 if(!strcmp(key,"fps"))return @[@15,@20,@30];
 if(!strcmp(key,"anisotropy"))return @[@0,@2,@4,@8,@16];
 if(!strcmp(key,"shadow"))return @[@(-1),@0,@1,@2];
 return nil;
}
static NSString *NativeSettingText(const NativeSettingSpec *f,double v){
 const char *k=f->key;
 if(!strcmp(k,"preset"))return @(native_settings_preset_name((unsigned)v));
 if(!strcmp(k,"base"))return @[@"640 × 360",@"960 × 540",@"1280 × 720"][(unsigned)v];
 if(!strcmp(k,"mode"))return v==2?@"Sandbox Mode":@"Story Mode";
 if(!strcmp(k,"scale"))return [NSString stringWithFormat:@"%.0f%%",v];
 if(!strcmp(k,"fps"))return [NSString stringWithFormat:@"%.0f FPS",v];
 if(!strcmp(k,"texture"))return @[@"Normal",@"High",@"Very High"][(unsigned)v];
 if(!strcmp(k,"aa"))return v?@"FXAA":@"Off";
 if(!strcmp(k,"shadow"))return @[@"Off",@"Normal",@"High",@"Very High"][(unsigned)(v+1)];
 if(!strcmp(k,"anisotropy"))return v?[NSString stringWithFormat:@"%.0f×",v]:@"Off";
 if(f->type==NS_BOOL)return v?@"On":@"Off";
 if(f->type==NS_FLOAT)return [NSString stringWithFormat:@"%.2f",v];
 return [NSString stringWithFormat:@"Level %.0f",v];
}

@implementation ProbeController (NativeSettingsUI)
- (void)buildLibraryNavigation {
 self.playDock=[[UIView alloc]init];self.playDock.translatesAutoresizingMaskIntoConstraints=NO;self.playDock.backgroundColor=[UIColor colorWithWhite:.045 alpha:1];[self.view addSubview:self.playDock];self.playDockHeight=[self.playDock.heightAnchor constraintEqualToConstant:72];self.playDockHeight.active=YES;
 self.navigationPanel=[[NativeMenuScrollView alloc]init];self.navigationPanel.translatesAutoresizingMaskIntoConstraints=NO;self.navigationPanel.backgroundColor=[UIColor colorWithWhite:.055 alpha:1];self.navigationPanel.showsHorizontalScrollIndicator=YES;self.navigationPanel.showsVerticalScrollIndicator=YES;self.navigationPanel.delaysContentTouches=NO;[self.view addSubview:self.navigationPanel];
 self.navigationItems=NativeVertical();self.navigationItems.translatesAutoresizingMaskIntoConstraints=NO;self.navigationItems.spacing=6;[self.navigationPanel addSubview:self.navigationItems];
 NSArray *names=@[@"Home",@"GTA V",@"Graphics",@"Gameplay",@"Controls",@"Audio",@"Advanced",@"Game Files",@"Diagnostics"];
 NSArray *icons=@[@"house",@"square.stack",@"display",@"person",@"gamecontroller",@"speaker.wave.2",@"slider.horizontal.3",@"folder",@"waveform.path.ecg"];
 const unsigned pages[]={99,8,1,5,2,6,7,3,4};
 for(unsigned i=0;i<names.count;i++){UIButton *b=[self dashboardButton:names[i] symbol:icons[i] action:@selector(libraryNavigate:)];b.tag=pages[i];b.accessibilityIdentifier=[@"library." stringByAppendingString:names[i]];UIButtonConfiguration *c=b.configuration;c.contentInsets=NSDirectionalEdgeInsetsMake(12,12,12,12);c.baseBackgroundColor=UIColor.clearColor;if(!i)c.baseForegroundColor=NativeGold();b.configuration=c;[self.navigationItems addArrangedSubview:b];}
 UILayoutGuide *safe=self.view.safeAreaLayoutGuide,*content=self.navigationPanel.contentLayoutGuide,*frame=self.navigationPanel.frameLayoutGuide;
 [NSLayoutConstraint activateConstraints:@[[self.navigationPanel.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],[self.navigationPanel.topAnchor constraintEqualToAnchor:safe.topAnchor],[self.navigationItems.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:8],[self.navigationItems.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-8],[self.navigationItems.topAnchor constraintEqualToAnchor:content.topAnchor constant:8],[self.navigationItems.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-8],[self.mainScroll.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],[self.mainScroll.bottomAnchor constraintEqualToAnchor:self.playDock.topAnchor],[self.playDock.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],[self.playDock.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],[self.playDock.leadingAnchor constraintEqualToAnchor:self.mainScroll.leadingAnchor]]];
 self.libraryWide=@[[self.navigationPanel.widthAnchor constraintEqualToConstant:166],[self.navigationPanel.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],[self.navigationItems.widthAnchor constraintEqualToAnchor:frame.widthAnchor constant:-16],[self.mainScroll.leadingAnchor constraintEqualToAnchor:self.navigationPanel.trailingAnchor],[self.mainScroll.topAnchor constraintEqualToAnchor:safe.topAnchor]];
 self.libraryNarrow=@[[self.navigationPanel.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],[self.navigationPanel.heightAnchor constraintEqualToConstant:68],[self.navigationItems.heightAnchor constraintEqualToAnchor:frame.heightAnchor constant:-16],[self.mainScroll.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],[self.mainScroll.topAnchor constraintEqualToAnchor:self.navigationPanel.bottomAnchor]];
 self.navigationItems.axis=UILayoutConstraintAxisHorizontal;[NSLayoutConstraint activateConstraints:self.libraryNarrow];
}
- (void)layoutLibrary {
 if(!self.navigationPanel)return;BOOL wide=self.view.bounds.size.width>=660&&self.view.bounds.size.width>self.view.bounds.size.height;
 if(wide==self.libraryLandscape)return;self.libraryLandscape=wide;self.libraryArtHeight.constant=wide?96:170;self.modeDescription.hidden=wide;self.mainStack.spacing=wide?12:18;[NSLayoutConstraint deactivateConstraints:wide?self.libraryNarrow:self.libraryWide];self.navigationItems.axis=wide?UILayoutConstraintAxisVertical:UILayoutConstraintAxisHorizontal;[NSLayoutConstraint activateConstraints:wide?self.libraryWide:self.libraryNarrow];
}
- (void)libraryNavigate:(UIButton *)sender {
 if(sender.tag==99){[self.mainScroll setContentOffset:CGPointZero animated:NO];return;}
 [self showOptionsPage:sender.tag];
}
- (void)applyStoredSettings {
 NativeSettings s=self.settingsStore.value;
 self.gameModeControl.selectedSegmentIndex=s.mode-1;self.qualityControl.selectedSegmentIndex=s.preset;
 self.fullscreenSwitch.on=s.fullscreen;self.timingSwitch.on=s.showFPS;self.executionSwitch.on=s.rendererV2;self.cpuCoreSwitch.on=s.coreV2;self.profileSwitch.on=s.profile;self.shadowCompatibilitySwitch.on=s.shadowCompatibility;
 self.deadzoneSlider.value=s.deadzone;self.sensitivitySlider.value=s.controllerSensitivity;self.opacitySlider.value=s.opacity;self.sizeSlider.value=s.stickSize;
 ni_configure_sources(s.deadzone,s.touchSensitivity,s.controllerSensitivity,s.invertY,s.touchEnabled);
 native_audio_set_volume(s.volume,s.mute);self.gameControls.overlayOpacity=s.opacity;self.gameControls.stickScale=s.stickSize;self.gameControls.touchDisabled=!s.touchEnabled;
 [self.gameControls updateSource];[self.gameControls setNeedsLayout];[self applyFullscreen];
 for(size_t i=0;i<NATIVE_SETTING_COUNT;i++){const NativeSettingSpec *f=&native_setting_specs[i];UIControl *control=self.settingControls[@(f->key)];double v=native_setting_get(&s,f);control.enabled=!(f->startup&&self.busy);
  self.settingValues[@(f->key)].text=NativeSettingText(f,v);
  if([control isKindOfClass:UISlider.class])((UISlider*)control).value=v;
  else if([control isKindOfClass:UISwitch.class])((UISwitch*)control).on=v!=0;
  else if([control isKindOfClass:UIButton.class])[self updateSettingMenu:(UIButton*)control index:i];
 }
 unsigned w=0,h=0;native_settings_size(&s,&w,&h);self.resolutionPreview.text=[NSString stringWithFormat:@"Internal render: %u × %u\nDisplay stays aspect-fit. HUD shares the internal target; touch positions use the safe area. Restart required.",w,h];
 [self refreshDashboard];
}
- (void)changeSetting:(NSUInteger)index value:(double)value {
 if(index>=NATIVE_SETTING_COUNT)return;const NativeSettingSpec *f=&native_setting_specs[index];if(self.busy&&f->startup)return;
 NativeSettings s=self.settingsStore.value;if(!native_setting_set(&s,f,value))return;
 if(!strcmp(f->key,"preset")&&value<5)native_settings_preset(&s,(unsigned)value);
 else if(f->argument||(!strcmp(f->key,"base"))||!strcmp(f->key,"scale")||!strcmp(f->key,"fps"))s.preset=5;
 const char *error=native_settings_validate(&s);if(error){self.runtimeStatus=@(error);[self refreshDashboard];return;}
 self.settingsStore.value=s;[self.settingsStore save];[self applyStoredSettings];
}
- (void)settingChanged:(UIControl *)control {
 double value=[control isKindOfClass:UISwitch.class]?((UISwitch*)control).isOn:((UISlider*)control).value;
 [self changeSetting:(NSUInteger)control.tag value:value];
}
- (void)updateSettingMenu:(UIButton *)button index:(NSUInteger)index {
 const NativeSettingSpec *f=&native_setting_specs[index];NativeSettings s=self.settingsStore.value;double value=native_setting_get(&s,f);
 NSString *selected=NativeSettingText(f,value);if(button.menu&&[button.accessibilityValue isEqualToString:selected])return;button.accessibilityValue=selected;
 UIButtonConfiguration *c=button.configuration;c.title=selected;c.image=[UIImage systemImageNamed:@"chevron.up.chevron.down"];c.imagePlacement=NSDirectionalRectEdgeTrailing;c.baseForegroundColor=NativeGold();button.configuration=c;
 NSArray *choices=NativeChoices(f->key);if(!choices){NSMutableArray *items=[NSMutableArray new];for(int i=(int)f->minimum;i<=f->maximum;i++)[items addObject:@(i)];choices=items;}
 NSMutableArray *actions=[NSMutableArray new];__weak ProbeController *weak=self;
 for(NSNumber *n in choices){UIAction *a=[UIAction actionWithTitle:NativeSettingText(f,n.doubleValue) image:nil identifier:nil handler:^(UIAction *action){[weak changeSetting:index value:n.doubleValue];}];a.state=n.doubleValue==value?UIMenuElementStateOn:UIMenuElementStateOff;[actions addObject:a];}
 button.menu=[UIMenu menuWithChildren:actions];button.showsMenuAsPrimaryAction=YES;
}
- (void)addModelSetting:(NSUInteger)index stack:(UIStackView *)stack {
 const NativeSettingSpec *f=&native_setting_specs[index];NativeSettings s=self.settingsStore.value;double v=native_setting_get(&s,f);UIStackView *row=NativeVertical();row.spacing=8;
 UILabel *title=NativeText(@(f->title),UIFontTextStyleBody);[row addArrangedSubview:title];UIControl *control;
 if(f->type==NS_FLOAT){UISlider *slider=[[UISlider alloc]init];slider.minimumValue=f->minimum;slider.maximumValue=f->maximum;slider.value=v;slider.minimumTrackTintColor=NativeGold();[slider addTarget:self action:@selector(settingChanged:) forControlEvents:UIControlEventValueChanged];control=slider;
  UILabel *value=NativeText(NativeSettingText(f,v),UIFontTextStyleCaption1);value.textColor=NativeGold();self.settingValues[@(f->key)]=value;[row addArrangedSubview:value];
 }else if(f->type==NS_BOOL){UISwitch *toggle=[[UISwitch alloc]init];toggle.on=v!=0;toggle.onTintColor=NativeGold();[toggle addTarget:self action:@selector(settingChanged:) forControlEvents:UIControlEventValueChanged];control=toggle;}
 else{UIButton *b=[self dashboardButton:@"" symbol:@"" action:NULL];[self updateSettingMenu:b index:index];control=b;}
 control.tag=index;control.accessibilityLabel=@(f->title);control.accessibilityIdentifier=[@"setting." stringByAppendingString:@(f->key)];control.enabled=!(f->startup&&self.busy);self.settingControls[@(f->key)]=control;
 [row addArrangedSubview:control];[stack addArrangedSubview:NativeCard(row)];
}
- (void)unavailable:(NSString *)title reason:(NSString *)reason stack:(UIStackView *)stack {
 UIStackView *row=NativeVertical();UILabel *t=NativeText([title stringByAppendingString:@" · Unavailable"],UIFontTextStyleSubheadline);t.textColor=UIColor.secondaryLabelColor;[row addArrangedSubview:t];UILabel *r=NativeText(reason,UIFontTextStyleFootnote);r.textColor=UIColor.secondaryLabelColor;[row addArrangedSubview:r];[stack addArrangedSubview:NativeCard(row)];
}
- (void)populateModelPage:(NSUInteger)page stack:(UIStackView *)stack {
 self.settingControls=[NSMutableDictionary new];self.settingValues=[NSMutableDictionary new];
 if(page==NS_GRAPHICS){self.resolutionPreview=NativeText(@"",UIFontTextStyleSubheadline);[stack addArrangedSubview:self.resolutionPreview];[stack addArrangedSubview:NativeText(@"Graphics changes apply on the next launch. Higher quality uses more memory; lower resolution cannot remove CPU-side stalls.",UIFontTextStyleFootnote)];}
 if(page==NS_CONTROLS){[stack addArrangedSubview:self.controllerLabel];[stack addArrangedSubview:NativeText(@"Meaningful controller input hides touch controls immediately. Touch the game to return. Disable touch gameplay only when a controller is available; the Settings button stays accessible.",UIFontTextStyleFootnote)];}
 if(page==NS_GAMEPLAY)[stack addArrangedSubview:NativeText(@"Story and Sandbox keep independent save folders. Ambient city density is the engine's global setting; variety controls model diversity, not entity counts. Scripts retain their own actors and population overrides.",UIFontTextStyleFootnote)];
 if(page==NS_AUDIO){char audio[1500]={0};native_audio_summary(audio,sizeof(audio));[stack addArrangedSubview:NativeText(@(audio),UIFontTextStyleFootnote)];[stack addArrangedSubview:NativeText(@"Master volume and mute control real stereo PCM at native output. iOS manages speaker/headphone routing. PCM consumption does not establish audible output or complete bank coverage.",UIFontTextStyleFootnote)];}
 if(page==NS_ADVANCED)[stack addArrangedSubview:NativeText(@"Existing runtime · AOT ARM64\nRenderer and CPU core are independent. Experimental choices require a fresh launch. Existing implementations remain available when switches are off.",UIFontTextStyleBody)];
 for(size_t i=0;i<NATIVE_SETTING_COUNT;i++)if(native_setting_specs[i].page==page)[self addModelSetting:i stack:stack];
 if(page==NS_GRAPHICS){
  [self unavailable:@"MSAA 2× / 4×, TXAA / TAA" reason:@"WebGPU has no 2× sample count. Complete game-wide 4× resolve/readback behavior is not validated. Off and FXAA explicitly use 1× attachments; TXAA is disabled." stack:stack];
  [self unavailable:@"24 / 40 / 60 FPS and Uncapped" reason:@"24 and 40 are not integral divisors of the original 60 Hz limiter. 60/uncapped simulation timing is not validated. Supported caps are 15, 20 and 30; this does not guarantee achieved FPS." stack:stack];
  [self unavailable:@"Ultra Low / Low texture tiers" reason:@"The original texture-quality domain has three levels: Normal (0), High (1), Very High (2). There is no verified lower mip policy. Textures and valid mip chains are preserved." stack:stack];
  [self unavailable:@"854 × 480 / native display resolution" reason:@"Only exact 16:9 bases up to 1280 × 720 are enabled. Internal dimensions are rounded to an even 32 × 18 lattice and previewed. Physical display resolution is never changed." stack:stack];
  [self unavailable:@"VSync, shader quality, tessellation and extra effects" reason:@"Display synchronization remains on. Shader quality stays at the compatible level 1; tessellation and reflection MSAA remain off. Soft/long shadows, bloom, DOF, fog, lens effects and alternate upscalers need separate lifecycle/backend validation." stack:stack];
 }
 if(page==NS_CONTROLS)[self unavailable:@"Vibration, manual source locking and button remapping" reason:@"Automatic switching and existing game action mappings are preserved. Haptics and manual source-lock semantics are not implemented. Camera view can be changed with the game's existing control." stack:stack];
 if(page==NS_AUDIO)[self unavailable:@"Music / SFX / dialogue / radio buses" reason:@"The host receives the final mixed stereo stream. Independent bus controls are unavailable until original mixer interfaces are verified; master gain does not alter pitch or sample rate." stack:stack];
 if(page==NS_GAMEPLAY)[self unavailable:@"Separate pedestrian and vehicle density" reason:@"Per-frame engine script multipliers exist, but safe injection order and mission overrides are not established. The verified city-density and separate variety settings are exposed above." stack:stack];
 if(page==NS_ADVANCED)[self unavailable:@"Muguet runtime" reason:@"Migration Stage A validates a runtime/link checkpoint only. Game host services, graphics, saves, input and audio integration are not complete. This is not an operational runtime choice." stack:stack];
 [self applyStoredSettings];
}
@end
