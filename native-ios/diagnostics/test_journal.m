#import "native_journal.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
static void *producer(void *arg){@autoreleasepool{unsigned id=(unsigned)(uintptr_t)arg;char line[100];for(unsigned i=0;i<20000;i++){int n=snprintf(line,sizeof(line),"worker%u-record%u-abcdefghijklmnopqrstuvwxyz",id,i);native_journal_capture("engine",line,(size_t)n);}}return NULL;}
static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
int main(void){@autoreleasepool{
 NSURL *directory=[NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString] isDirectory:YES];NSError *error=nil;assert([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
 NSURL *URL=[directory URLByAppendingPathComponent:@"session.txt"];NativeSessionLog *log=[[NativeSessionLog alloc]initWithURL:URL error:&error];assert(log&&!error);NativeJournalActivate(log);
 const char *unicode="full UTF-8: \xe2\x98\x83 \xf0\x9f\x8e\xae";native_journal_capture("header",unicode,strlen(unicode));pthread_t threads[4];uint64_t start=ns();for(unsigned i=0;i<4;i++)assert(!pthread_create(threads+i,NULL,producer,(void*)(uintptr_t)i));NSURL *live=[directory URLByAppendingPathComponent:@"live.txt"];assert([log snapshotToURL:live error:&error]);NSData *liveData=[NSData dataWithContentsOfURL:live];assert([[NSString alloc]initWithData:liveData encoding:NSUTF8StringEncoding]);for(unsigned i=0;i<4;i++)assert(!pthread_join(threads[i],NULL));double ms=(ns()-start)/1e6;
 assert([log flush:&error]&&!error);NSURL *snapshot=[directory URLByAppendingPathComponent:@"snapshot.txt"];assert([log snapshotToURL:snapshot error:&error]);
 NSData *data=[NSData dataWithContentsOfURL:snapshot];assert(data.length>3*1024*1024);NSString *text=[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding];assert(text&&[text containsString:@"full UTF-8:"]&&[text containsString:@"Snapshot prefix"]);unsigned next[4]={0};unsigned seen=0;for(NSString *line in [text componentsSeparatedByString:@"\n"]){if(![line hasPrefix:@"[engine] worker"])continue;unsigned id=99,index=0;assert(sscanf(line.UTF8String,"[engine] worker%u-record%u-",&id,&index)==2);assert(id<4&&index==next[id]++);seen++;}assert(seen==80000);for(unsigned id=0;id<4;id++)assert(next[id]==20000);
 native_journal_capture("tail","after snapshot",14);assert([log flush:&error]);NSString *before=[NSString stringWithContentsOfURL:snapshot encoding:NSUTF8StringEncoding error:&error];assert(![before containsString:@"after snapshot"]);NativeJournalActivate(nil);[log finish];[log finish];assert([log snapshotToURL:[directory URLByAppendingPathComponent:@"closed.txt"] error:&error]);
 NativeSessionLog *duplicate=[[NativeSessionLog alloc]initWithURL:URL error:&error];assert(!duplicate&&error);error=nil;
 NativeSessionLog *broken=[[NativeSessionLog alloc]initWithURL:[directory URLByAppendingPathComponent:@"disk-full.txt"] error:&error];assert(broken);[broken testFailAfter:100];broken.testWriteLimit=7;char huge[4096];memset(huge,'x',sizeof(huge));[broken appendBytes:huge length:sizeof(huge) category:NULL];assert(![broken flush:&error]&&error.code==ENOSPC);[broken finish];error=nil;NSURL *incomplete=[directory URLByAppendingPathComponent:@"incomplete.txt"];assert([broken snapshotToURL:incomplete error:&error]);NSString *failure=[NSString stringWithContentsOfURL:incomplete encoding:NSUTF8StringEncoding error:&error];assert([failure containsString:@"INCOMPLETE"]&&[failure containsString:@"I/O ERROR"]);
 start=ns();for(unsigned i=0;i<80000;i++)native_journal_capture("inactive","no allocation",13);double disabled=(ns()-start)/1e6;
 printf("PASS: file journal:80000 concurrent records, UTF-8, >3MiB full export, live concurrent snapshot, stable prefix, post-close export, repeated stop, duplicate-path protection, seven-byte partial writes and injected ENOSPC with explicit incomplete status. Fixed2MiB queue; no file-size quota.\n");
 printf("Journal host throughput:80000 enabled records %.3fms;80000 inactive calls %.3fms. Includes storage/sanitizer/scheduling, not iPhone FPS or latency.\n%s\n",ms,disabled,[log summary].UTF8String);
 assert([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
 }return 0;}
