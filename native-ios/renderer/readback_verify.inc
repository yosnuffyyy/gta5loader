for(unsigned test=0;test<30&&ok;test++){
 uint32_t row=rb_formats[test].bw==4?rb_formats[test].bytes:4*rb_formats[test].bytes,rows=rb_formats[test].bw==4?1:4;
 uint8_t actual[320],expected[320];memset(expected,0xa5,sizeof(expected));host_load(h,actual,dst+20000+test*320,sizeof(actual));
 for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<row;x++)expected[16+y*(row+4)+x]=(uint8_t)((y*row+x)*37+test*13+19);
 if(memcmp(actual,expected,sizeof(actual))){snprintf(g->error,sizeof(g->error),"Readback DXGI %u raw data/stride/guard mismatch",rb_formats[test].dxgi);atomic_store(&g->failed,true);ok=false;}else g->readback_cases_tested++;
}
for(unsigned test=0;test<2&&ok;test++){
 uint8_t actual[128],expected[128];memset(expected,0xa5,sizeof(expected));host_load(h,actual,dst+30000+test*160,sizeof(actual));
 for(unsigned z=0;z<(test?2:1);z++)for(unsigned y=0;y<2;y++)for(unsigned x=0;x<8;x++)expected[16+z*56+y*20+x]=(uint8_t)((test?(z+1)*64:0)+(y+1)*16+4+x+113);
 if(memcmp(actual,expected,sizeof(actual))){snprintf(g->error,sizeof(g->error),"Readback %s box/stride/guard mismatch",test?"3D":"array mip");atomic_store(&g->failed,true);ok=false;}else g->readback_cases_tested++;
}
if(ok){uint32_t bc[2];host_load(h,bc,dst+30400,8);ok=bc[0]==0x12345678&&bc[1]==0xabcdef09;if(ok)g->readback_cases_tested++;}
if(ok){uint8_t row[8192];for(unsigned y=0;y<129&&ok;y++){host_load(h,row,65536+(uint64_t)y*8192,sizeof(row));for(unsigned x=0;x<sizeof(row);x++)if(row[x]!=(x%2?255:0)){ok=false;break;}}if(ok)g->readback_cases_tested++;}
if(ok){uint8_t bytes[8192];for(uint32_t at=0;at<READBACK_STAGING_MAX+19&&ok;){uint32_t n=READBACK_STAGING_MAX+19-at;if(n>sizeof(bytes))n=sizeof(bytes);host_load(h,bytes,1200000+at,n);for(unsigned i=0;i<n;i++){uint32_t pos=at+i;uint8_t expected=pos>=READBACK_STAGING_MAX&&pos<READBACK_STAGING_MAX+16?(uint8_t)((pos-READBACK_STAGING_MAX)*3+17):0;if(bytes[i]!=expected){ok=false;break;}}at+=n;}if(ok)g->readback_cases_tested++;}
if(ok){uint8_t bytes[32];host_load(h,bytes,dst+30500,sizeof(bytes));for(unsigned i=0;i<32;i++)if(bytes[i]!=(i>=8&&i<19?(uint8_t)((i-8+4)*3+17):0xa5))ok=false;if(atomic_load(heap_word(g,flag+24))!=0)ok=false;if(ok)g->readback_cases_tested++;}
if(ok&&g->readback_capacity==READBACK_STAGING_MAX&&g->readback_allocations==4&&g->readback_cases_tested==36){}else{ok=false;if(!g->error[0])snprintf(g->error,sizeof(g->error),"Readback replay/pool verification failed: %u cases, %u capacity, %u allocations",g->readback_cases_tested,g->readback_capacity,g->readback_allocations);atomic_store(&g->failed,true);}
