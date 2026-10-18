#ifdef NATIVE_PROBE_CLI
static uint64_t ordered_baseline_submits,ordered_baseline_bytes;
static bool ordered_fixture(struct w2c_env *h,bool legacy,char *report,size_t capacity){
 uint64_t base=atomic_load(&h->memory.size)-4194304,dst=base+3145728,flag=base+3144000,qaddr=base+3143000;
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;g->draw.fixture=true;g->test_legacy_ordered_upload=legacy;g->test_drain_controlled_clock=true;g->test_measure_draw=true;
 host_put32(h,base,64);native_gpu_start(g,base,2097152,0);uint64_t deadline=now_ns()+60000000000ull;
 while(!native_gpu_ready(g)&&!native_gpu_error(g)&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}
 bool ok=native_gpu_ready(g);uint32_t tail=64,cmd[16387];
 #define EMIT(op,count) do{if(tail>2097000-8-(count)*4){ok=false;fail(g,"Ordered replay overflow");}else{host_put32(h,base+tail,op);host_put32(h,base+tail+4,count);host_store(h,base+tail+8,cmd,(count)*4);tail+=8+(count)*4;}}while(0)
 #include "query_setup.inc"
 cmd[0]=105;EMIT(29,1);cmd[0]=11165;cmd[1]=1;cmd[2]=(uint32_t)qaddr;cmd[3]=qaddr>>32;EMIT(44,4);EMIT(45,1);
 /* Rewrite the same live vertex/color bytes between each draw. Previously
    every such mutation submitted the encoder. The pixel captures every version. */
 for(unsigned i=0;i<256;i++){
  cmd[0]=109;cmd[1]=12;cmd[2]=4;cmd[3]=0xff000000|((i&255)<<16)|(((255-i)&255)<<8)|255;EMIT(i&1?47:12,4);
  for(unsigned v=1;v<6;v++){cmd[1]=v*24+12;EMIT(12,4);}
  cmd[0]=1;cmd[1]=i%16;cmd[2]=i/16;cmd[3]=cmd[1]+1;cmd[4]=cmd[2]+1;EMIT(31,5);
  memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);
 }
 /* More than two arena rotations, then an unaligned mutation. The readback
    must see both the final encoded copy and its later exact byte edges. */
 cmd[0]=119;cmd[1]=65536;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);
 for(unsigned j=0;j<20;j++){cmd[0]=119;cmd[1]=0;cmd[2]=65536;for(unsigned k=0;k<16384;k++)cmd[k+3]=0x10000000+j*0x10000+k;EMIT(12,16387);}
 cmd[0]=119;cmd[1]=1;cmd[2]=3;cmd[3]=0x00ccbbaa;EMIT(12,4);
 cmd[0]=11165;cmd[1]=123;EMIT(46,2);
 memset(cmd,0,17*4);cmd[0]=119;cmd[9]=(uint32_t)(dst+2048);cmd[10]=(dst+2048)>>32;cmd[13]=1;cmd[14]=65536;EMIT(42,17);
 cmd[0]=119;EMIT(4,1);cmd[0]=119;cmd[1]=16;cmd[2]=1;cmd[3]=cmd[4]=0;EMIT(11,5);
 cmd[0]=119;cmd[1]=0;cmd[2]=16;for(unsigned k=0;k<4;k++)cmd[k+3]=0x12340000+k;EMIT(12,7);
 memset(cmd,0,17*4);cmd[0]=119;cmd[9]=(uint32_t)(dst+2048+65536);cmd[10]=(dst+2048+65536)>>32;cmd[13]=1;cmd[14]=16;EMIT(42,17);
 memset(cmd,0,sizeof(cmd));cmd[0]=116;cmd[9]=(uint32_t)dst;cmd[10]=dst>>32;cmd[11]=64;cmd[12]=cmd[14]=1024;EMIT(42,17);
 memset(cmd,0,sizeof(cmd));cmd[0]=109;cmd[9]=(uint32_t)(dst+1024);cmd[10]=(dst+1024)>>32;cmd[13]=1;cmd[14]=288;EMIT(42,17);
 cmd[0]=(uint32_t)flag;cmd[1]=flag>>32;cmd[2]=234;host_put32(h,flag,0);EMIT(8,3);
 #undef EMIT
 uint64_t start=now_ns();if(ok){atomic_store_explicit(heap_word(g,base),tail,memory_order_release);wake(g,base);while(!native_gpu_error(g)&&atomic_load_explicit(heap_word(g,flag),memory_order_acquire)!=234&&now_ns()<deadline){struct timespec t={0,1000000};nanosleep(&t,NULL);}}
 native_gpu_stop(g);uint64_t elapsed=now_ns()-start;
 ok=ok&&!native_gpu_error(g)&&atomic_load(heap_word(g,flag))==234&&atomic_load(heap_word(g,qaddr))==123&&host_u64(h,qaddr+8)==256;
 unsigned char pixels[1024],buffer[288];host_load(h,pixels,dst,sizeof(pixels));host_load(h,buffer,dst+1024,sizeof(buffer));
 for(unsigned i=0;i<256&&ok;i++){int expected[]={200,(int)lroundf(100*(255-i)/255.0),(int)lroundf(50*i/255.0),255};for(unsigned c=0;c<4;c++)if(abs(pixels[4*i+c]-expected[c])>1)ok=false;}
 for(unsigned v=0;v<6;v++){uint32_t color;memcpy(&color,buffer+v*24+12,4);if(color!=0xffff00ff)ok=false;}
 for(unsigned k=0;k<16384;k++){uint32_t value;host_load(h,&value,dst+2048+k*4,4);if(value!=(k?0x10130000+k:0xccbbaa00))ok=false;}
 for(unsigned k=0;k<4;k++){uint32_t value;host_load(h,&value,dst+2048+65536+k*4,4);if(value!=0x12340000+k)ok=false;}
 if(!legacy&&g->ordered_capacity_flushes<2)ok=false;
 if(legacy){ordered_baseline_submits=g->submissions;ordered_baseline_bytes=g->draw.uniform_upload_bytes;}else ok=ok&&g->submissions<ordered_baseline_submits&&g->draw.uniform_upload_bytes<ordered_baseline_bytes&&g->ordered_uploads>1400;
 char timing[400];timing_text(&g->test_draw_times,"Original-shader DRAW wall (host only)",timing,sizeof(timing));
 snprintf(report,capacity,"%s: ordered upload legacy %u: 256 original-shader draws/pixels and occlusion samples; exact VB and 64-KiB bytes, arena rotation, unaligned ordering and resource ID reuse; submissions/batches %llu/%llu, uniform bytes %llu; ordered copies/bytes %llu/%llu; arena bound 512 KiB CPU + 1 MiB GPU, existing 16-batch/4-MiB queue pressure preserved; wall %.3f ms, handler/upload %.3f/%.3f ms, completions %llu.\n%s\n%s",ok?"PASS":"FAIL",legacy,(unsigned long long)g->submissions,(unsigned long long)g->draw.batches,(unsigned long long)g->draw.uniform_upload_bytes,(unsigned long long)g->ordered_uploads,(unsigned long long)g->ordered_bytes,elapsed/1e6,g->metrics.frame_busy/1e6,g->metrics.frame_upload/1e6,(unsigned long long)g->metrics.windows[METRIC_COMPLETION].count,timing,native_gpu_error(g)?native_gpu_error(g):"Host Metal pixel/byte test, not iPhone FPS.");native_gpu_end(g);return ok;
}
#endif
