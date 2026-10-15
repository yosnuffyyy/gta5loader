#ifndef NATIVE_SHADOW_FAILURE_LOG_H
#define NATIVE_SHADOW_FAILURE_LOG_H
#import <Foundation/Foundation.h>
enum { NATIVE_SHADOW_FAILURE_LIMIT=16384 };
static inline BOOL NativeShadowFailureLine(NSString *text){
 return [text hasPrefix:@"Shadow transition "]||[text hasPrefix:@"Shadow fatal CB"]||[text hasPrefix:@"Active shadow "]||[text hasPrefix:@"SRV source lifetime:"];
}
/* Caller serializes access. Keep fatal evidence separate from periodic
   timing/cleanup output, which can roll the ordinary progress buffer. */
static inline void NativeShadowFailureAppend(NSMutableString *buffer,NSString *text){
 if(!NativeShadowFailureLine(text))return;
 [buffer appendFormat:@"\n%@",text];
 if(buffer.length>NATIVE_SHADOW_FAILURE_LIMIT)[buffer deleteCharactersInRange:NSMakeRange(0,buffer.length-NATIVE_SHADOW_FAILURE_LIMIT)];
}
static inline NSString *NativeShadowFailureReport(NSString *buffer){
 return buffer.length?[@"Retained shadow failure context (bounded):" stringByAppendingString:buffer]:@"";
}
#endif
