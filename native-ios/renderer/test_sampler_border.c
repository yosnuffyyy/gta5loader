#include "sampler_border.h"
#include <assert.h>
#include <stdio.h>
int main(void){uint32_t bits[4]={0};assert(sampler_border_color(bits)==1);float x[]={0,0,0,1};memcpy(bits,x,sizeof(bits));assert(sampler_border_color(bits)==2);for(unsigned i=0;i<4;i++)x[i]=1;memcpy(bits,x,sizeof(bits));assert(sampler_border_color(bits)==3);x[0]=.5f;memcpy(bits,x,sizeof(bits));assert(sampler_border_color(bits)==0);x[0]=NAN;memcpy(bits,x,sizeof(bits));assert(sampler_border_color(bits)==0);x[0]=INFINITY;memcpy(bits,x,sizeof(bits));assert(sampler_border_color(bits)==0);x[0]=x[1]=x[2]=-0.f;x[3]=0;memcpy(bits,x,sizeof(bits));assert(sampler_border_color(bits)==1);puts("PASS: exact native sampler border classification; unsupported custom/non-finite colors rejected.");return 0;}
