#include "shadow_failure_log.h"
#include <assert.h>
#include <stdio.h>
static NSString *boundedReport(NSString *previous,NSString *next){
 NSString *joined=[previous stringByAppendingFormat:@"\n%@",next];
 return joined.length>32768?[@"Earlier startup messages omitted to bound log memory.\n" stringByAppendingString:[joined substringFromIndex:joined.length-24576]]:joined;
}
int main(void){@autoreleasepool{
 NSMutableString *fatal=[NSMutableString string];assert(!NativeShadowFailureReport(fatal).length);
 NativeShadowFailureAppend(fatal,@"Normal progress");assert(!fatal.length);
 for(unsigned i=0;i<400;i++)NativeShadowFailureAppend(fatal,[NSString stringWithFormat:@"Shadow transition %u: old bounded history data",i]);
 assert(fatal.length<=NATIVE_SHADOW_FAILURE_LIMIT);
 for(unsigned slot=3;slot<=6;slot+=3)for(unsigned row=0;row<(slot==3?236:124);row+=8)NativeShadowFailureAppend(fatal,[NSString stringWithFormat:@"Shadow fatal CB%u words %u: 00000001 00000002 00000003 00000004 00000005 00000006 00000007 40000000",slot,row]);
 NativeShadowFailureAppend(fatal,@"Active shadow sampler raw[902,3,128,3,3,3,0,0,4]");
 NSString *saved=[fatal copy];for(unsigned i=0;i<2000;i++)NativeShadowFailureAppend(fatal,@"Periodic metrics and cleanup logs");assert([saved isEqualToString:fatal]);
 NSString *report=boundedReport([@"Old startup\n" stringByPaddingToLength:32768 withString:@"x" startingAtIndex:0],NativeShadowFailureReport(fatal));report=boundedReport(report,[@"Native startup stopped: SRV mismatch\n" stringByPaddingToLength:4096 withString:@"y" startingAtIndex:0]);
 assert(report.length<=32768&&[report containsString:@"Shadow fatal CB3 words 0:"]&&[report containsString:@"Shadow fatal CB3 words 232:"]&&[report containsString:@"Shadow fatal CB6 words 120:"]&&[report containsString:@"Active shadow sampler raw["]);
 [fatal setString:@""];assert(!NativeShadowFailureReport(fatal).length);
 puts("PASS: bounded fatal shadow report survives progress/cleanup rollover; both constant blocks and sampler retained, 16Ki-character cap, reset without stale evidence.");return 0;
}}
