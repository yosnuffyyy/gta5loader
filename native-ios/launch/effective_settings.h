#ifndef NATIVE_EFFECTIVE_SETTINGS_H
#define NATIVE_EFFECTIVE_SETTINGS_H
/* Pinned CApp startup destinations, independently checked against original
   bytecode in test_settings_engine.cjs. Engine owner reads at pad boundary. */
static void native_settings_observe(struct w2c_env *h,const NativeSettings *s){
 static const struct {const char *name;uint32_t address;bool floating;unsigned bytes;} fields[]={
 {"shadowQuality",19446864u,false,4},
 {"reflectionQuality",19446868u,false,4},
 {"SSAO",19446876u,false,4},
 {"anisotropicQualityLevel",19446880u,false,4},
 {"textureQuality",19446900u,false,4},
 {"particleQuality",19446904u,false,4},
 {"waterQuality",19446908u,false,4},
 {"grassQuality",19446912u,false,4},
 {"shaderQuality",19446916u,false,4},
 {"cityDensity",19446956u,true,4},
 {"pedVariety",19446960u,true,4},
 {"vehicleVariety",19446964u,true,4},
 {"fxaa",19446950u,false,1},
 {"postFX",19446972u,false,4},
 {"MotionBlurStrength",19446984u,true,4},
 {"lodScale",19446852u,true,4},
 {"pedLodBias",19446856u,true,4},
 {"vehicleLodBias",19446860u,true,4},
 {"tessellation",19446848u,false,4},
 {"reflectionMSAA",19446872u,false,4},
 {"txaa",19446951u,false,1},
 };
 char line[2304];size_t used=(size_t)snprintf(line,sizeof(line),"Engine effective settings at owner pad boundary (requested argument -> current engine value; shadow -1 maps to 0/off):");
 for(size_t i=0;i<sizeof(fields)/sizeof(fields[0]);i++){
  uint32_t raw=0;float f=0;host_load(h,&raw,fields[i].address,fields[i].bytes);memcpy(&f,&raw,4);double value=fields[i].floating?f:(double)raw;
  double requested=0;for(size_t j=0;j<NATIVE_SETTING_COUNT;j++)if(native_setting_specs[j].argument&&!strcmp(fields[i].name,native_setting_specs[j].argument)){requested=native_setting_get(s,&native_setting_specs[j]);break;}
  if(used<sizeof(line))used+=(size_t)snprintf(line+used,sizeof(line)-used," %s %.4g->%.4g;",fields[i].name,requested,value);
 }
 uint32_t divisor=0;host_load(h,&divisor,12622836,4);
 if(used<sizeof(line))snprintf(line+used,sizeof(line)-used," frameLimit %u->%u (60Hz divisor; not measured FPS).",60/s->fps,divisor);
 host_log(h,line);
}
#endif
