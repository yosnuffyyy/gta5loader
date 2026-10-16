/* Three independent shadow contributions: two known placeholders and one
   real comparison shadow. Binding65 exercises texture slots above31 and
   copying reference selection into linked inactive-binding variants. */
static bool shadow_semantic_fixture(struct w2c_env *h){
 uint64_t base=atomic_load(&h->memory.size)-262144,dst=base+131072,flag=base+128000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;
 g->draw.fixture=true;g->reference_shadow_compat=true;host_put32(h,base,64);native_gpu_start(g,base,131072,0);
 uint64_t deadline=now_ns()+45000000000ull;while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[128];
 #define EMIT(op,count) do{if((count)>128||tail>127488-8-(count)*4){ok=false;fail(g,"Semantic shadow fixture ring overflow");}else{host_put32(h,base+tail,(op));host_put32(h,base+tail+4,(count));host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 cmd[0]=901;cmd[1]=1;cmd[2]=0x224;cmd[3]=UINT32_MAX;cmd[4]=0;EMIT(14,5);cmd[0]=1;cmd[1]=901;EMIT(18,2);
 uint32_t sampler[]={3511,3,20,3,3,1,0,1,1,0,0,0,0,0,1094713344};memcpy(cmd,sampler,sizeof(sampler));EMIT(16,15);
 uint32_t compare[]={902,3,0x80,3,3,3,0,0,4,0,0,0,0,0,0x42000000};memcpy(cmd,compare,sizeof(compare));EMIT(16,15);
 uint32_t samplers[]={1,13,2,902,3511};memcpy(cmd,samplers,sizeof(samplers));EMIT(25,5);
 uint32_t texture[]={708,2,4,4,1,1,1,71,8,1},view[]={709,0,708,71,4,0,1,0,0};
 memcpy(cmd,texture,sizeof(texture));EMIT(3,10);memset(cmd,0,sizeof(cmd));cmd[0]=708;cmd[6]=cmd[7]=4;cmd[8]=1;cmd[9]=8;cmd[10]=1;cmd[11]=8;EMIT(13,14);memcpy(cmd,view,sizeof(view));EMIT(17,9);
 uint32_t cube[]={710,2,4,4,1,1,6,41,0x28,1},cube_view[]={711,0,710,41,9,0,1,0,6};memcpy(cmd,cube,sizeof(cube));EMIT(3,10);memcpy(cmd,cube_view,sizeof(cube_view));EMIT(17,9);
 cmd[0]=714;cmd[1]=496;cmd[2]=4;cmd[3]=cmd[4]=0;EMIT(11,5);
 float values[124]={0};values[60]=values[65]=values[70]=1;values[71]=.5f;values[74]=-1;values[92]=values[97]=values[102]=1;values[103]=.5f;values[104]=-1;values[95]=2;
 memset(cmd,0,sizeof(cmd));cmd[0]=714;cmd[2]=496;memcpy(cmd+3,values,sizeof(values));EMIT(12,127);uint32_t cb[]={1,6,1,714,0,31};memcpy(cmd,cb,sizeof(cb));EMIT(23,6);
 for(unsigned i=0;i<12;i++){
  if(i==6){texture[0]=1800;view[0]=1801;view[2]=1800;memcpy(cmd,texture,sizeof(texture));EMIT(3,10);memset(cmd,0,sizeof(cmd));cmd[0]=1800;cmd[6]=cmd[7]=4;cmd[8]=1;cmd[9]=8;cmd[10]=1;cmd[11]=8;EMIT(13,14);memcpy(cmd,view,sizeof(view));EMIT(17,9);}
  for(unsigned face=0;face<6;face++){memset(cmd,0,sizeof(cmd));cmd[0]=710;cmd[2]=face;float depth=i%2?.75f:.25f;memcpy(cmd+3,&depth,4);EMIT(5,7);}
  cmd[0]=714;cmd[1]=63*4;cmd[2]=4;cmd[3]=i%3?0x40000000:0;EMIT(12,4);cmd[1]=79*4;cmd[3]=i%4>=2?0x40000000:0;EMIT(12,4);
  uint32_t slots[]={33,25,27},views[]={view[0],view[0],711};for(unsigned j=0;j<3;j++){cmd[0]=1;cmd[1]=slots[j];cmd[2]=1;cmd[3]=views[j];EMIT(24,4);}
  memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
  memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)(dst+i*1024);cmd[10]=(dst+i*1024)>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
 }
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=888;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=888&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);ok=ok&&!native_gpu_error(g)&&g->draw.draws==12&&g->draw.creates==4&&g->draw.hits==8&&g->reference_shadow_uses==14&&g->reference_shadow_allocations==1;
 for(unsigned i=0;i<12&&ok;i++){uint8_t pixels[1024];host_load(h,pixels,dst+i*1024,sizeof(pixels));unsigned expected=i%2?255:170;for(unsigned j=0;j<256&&ok;j++)for(unsigned c=0;c<4;c++)if(pixels[j*4+c]!=(c==3?255:expected))ok=false;}
 printf("%s: semantic multi-shadow:12 draws/3072 checked pixels; two placeholders, independent real Less shadow, binding65/linked variants, handle replacement, one cube,14 reference uses,4 pipelines/8 hits. %s\n",ok?"PASS":"FAIL",native_gpu_error(g)?native_gpu_error(g):"No scene-pixel or phone FPS claim.");
 native_gpu_end(g);return ok;
}
