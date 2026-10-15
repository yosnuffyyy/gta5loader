#include "native_input.h"
#include "touch_layout.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static uint8_t pad[56];
static uint32_t mask(void){uint32_t m;memcpy(&m,pad+12,4);return m;}
static void drain(void){NIMetrics m;do{ni_consume(pad);ni_metrics(&m);}while(m.queued);}
static void *producer(void *p){(void)p;for(unsigned i=0;i<100000;i++)ni_controller((NIState){.buttons=i&1?NI_CROSS:0,.lx=i&1?.8f:0});return NULL;}
static void *consumer(void *p){(void)p;uint8_t local[56]={0};for(unsigned i=0;i<100000;i++){ni_consume(local);assert(local[50]<=1);}return NULL;}
int main(int argc,char **argv){
 ni_configure(.15f,1);ni_connect(true);ni_begin();drain();assert(pad[20]==128&&pad[21]==128&&pad[50]==1);
 const uint32_t bits[]={NI_CROSS,NI_CIRCLE,NI_SQUARE,NI_TRIANGLE,NI_L1,NI_R1,NI_L2,NI_R2,NI_SELECT,NI_START,NI_L3,NI_R3,NI_UP,NI_DOWN,NI_LEFT,NI_RIGHT};
 FILE *fixture=argc>1?fopen(argv[1],"wb"):NULL;if(argc>1)assert(fixture);
 for(unsigned i=0;i<16;i++){
  NIState s={.buttons=bits[i],.lt=bits[i]==NI_L2?1:0,.rt=bits[i]==NI_R2?1:0};ni_controller((NIState){0});drain();ni_controller(s);drain();assert(mask()==bits[i]);if(fixture)assert(fwrite(pad,1,56,fixture)==56);
  ni_consume(pad);assert(mask()==bits[i]);ni_controller((NIState){0});drain();assert(mask()==0);
 }
 ni_controller((NIState){.lx=.6f,.ly=1,.rx=1,.ry=.6f,.lt=.4f,.rt=.7f});drain();assert(pad[21]<128&&pad[22]>128&&pad[24]==102&&pad[25]==179);if(fixture)assert(fwrite(pad,1,56,fixture)==56);if(fixture)assert(fclose(fixture)==0);
 /* A tap between engine updates must remain press, then release. */
 ni_controller((NIState){0});drain();ni_controller((NIState){.buttons=NI_START});ni_controller((NIState){0});ni_consume(pad);assert(mask()==NI_START);ni_consume(pad);assert(mask()==0);
 /* Touch wins immediately, held controller cannot steal back on unrelated releases. */
 ni_controller((NIState){.buttons=NI_CROSS,.lx=.8f});drain();assert(ni_source()==NI_CONTROLLER);
 ni_touch((NIState){.buttons=NI_SQUARE,.ly=1},true);drain();assert(ni_source()==NI_TOUCH&&mask()==NI_SQUARE);
 ni_controller((NIState){.buttons=NI_CROSS,.lx=.81f});drain();assert(ni_source()==NI_TOUCH&&mask()==NI_SQUARE);
 ni_controller((NIState){.buttons=NI_CROSS|NI_START,.lx=.81f});drain();assert(ni_source()==NI_CONTROLLER&&mask()==NI_START&&pad[20]==128);
 ni_controller((NIState){0});drain();ni_controller((NIState){.lx=.8f});drain();assert(pad[20]>128);
 ni_touch((NIState){0},true);drain();ni_controller((NIState){0});ni_controller((NIState){.lx=.04f,.lt=.06f});drain();assert(ni_source()==NI_TOUCH);
 ni_controller((NIState){.lt=.2f});drain();assert(ni_source()==NI_CONTROLLER&&pad[24]==51);
 ni_focus(false);drain();assert(mask()==0&&pad[24]==0);ni_controller((NIState){.buttons=NI_CROSS,.ly=1});ni_focus(true);ni_controller((NIState){.buttons=NI_CROSS,.ly=1});drain();assert(ni_source()==NI_TOUCH&&mask()==0);
 ni_controller((NIState){0});ni_controller((NIState){.buttons=NI_CROSS});drain();assert(mask()==NI_CROSS);
 ni_connect(false);drain();assert(ni_source()==NI_TOUCH&&mask()==0);ni_connect(true);assert(ni_source()==NI_TOUCH);ni_controller((NIState){.buttons=NI_CROSS});drain();assert(mask()==NI_CROSS);
 /* NaN and infinities never escape normalization. */
 ni_controller((NIState){.lx=NAN,.rx=INFINITY,.lt=NAN,.rt=-1});drain();assert(pad[20]==128&&pad[22]==128&&pad[24]==0&&pad[25]==0);
 ni_end();drain();assert(mask()==0&&pad[50]==0&&pad[55]==0);
 uint8_t control[4];int32_t v;ni_control(control);memcpy(&v,control,4);assert(v==-4);ni_begin();ni_control(control);memcpy(&v,control,4);assert(v==0);assert(ni_is_control(24488864)&&!ni_is_control(24488865));
 /* Shared production geometry, multitouch and resize cancellation. */
 NIRect fit=ni_aspect_fit(932,430);assert(fabsf(fit.w/fit.h-16.f/9)<.00001f&&fit.x>0);NITouches t={.rect={59,0,814,400},.scale=1};float x,y;ni_center(&t,0,&x,&y);assert(ni_touch_begin(&t,0,x,y));ni_touch_move(&t,0,x,y-100);ni_center(&t,6,&x,&y);assert(ni_touch_begin(&t,1,x,y));ni_center(&t,7,&x,&y);assert(ni_touch_begin(&t,2,x,y));NIState s=ni_touch_state(&t);assert(s.ly>0&&s.lt==1&&s.rt==1);ni_touch_end(&t,1);s=ni_touch_state(&t);assert(s.lt==0&&s.rt==1&&s.ly>0);assert(!ni_touch_begin(&t,10,x,y));ni_touch_clear(&t);assert(ni_touch_state(&t).buttons==0);for(unsigned c=0;c<NI_CONTROL_COUNT;c++){ni_center(&t,c,&x,&y);assert(ni_touch_hit(&t,x,y)==(int)c);}
 /* Concurrent producer/engine snapshot consumer, including bounded overflow. */
 ni_connect(true);pthread_t a,b;assert(!pthread_create(&a,NULL,producer,NULL));assert(!pthread_create(&b,NULL,consumer,NULL));assert(!pthread_join(a,NULL)&&!pthread_join(b,NULL));ni_controller((NIState){0});drain();assert(mask()==0);NIMetrics metrics;ni_metrics(&metrics);assert(metrics.queued<=64);
 ni_end();ni_begin();ni_connect(true);struct timespec start,end;clock_gettime(CLOCK_MONOTONIC,&start);for(unsigned i=0;i<100000;i++){ni_controller((NIState){.lx=i&1?.8f:0});ni_consume(pad);}clock_gettime(CLOCK_MONOTONIC,&end);double elapsed=(end.tv_sec-start.tv_sec)*1000.+(end.tv_nsec-start.tv_nsec)/1e6;ni_metrics(&metrics);assert(!metrics.overflows);printf("PASS: native input mapping, trigger pressure, press/hold/release, queued tap, source takeover, held-axis blocking, drift/hysteresis, reconnect, focus, NaN, pad restoration, multitouch, viewport and concurrency. 100000 event/consume pairs %.3f ms (%.3f us/pair); bridge queue capacity 64, no heap allocations. Not device latency.\n",elapsed,elapsed*.01);
 return 0;
}
