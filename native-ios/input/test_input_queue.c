#include "native_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t clock_ns=1;uint64_t ni_test_clock(void){return clock_ns;}
static uint8_t pad[56];
static uint32_t consume(void){ni_consume(pad);uint32_t b;memcpy(&b,pad+12,4);return b;}
static void begin(void){ni_end();consume();ni_begin();consume();ni_connect(true);ni_controller((NIState){0});}
int main(void){
 begin();ni_touch((NIState){.lx=.8f},true);clock_ns+=1000000;ni_touch((NIState){.lx=.8f,.rt=1},false);clock_ns+=1800000000;
 assert(consume()==NI_R2&&pad[20]>128&&pad[25]==255);NIMetrics m;ni_metrics(&m);assert(m.analog_skipped==1&&m.queued==0); /* old policy delivered movement only */
 ni_touch((NIState){.lx=.5f,.rt=1},false);ni_touch((NIState){.lx=.5f},false);assert(consume()==0&&pad[25]==0); /* release is not delayed by held analog */
 begin();ni_touch((NIState){.buttons=NI_START},true);ni_touch((NIState){0},false);assert(consume()==NI_START);assert(consume()==0); /* tap survives */
 ni_touch((NIState){.buttons=NI_CROSS},true);assert(consume()==NI_CROSS);ni_controller((NIState){.buttons=NI_SQUARE});assert(ni_source()==NI_CONTROLLER);assert(consume()==0);assert(consume()==NI_SQUARE); /* source release preserved */
 ni_focus(false);assert(consume()==0);ni_focus(true);assert(consume()==0);ni_touch((NIState){.buttons=NI_START},true);assert(consume()==NI_START);ni_touch((NIState){0},false);assert(consume()==0);
 begin();for(unsigned i=0;i<32;i++){ni_touch((NIState){.lx=.5f},true);ni_touch((NIState){.buttons=NI_CROSS,.lx=.5f},false);ni_touch((NIState){0},false);assert(consume()==NI_CROSS);assert(consume()==0);}
 begin();ni_touch((NIState){.buttons=NI_CROSS,.lx=-.8f,.rx=-.8f,.lt=.4f},true);
 ni_touch((NIState){.lx=.8f,.rx=.8f},false);clock_ns+=100000000;
 assert(consume()==(NI_CROSS|NI_L2)&&pad[20]>128&&pad[22]>128&&pad[24]==102); /* newest sticks, exact press/trigger */
 assert(consume()==0&&pad[20]>128&&pad[24]==0); /* release survives */
 ni_controller((NIState){.buttons=NI_SQUARE,.lx=-.8f});assert(consume()==0&&pad[20]==128&&pad[22]==128);assert(consume()==NI_SQUARE&&pad[20]<128);
 puts("PASS: latest stick state bypasses two queued digital snapshots without losing trigger pressure/edges; neutral handoff remains neutral. Simulated 100ms backlog, no physical latency claim.");
 puts("PASS: controlled-clock input queue: movement+fire and release need 1 hook instead of 2 after a 1.8s simulated stall; tap edges, handoff neutral, focus release and32 repeated sequences preserved. No input is injected outside the original engine hook; not device latency.");return 0;
}
