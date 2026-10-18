#include "options.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){NativeLaunchOptions o;unsigned checks=0;for(unsigned m=1;m<=2;m++)for(unsigned q=0;q<3;q++){assert(native_launch_options(m,q,&o));assert(o.mode==m&&o.quality==q&&o.width*9==o.height*16);assert((q==0&&o.width==640&&o.height==360)||(q==1&&o.width==480&&o.height==270)||(q==2&&o.width==320&&o.height==180));assert(strcmp(native_launch_saves(1),native_launch_saves(2)));checks+=4;}assert(!native_launch_options(0,0,&o));assert(!native_launch_options(3,0,&o));assert(!native_launch_options(1,3,&o));assert(!native_launch_options(1,0,NULL));printf("PASS: launch options: %u mode/resolution/save-isolation checks, four invalid inputs rejected; 640x360 / 480x270 / 320x180, no mutable runtime option.\n",checks);}
