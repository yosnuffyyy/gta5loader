#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "native_renderer.h"
#include <stdio.h>
int main(void){@autoreleasepool{
 if(!MTLCreateSystemDefaultDevice()){puts("SKIP: this Mac runner exposes no Metal device; iPhone renderer validation is required.");return 0;}
 char report[4096];int code=native_renderer_probe(report,sizeof(report),NULL,0);puts(report);return code;
}}
