#ifndef NATIVE_TOUCH_LAYOUT_H
#define NATIVE_TOUCH_LAYOUT_H
#include "native_input.h"
#include <math.h>
#include <string.h>
typedef struct {float x,y,w,h;} NIRect;
typedef struct {float x,y,r;uint32_t button;const char *label;} NIControl;
/* Pad symbols deliberately retain the game's contextual control mappings. */
static const NIControl ni_controls[]={
 {.16f,.72f,1,0,"MOVE"},{.64f,.72f,1,0,"LOOK"},
 {.88f,.83f,0,NI_CROSS,"×"},{.95f,.65f,0,NI_CIRCLE,"○"},
 {.81f,.65f,0,NI_SQUARE,"□"},{.88f,.47f,0,NI_TRIANGLE,"△"},
 {.10f,.28f,0,NI_L2,"L2"},{.94f,.28f,0,NI_R2,"R2"},
 {.23f,.16f,0,NI_L1,"L1"},{.78f,.16f,0,NI_R1,"R1"},
 {.30f,.48f,0,NI_L3,"L3"},{.71f,.43f,0,NI_R3,"R3"},
 {.40f,.64f,0,NI_UP,"↑"},{.46f,.79f,0,NI_RIGHT,"→"},
 {.40f,.94f,0,NI_DOWN,"↓"},{.34f,.79f,0,NI_LEFT,"←"},
 {.45f,.14f,0,NI_SELECT,"View"},{.55f,.14f,0,NI_START,"Pause"}
};
#define NI_CONTROL_COUNT (sizeof(ni_controls)/sizeof(ni_controls[0]))
#define NI_TOUCHES 10
typedef struct {bool used,inside;unsigned control;float x,y;} NITouchPoint;
typedef struct {NITouchPoint points[NI_TOUCHES];NIRect rect;float scale;} NITouches;
static inline NIRect ni_aspect_fit(float w,float h){float fw=fminf(w,h*16/9),fh=fw*9/16;return (NIRect){(w-fw)/2,(h-fh)/2,fw,fh};}
static inline float ni_radius(NITouches *t,unsigned c){float base=fminf(t->rect.w/11,t->rect.h*.23f);return c<2?base*t->scale:fmaxf(t->rect.w>=600?22:15,base*.40f);}
static inline void ni_center(NITouches *t,unsigned c,float *x,float *y){*x=t->rect.x+ni_controls[c].x*t->rect.w;*y=t->rect.y+ni_controls[c].y*t->rect.h;}
static inline int ni_touch_hit(NITouches *t,float x,float y){if(t->rect.w<=0||t->rect.h<=0)return -1;for(unsigned c=0;c<NI_CONTROL_COUNT;c++){float cx,cy;ni_center(t,c,&cx,&cy);if(hypotf(x-cx,y-cy)<=ni_radius(t,c))return (int)c;}return -1;}
static inline void ni_touch_clear(NITouches *t){memset(t->points,0,sizeof(t->points));}
static inline bool ni_touch_begin(NITouches *t,unsigned slot,float x,float y){if(slot>=NI_TOUCHES)return false;int c=ni_touch_hit(t,x,y);if(c<0)return false;/* One finger per stick; buttons allow independent owners. */if(c<2)for(unsigned i=0;i<NI_TOUCHES;i++)if(t->points[i].used&&t->points[i].control==(unsigned)c)return false;t->points[slot]=(NITouchPoint){true,true,(unsigned)c,x,y};return true;}
static inline void ni_touch_move(NITouches *t,unsigned slot,float x,float y){if(slot>=NI_TOUCHES||!t->points[slot].used)return;NITouchPoint *p=&t->points[slot];p->x=x;p->y=y;float cx,cy;ni_center(t,p->control,&cx,&cy);p->inside=p->control<2||hypotf(x-cx,y-cy)<=ni_radius(t,p->control)*1.25f;}
static inline void ni_touch_end(NITouches *t,unsigned slot){if(slot<NI_TOUCHES)t->points[slot]=(NITouchPoint){0};}
static inline NIState ni_touch_state(NITouches *t){NIState s={0};for(unsigned i=0;i<NI_TOUCHES;i++){NITouchPoint *p=&t->points[i];if(!p->used||!p->inside)continue;unsigned c=p->control;if(c<2){float x,y;ni_center(t,c,&x,&y);x=(p->x-x)/ni_radius(t,c);y=(y-p->y)/ni_radius(t,c);float m=hypotf(x,y);if(m>1){x/=m;y/=m;}if(c==0){s.lx=x;s.ly=y;}else{s.rx=x;s.ry=y;}}else{s.buttons|=ni_controls[c].button;}}s.lt=(s.buttons&NI_L2)?1:0;s.rt=(s.buttons&NI_R2)?1:0;return s;}
#endif
