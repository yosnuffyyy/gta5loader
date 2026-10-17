#ifdef NATIVE_PROBE_CLI
/* Real original constructor, buddy allocation, free and accounting. The native growth policy and original availability contract are tested.
   No synthetic allocator success. */
static uint64_t memory_test_create(struct w2c_env *h,uint64_t budget){uint64_t p=w2c_gta_f91000(h->engine,20000);require(h,p&&host_range(h,p,20000),"memory fixture allocator storage");w2c_gta_f3035(h->engine,p,budget);return p;}
static void memory_test_destroy(struct w2c_env *h,uint64_t p){for(unsigned i=0;i<32;i++){uint64_t meta=host_u64(h,p+16128+i*8),data=host_u64(h,p+16384+i*8);if(data)w2c_gta_f91002(h->engine,data);if(meta)w2c_gta_f91002(h->engine,meta);}w2c_gta_f91002(h->engine,p);}
static void native_allocator_fixture(struct w2c_env *h){
 const uint64_t request=37314560,budget=536870912;
 uint64_t root=host_u64(h,h->engine->w2c_g11),resource=w2c_gta_f3002(h->engine,root,1);require(h,resource&&host_u64(h,resource)==4181424,"real resource allocator is not pinned GrowBuddy");uint64_t before=w2c_gta_f3040(h->engine,resource,UINT32_MAX),rounded=w2c_gta_f2980(h->engine,root,67108864,128,1);require(h,rounded&&host_range(h,rounded,67108864),"actual resource-class rounded funeral allocation failed");w2c_gta_f3039(h->engine,resource,rounded);require(h,w2c_gta_f3040(h->engine,resource,UINT32_MAX)==before,"actual resource-class free accounting changed");message(h,"PASS: actual engine resource allocator class1 at 0x%llx: 37,314,560-byte resource-map demand rounds to 64 MiB; real root Allocate/child Free and original live accounting verified.",(unsigned long long)resource);
 uint64_t old=memory_test_create(h,budget);
 require(h,native_stream_try_legacy(h->engine,old,request,4096,0)==0,"legacy 32 MiB segment should reproduce funeral allocation failure");
 require(h,w2c_gta_f3040(h->engine,old,UINT32_MAX)==0&&w2c_gta_f3042(h->engine,old)<=w2c_gta_f3041(h->engine,old),"legacy failure accounting changed");
 uint64_t old_largest=w2c_gta_f3043(h->engine,old);memory_test_destroy(h,old);
 uint64_t p=memory_test_create(h,budget),peak=0;unsigned allocations=0;
 for(unsigned pass=0;pass<64;pass++){
  uint64_t held[8]={0};for(unsigned i=0;i<8;i++){held[i]=w2c_gta_f3038(h->engine,p,(i+1)*8192,4096,0);require(h,held[i]!=0,"fragment fixture small allocation");}
  for(unsigned i=0;i<8;i+=2)w2c_gta_f3039(h->engine,p,held[i]);
  uint64_t v=w2c_gta_f3038(h->engine,p,request,4096,0);require(h,v&&host_range(h,v,request),"request-aware growth did not satisfy funeral-sized request");
  unsigned char expected[64],actual[64];memset(expected,pass+1,sizeof(expected));host_store(h,v,expected,64);host_store(h,v+request-64,expected,64);
  host_load(h,actual,v,64);require(h,!memcmp(actual,expected,64),"large allocation head data corrupted");host_load(h,actual,v+request-64,64);require(h,!memcmp(actual,expected,64),"large allocation tail data corrupted");
  if(pass==0){host_put64(h,p+488,16777216);uint64_t actual=w2c_gta_f3042(h->engine,p),heap_bytes=w2c_gta_f3041(h->engine,p),live=w2c_gta_f3040(h->engine,p,UINT32_MAX);require(h,actual==heap_bytes-live&&actual<UINT64_C(4294967296),"available-space underflow or fictitious capacity");host_put64(h,p+488,budget);}
  uint64_t used=w2c_gta_f3040(h->engine,p,UINT32_MAX);if(used>peak)peak=used;w2c_gta_f3039(h->engine,p,v);for(unsigned i=1;i<8;i+=2)w2c_gta_f3039(h->engine,p,held[i]);
  require(h,w2c_gta_f3040(h->engine,p,UINT32_MAX)==0,"scene transition cleanup retains live allocations");allocations+=9;
 }
 require(h,host_u64(h,p+488)==budget,"request-aware growth changed original growth target");uint64_t heap=w2c_gta_f3041(h->engine,p);memory_test_destroy(h,p);
 p=memory_test_create(h,16777216);require(h,w2c_gta_f3038(h->engine,p,request,4096,0)==0,"insufficient original growth target must still fail");require(h,w2c_gta_f3040(h->engine,p,UINT32_MAX)==0,"failed allocation retained live bytes");memory_test_destroy(h,p);
 for(unsigned legacy=0;legacy<2;legacy++){
  uint64_t owner=memory_test_create(h,UINT64_C(1073741824)),held[96];for(unsigned i=0;i<96;i++){if(!legacy)w2c_gta_f3043(h->engine,owner);held[i]=legacy?native_stream_try_legacy(h->engine,owner,8388608,4096,0):w2c_gta_f3038(h->engine,owner,8388608,4096,0);require(h,held[i]!=0,"long-session eight-MiB allocation failed early");}
  uint32_t segments;host_load(h,&segments,owner+508,4);uint64_t v=legacy?native_stream_try_legacy(h->engine,owner,request,4096,0):w2c_gta_f3038(h->engine,owner,request,4096,0);require(h,legacy?!v:!!v,"fixed-slot long-session large transition differs");if(legacy)require(h,segments==32,"legacy segment-slot failure not reproduced");else require(h,segments<32,"candidate lost slot reserve");
  if(v)w2c_gta_f3039(h->engine,owner,v);for(unsigned i=0;i<96;i++)w2c_gta_f3039(h->engine,owner,held[i]);require(h,w2c_gta_f3040(h->engine,owner,UINT32_MAX)==0,"long-session cleanup retains live allocations");if(!legacy)require(h,host_u64(h,owner+488)==UINT64_C(1073741824),"long-session growth target changed");message(h,"PASS: original allocator slot-pressure legacy%u: 96 held eight-MiB objects, segment slots before transition %u/32; funeral allocation %s; initial 1 GiB growth target and complete cleanup; legacy failure recalibration preserved.",legacy,segments,v?"succeeded":"safely failed");memory_test_destroy(h,owner);
 }
 message(h,"PASS: original allocator funeral fixture: request %llu, legacy largest %llu; 64 fragmented transitions / %u allocations, exact head/tail bytes, full live cleanup, same %llu growth target; candidate retained heap %llu, peak live %llu; insufficient segment growth remains failure. No phone-memory or next-scene claim.",(unsigned long long)request,(unsigned long long)old_largest,allocations,(unsigned long long)budget,(unsigned long long)heap,(unsigned long long)peak);
}
#endif
