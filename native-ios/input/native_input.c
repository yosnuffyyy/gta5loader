#include "native_input.h"
#include <pthread.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#define NI_QUEUE 64
typedef struct {NIState state;uint64_t at;} NIEvent;
static struct {
 pthread_mutex_t lock;
 NIState physical,output;NIEvent queue[NI_QUEUE];unsigned head,count;uint32_t delivered_buttons;
 uint32_t blocked;bool connected,running,focused,baseline,stick_ready[2],stick_blocked[2],trigger_ready[2],pad_saved;
 uint8_t saved50,saved55;NISource source;
 float deadzone,sensitivity;NIMetrics metrics;
} input={.lock=PTHREAD_MUTEX_INITIALIZER,.deadzone=.15f,.sensitivity=1};
static void lock(void){if(pthread_mutex_lock(&input.lock))abort();}
static void unlock(void){if(pthread_mutex_unlock(&input.lock))abort();}
#if defined(NI_TEST_CLOCK)
extern uint64_t ni_test_clock(void);
static uint64_t ns(void){return ni_test_clock();}
#else
static uint64_t ns(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))abort();return (uint64_t)t.tv_sec*1000000000ull+(uint64_t)t.tv_nsec;}
#endif
static float finite_clamp(float x,float lo,float hi){return !isfinite(x)?0:fminf(hi,fmaxf(lo,x));}
static void stick(float *x,float *y){*x=finite_clamp(*x,-1,1);*y=finite_clamp(*y,-1,1);float m=hypotf(*x,*y);if(m<input.deadzone){*x=*y=0;}else if(m>1){*x/=m;*y/=m;}}
static NIState clean(NIState s){s.buttons&=65535;stick(&s.lx,&s.ly);stick(&s.rx,&s.ry);s.lt=finite_clamp(s.lt,0,1);s.rt=finite_clamp(s.rt,0,1);/* Trigger threshold applies consistently to both sources. */s.buttons&=~(NI_L2|NI_R2);if(s.lt>.05f)s.buttons|=NI_L2;if(s.rt>.05f)s.buttons|=NI_R2;return s;}
static void enqueue(NIState s){
 if(!memcmp(&s,&input.output,sizeof(s)))return;
 /* Coalesce analog motion only; never erase a queued digital edge. */
 if(input.count&&input.queue[(input.head+input.count-1)%NI_QUEUE].state.buttons==s.buttons){uint64_t first=input.queue[(input.head+input.count-1)%NI_QUEUE].at;input.queue[(input.head+input.count-1)%NI_QUEUE]=(NIEvent){s,first?first:ns()};}
 else {
  if(input.count==NI_QUEUE){/* Fail safe on overload: release before latest state, report lost history. */input.head=0;input.count=1;input.queue[0]=(NIEvent){.at=ns()};input.metrics.overflows++;}
  input.queue[(input.head+input.count++)%NI_QUEUE]=(NIEvent){s,ns()};
 }
 input.output=s;
}
static void neutral(void){input.head=input.count=0;input.output=(NIState){0};input.queue[input.count++]=(NIEvent){0};}
static void rearm(void){input.blocked=input.physical.buttons;float m[2]={hypotf(input.physical.lx,input.physical.ly),hypotf(input.physical.rx,input.physical.ry)};for(unsigned i=0;i<2;i++){input.stick_blocked[i]=m[i]>=input.deadzone;input.stick_ready[i]=!input.stick_blocked[i];}input.trigger_ready[0]=input.physical.lt<.03f;input.trigger_ready[1]=input.physical.rt<.03f;}
static void switch_to(NISource s){if(s==input.source)return;neutral();input.source=s;input.metrics.switches++;if(s==NI_TOUCH)rearm();}
void ni_begin(void){lock();input.running=true;input.focused=true;input.baseline=input.connected;input.source=NI_TOUCH;input.metrics=(NIMetrics){0};input.pad_saved=false;rearm();neutral();unlock();}
void ni_end(void){lock();input.running=false;input.focused=false;rearm();neutral();unlock();}
void ni_focus(bool focused){lock();input.focused=focused;input.baseline=input.connected;rearm();neutral();input.source=NI_TOUCH;unlock();}
void ni_connect(bool connected){lock();input.connected=connected;input.baseline=connected;input.physical=(NIState){0};rearm();/* Release a disconnected physical source, but preserve an already active touch hold. */if(!connected)switch_to(NI_TOUCH);unlock();}
void ni_configure(float deadzone,float sensitivity){lock();input.deadzone=isfinite(deadzone)?finite_clamp(deadzone,.05f,.4f):.15f;input.sensitivity=isfinite(sensitivity)?finite_clamp(sensitivity,.25f,2):1;unlock();}
NISource ni_source(void){lock();NISource s=input.source;unlock();return s;}
NISource ni_controller(NIState raw){
 lock();NIState s=clean(raw),old=input.physical;input.physical=s;input.metrics.events++;
 if(input.baseline){input.baseline=false;rearm();NISource mode=input.source;unlock();return mode;}
 if(!input.running||!input.focused||!input.connected){rearm();NISource mode=input.source;unlock();return mode;}
 input.blocked&=s.buttons;uint32_t rising=s.buttons&~old.buttons&~input.blocked;
 bool activity=(rising&~(NI_L2|NI_R2))!=0;float magnitude[2]={hypotf(s.lx,s.ly),hypotf(s.rx,s.ry)};
 for(unsigned i=0;i<2;i++){if(magnitude[i]<input.deadzone){input.stick_ready[i]=true;input.stick_blocked[i]=false;}else if(input.stick_ready[i]&&magnitude[i]>input.deadzone+.08f){activity=true;input.stick_ready[i]=false;}}
 float triggers[2]={s.lt,s.rt};for(unsigned i=0;i<2;i++){if(triggers[i]<.03f)input.trigger_ready[i]=true;else if(input.trigger_ready[i]&&triggers[i]>.12f){activity=true;input.trigger_ready[i]=false;}}
 if(activity)switch_to(NI_CONTROLLER);
 if(input.source==NI_CONTROLLER){s.buttons&=~input.blocked;/* Outputs held during a touch takeover need a release/rearm. */
  if(input.blocked&NI_L2)s.lt=0;if(input.blocked&NI_R2)s.rt=0;
  if(input.stick_blocked[0])s.lx=s.ly=0;if(input.stick_blocked[1])s.rx=s.ry=0;
  s.rx=finite_clamp(s.rx*input.sensitivity,-1,1);s.ry=finite_clamp(s.ry*input.sensitivity,-1,1);enqueue(s);}
 NISource mode=input.source;unlock();return mode;
}
NISource ni_touch(NIState raw,bool begin){lock();input.metrics.events++;if(input.running&&input.focused){if(begin)switch_to(NI_TOUCH);if(input.source==NI_TOUCH){NIState s=clean(raw);s.rx=finite_clamp(s.rx*input.sensitivity,-1,1);s.ry=finite_clamp(s.ry*input.sensitivity,-1,1);enqueue(s);}}NISource mode=input.source;unlock();return mode;}
static uint8_t axis(float x,bool invert){x=finite_clamp(x,-1,1);if(invert)x=-x;return (uint8_t)lroundf(128+x*(x<0?128:127));}
void ni_consume(uint8_t pad[56]){
 lock();
 /* A leading analog-only snapshot has no button edge if its buttons already
    match the last engine delivery. Fold it into the next queued snapshot;
    never skip a press/release, including neutral during source takeover. */
 while(input.count>1&&input.queue[input.head].state.buttons==input.delivered_buttons){input.head=(input.head+1)%NI_QUEUE;input.count--;input.metrics.analog_skipped++;}
 NIState s=input.output;if(input.count){NIEvent e=input.queue[input.head];input.head=(input.head+1)%NI_QUEUE;input.count--;s=e.state;if(e.at){double ms=(ns()-e.at)/1e6;input.metrics.samples++;input.metrics.mean_ms+=(ms-input.metrics.mean_ms)/(double)input.metrics.samples;if(ms>input.metrics.max_ms)input.metrics.max_ms=ms;}}
 if(!input.running||!input.focused)s=(NIState){0};
 input.delivered_buttons=s.buttons;
 if(!input.pad_saved){input.saved50=pad[50];input.saved55=pad[55];input.pad_saved=true;}
 memcpy(pad+12,&s.buttons,4);pad[20]=axis(s.lx,false);pad[21]=axis(s.ly,true);pad[22]=axis(s.rx,false);pad[23]=axis(s.ry,true);pad[24]=(uint8_t)lroundf(s.lt*255);pad[25]=(uint8_t)lroundf(s.rt*255);
 pad[50]=input.running?1:input.saved50;pad[55]=input.running?1:input.saved55;input.metrics.consumed++;unlock();
}
bool ni_is_control(uint64_t p){return p==24488864||p==24660320||p==24831776;}
void ni_control(uint8_t field[4]){lock();int32_t source=input.running&&input.focused?0:-4;memcpy(field,&source,4);unlock();}
void ni_metrics(NIMetrics *out){lock();*out=input.metrics;out->queued=input.count;out->running=input.running;out->focused=input.focused;out->source=input.source;unlock();}
