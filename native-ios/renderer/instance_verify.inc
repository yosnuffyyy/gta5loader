for(unsigned test=0;test<10&&ok;test++){
 uint8_t actual[1024],expected[1024]={0};host_load(h,actual,dst+32000+test*1024,sizeof(actual));
 unsigned rate=instance_cases[test].layout==306?0:instance_cases[test].layout==307||instance_cases[test].layout==309?2:instance_cases[test].layout==308?3:1;
 unsigned count=test==8?4:instance_cases[test].count;
 for(unsigned i=0;i<count;i++){
  unsigned record=instance_cases[test].stride?(test==8?6:instance_cases[test].first)+(rate?i/rate:0):0;
  unsigned pixel=8*16+2*(instance_cases[test].first+i)+1;
  expected[pixel*4]=instance_colors[record][0]?200:0;expected[pixel*4+1]=instance_colors[record][1]?100:0;
  expected[pixel*4+2]=test==5?((instance_cases[test].first+i/3)%2?50:0):25;expected[pixel*4+3]=(uint8_t)lroundf((i+1)*255.f/8);
 }
 for(unsigned i=0;i<sizeof(actual);i++)if(abs((int)actual[i]-expected[i])>1){char why[240];snprintf(why,sizeof(why),"Instance case %u pixel %u channel %u: got %u expected %u",test,i/4,i%4,actual[i],expected[i]);fail(g,why);ok=false;break;}
 if(ok)g->instance_cases_tested++;
}
if(ok&&!check(g,g->draw.instance_draws==9&&g->draw.instance_batches==17&&g->draw.indirect_decodes==2&&g->instance_cases_tested==10,"Instance path/counter verification failed"))ok=false;
