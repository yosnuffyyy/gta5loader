/* Captured SRV 709/source 708 shape and unchanged original shadow-control
   block. Upload bytes, uniforms and geometry are deterministic test inputs. */
static bool shadow_fixture_run(struct w2c_env *h,unsigned invalid,bool count_control,bool legacy,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;
#ifdef NATIVE_PROBE_CLI
 g->test_legacy_shadow_counts=legacy;
#else
 if(legacy){native_gpu_end(g);return false;}
#endif
host_put32(h,base,64);native_gpu_start(g,base,131072,0);uint64_t deadline=now_ns()+45000000000ull;while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}bool ready=native_gpu_ready(g),ok=ready;uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if((count)>128||tail>127488-8-(count)*4){ok=false;fail(g,"Shadow fixture ring overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 cmd[0]=901;cmd[1]=1;cmd[2]=count_control?0x223:invalid==5?0x221:0x220;cmd[3]=UINT32_MAX;cmd[4]=0;EMIT(14,5);uint32_t sampler[]={902,3,0x80,3,3,3,0,0,4,0,0,0,0,0,0x42000000};memcpy(cmd,sampler,sizeof(sampler));EMIT(16,15);cmd[0]=1;cmd[1]=14;cmd[2]=1;cmd[3]=902;EMIT(25,4);
 uint32_t capture[]={708,2,4,4,1,1,1,71,8,1};memcpy(cmd,capture,sizeof(capture));EMIT(3,10);memset(cmd,0,sizeof(cmd));cmd[0]=708;cmd[6]=cmd[7]=4;cmd[8]=1;cmd[9]=8;cmd[10]=1;cmd[11]=8;cmd[12]=0xf800;EMIT(13,14);
 uint32_t captured_srv[]={709,0,708,71,4,0,1,0,0};if(invalid==2)captured_srv[4]=9;if(invalid==3)captured_srv[6]=2;if(invalid==4)captured_srv[3]=28;memcpy(cmd,captured_srv,sizeof(captured_srv));EMIT(17,9);
 uint32_t cube[]={710,2,8,8,1,2,6,41,0x28,1};memcpy(cmd,cube,sizeof(cube));EMIT(3,10);for(unsigned m=0;m<2;m++)for(unsigned face=0;face<6;face++){memset(cmd,0,sizeof(cmd));cmd[0]=710;cmd[1]=m;cmd[2]=face;float v=m?.75f:.25f;memcpy(cmd+3,&v,4);EMIT(5,7);}uint32_t cube_srv[]={711,0,710,41,9,0,1,0,6};memcpy(cmd,cube_srv,sizeof(cube_srv));EMIT(17,9);
 uint32_t spot[]={712,2,4,4,1,1,1,41,0x28,1};memcpy(cmd,spot,sizeof(spot));EMIT(3,10);memset(cmd,0,sizeof(cmd));cmd[0]=712;float spot_depth=.75f;memcpy(cmd+3,&spot_depth,4);EMIT(5,7);uint32_t spot_srv[]={713,0,712,41,4,0,1,0,0};memcpy(cmd,spot_srv,sizeof(spot_srv));EMIT(17,9);
 cmd[0]=714;cmd[1]=496;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);uint32_t cb[]={1,6,1,714,0,31};memcpy(cmd,cb,sizeof(cb));EMIT(23,6);cmd[0]=1;cmd[1]=24;cmd[2]=1;cmd[3]=713;EMIT(24,4);cmd[0]=1;cmd[1]=901;EMIT(18,2);
 /* A real BC1 cube, with six independent faces at two mip levels. */
 cube[0]=716;cube[7]=71;cube[8]=8;memcpy(cmd,cube,sizeof(cube));EMIT(3,10);for(unsigned m=0;m<2;m++)for(unsigned face=0;face<6;face++){memset(cmd,0,sizeof(cmd));unsigned side=8>>m,bytes=m?8:32;cmd[0]=716;cmd[1]=m;cmd[2]=face;cmd[6]=cmd[7]=side;cmd[8]=1;cmd[9]=m?8:16;cmd[10]=m?1:2;cmd[11]=bytes;for(unsigned block=0;block<bytes/8;block++)cmd[12+block*2]=(face+m)&1?0xffff:0;EMIT(13,12+bytes/4);}
 if(count_control){cmd[0]=715;cmd[1]=48;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);uint32_t count_cb[]={1,3,1,715,0,3};memcpy(cmd,count_cb,sizeof(count_cb));EMIT(23,6);}
 unsigned expected[24]={255,255,255,0,255,255,255,0};unsigned cases=invalid?1:20;
 for(unsigned test=0;test<cases;test++){
  if(count_control){memset(cmd,0,sizeof(cmd));cmd[0]=715;cmd[2]=48;int32_t lights=test==1?0:test==4?-1:test==6?INT32_MIN:1;memcpy(cmd+11,&lights,4);EMIT(12,15);}
  float values[124]={0};values[15*4]=1;values[16*4+1]=1;values[17*4+2]=1;values[17*4+3]=.5f;values[18*4+2]=-1;values[15*4+3]=invalid==1?1:test==2?3:test==3||test==5||test==7||test>=8?1:0;
  if(count_control&&test==1)values[15*4+3]=1;
  if(test==5){cmd[0]=711;EMIT(4,1);cube_srv[5]=1;memcpy(cmd,cube_srv,sizeof(cube_srv));EMIT(17,9);}
  if(test==6){cmd[0]=708;EMIT(4,1);memcpy(cmd,capture,sizeof(capture));EMIT(3,10);memset(cmd,0,sizeof(cmd));cmd[0]=708;cmd[6]=cmd[7]=4;cmd[8]=1;cmd[9]=8;cmd[10]=1;cmd[11]=8;cmd[12]=0x7e0;EMIT(13,14);}
  if(test==7){cmd[0]=711;EMIT(4,1);cube_srv[5]=0;memcpy(cmd,cube_srv,sizeof(cube_srv));EMIT(17,9);}
  uint32_t selected=test==1||test==2||test==4||test==6||invalid?709:711;
  if(test>=8){unsigned face=(test-8)%6,m=(test-8)/6;expected[test]=(face+m)&1?255:0;values[18*4]=values[18*4+1]=values[18*4+2]=0;values[18*4+face/2]=face&1?1:-1;uint32_t bc_srv[]={717,0,716,71,9,m,1,0,6};if(test>8){cmd[0]=717;EMIT(4,1);}memcpy(cmd,bc_srv,sizeof(bc_srv));EMIT(17,9);selected=717;}
  memset(cmd,0,sizeof(cmd));cmd[0]=714;cmd[2]=496;memcpy(cmd+3,values,sizeof(values));EMIT(12,127);cmd[0]=1;cmd[1]=14;cmd[2]=1;cmd[3]=selected;EMIT(24,4);memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)(dst+test*1024);cmd[10]=(dst+test*1024)>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
 }
 if(!invalid){memset(cmd,0,sizeof(cmd));cmd[0]=708;cmd[9]=(uint32_t)(dst+24*1024);cmd[10]=(dst+24*1024)>>32;cmd[11]=cmd[12]=cmd[14]=8;EMIT(42,17);}
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=888;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ready&&ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=888&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);if(legacy)ok=ready&&native_gpu_error(g)&&strstr(native_gpu_error(g),"SRV range outside texture:")&&strstr(native_gpu_error(g),"guard(3:8")&&strstr(native_gpu_error(g),"raw00000000")&&g->draw.draws==1;else if(invalid)ok=ready&&native_gpu_error(g)&&strstr(native_gpu_error(g),"SRV range outside texture:")&&strstr(native_gpu_error(g),"SRV 709 raw[")&&g->draw.draws==0&&!g->draw.proxy_bytes;
 else{uint32_t bytes[2];host_load(h,bytes,dst+24*1024,8);ok=bytes[0]==0x7e0&&bytes[1]==0&&ok&&!native_gpu_error(g)&&g->draw.draws==cases&&g->metrics.shadow_creates==1&&g->metrics.shadow_hits>=3;for(unsigned t=0;t<cases&&ok;t++){uint8_t pixels[1024];host_load(h,pixels,dst+t*1024,sizeof(pixels));for(unsigned i=0;i<256&&ok;i++)for(unsigned c=0;c<4;c++)if(pixels[i*4+c]!=(c==3?255:expected[t]))ok=false;}}
 if(count_control){if(report&&capacity)snprintf(report,capacity,"%s: signed light-count captured SRV replay (%s): %s",ok?"PASS":"FAIL",legacy?"0.22 proof reference":"corrected",native_gpu_error(g)?native_gpu_error(g):"20 cases/5120 pixels; no-light stale cube selector, signed count boundaries, active six-face cube and spot paths retained.");}
 else if(report&&capacity)snprintf(report,capacity,"%s: captured 709/708 SRV shadow replay: 20 cases, 5120 pixels; original uniform-controlled cube/spot code, valid six-face float/BC1 cubes and two mips, disabled/spot fallback equivalence, cache reuse and source ID reuse. Invalid case %u. %s",ok?"PASS":"FAIL",invalid,native_gpu_error(g)?native_gpu_error(g):"Synthetic upload/uniform/geometry inputs; no game scene claimed.");native_gpu_end(g);return ok;
}

static bool shadow_fixture(struct w2c_env *h,unsigned invalid,char *report,size_t capacity){return shadow_fixture_run(h,invalid,false,false,report,capacity);}
static bool shadow_count_fixture(struct w2c_env *h,bool legacy,char *report,size_t capacity){return shadow_fixture_run(h,0,true,legacy,report,capacity);}
