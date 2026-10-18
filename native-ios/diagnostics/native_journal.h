#ifndef NATIVE_JOURNAL_H
#define NATIVE_JOURNAL_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
/* Inactive is a no-op. The active object is retained across every call. */
void native_journal_capture(const char *category,const void *bytes,size_t length);
#ifdef __OBJC__
#import <Foundation/Foundation.h>
@interface NativeSessionLog : NSObject
@property(nonatomic,strong,readonly) NSURL *URL;
- (instancetype)initWithURL:(NSURL *)URL error:(NSError **)error;
- (void)appendBytes:(const void *)bytes length:(size_t)length category:(const char *)category;
- (void)appendText:(NSString *)text category:(const char *)category;
- (BOOL)flush:(NSError **)error;
- (void)finish;
- (BOOL)snapshotToURL:(NSURL *)URL error:(NSError **)error;
- (NSString *)summary;
#if defined(NATIVE_JOURNAL_TEST)
- (void)testFailAfter:(uint64_t)bytes;
@property(nonatomic,assign) size_t testWriteLimit;
#endif
@end
void NativeJournalActivate(NativeSessionLog *log);
BOOL NativeLogCopyPrefix(NSURL *source,NSURL *destination,uint64_t bytes,NSError **error);
#endif
#endif
