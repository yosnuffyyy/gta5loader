for(unsigned test=0;test<14&&ok;test++){
 uint8_t actual[1024];host_load(h,actual,dst+44000+test*1024,sizeof(actual));int red=(int)lroundf(proxy_expected[test]*255);
 for(unsigned i=0;i<256&&ok;i++)for(unsigned channel=0;channel<4;channel++){
  int expected=channel==0?red:channel==3?255:0;if(abs((int)actual[i*4+channel]-expected)>1){char why[240];snprintf(why,sizeof(why),"Depth proxy case %u pixel %u channel %u: got %u expected %d",test,i,channel,actual[i*4+channel],expected);fail(g,why);ok=false;break;}
 }
 if(ok)g->draw.proxy_cases++;
}
if(ok&&!check(g,g->draw.proxy_cases==14&&g->draw.proxy_allocations==5&&g->draw.proxy_refreshes==11&&g->draw.proxy_reuses>=1&&!g->draw.proxy_bytes&&!g->draw.proxy_passes,"Depth proxy refresh/reuse/lifetime counters failed"))ok=false;
