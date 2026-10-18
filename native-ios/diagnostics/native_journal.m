#import "native_journal.h"
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdatomic.h>
#include "native_recovery.h"

/* File size has no application quota. Fixed memory, one writer, no per-line
   NSData/block allocation on the engine path. Backpressure is explicit. */
_Static_assert(sizeof(off_t)>=8,"Session logs require 64-bit file offsets");
enum { JOURNAL_CAPACITY=2*1024*1024,JOURNAL_CHUNK=64*1024 };
static uint64_t journal_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
static void jc(int e){if(e)abort();}
static BOOL journal_error(NSError **out,int code,NSString *operation){if(out)*out=[NSError errorWithDomain:NSPOSIXErrorDomain code:code userInfo:@{NSLocalizedDescriptionKey:[NSString stringWithFormat:@"%@: %s. The original log is retained.",operation,strerror(code)]}];return NO;}
@interface NativeSessionLog () {
 pthread_mutex_t _producer,_mutex;pthread_cond_t _changed;pthread_t _thread;
 NativeRecovery _recovery;char *_recoveryPath;uint64_t _recoveryAt;int _recoveryError;
 unsigned char *_ring;size_t _head,_count;int _fd,_error;
 BOOL _started,_stopping,_joined;uint64_t _accepted,_written,_lost,_waits,_wait_ns,_flush_mark;
#if defined(NATIVE_JOURNAL_TEST)
 uint64_t _failAfter;
#endif
}
- (void)writer;
- (void)checkpoint;
@end
static void *journal_writer(void *context){@autoreleasepool{NativeSessionLog *log=(__bridge_transfer NativeSessionLog *)context;[log writer];}return NULL;}
@implementation NativeSessionLog
- (instancetype)initWithURL:(NSURL *)URL error:(NSError **)error {
 if(!(self=[super init]))return nil;_fd=-1;_URL=[URL copy];_recoveryPath=strdup([[URL.path stringByAppendingString:@".recovery.txt"] fileSystemRepresentation]);
 jc(pthread_mutex_init(&_producer,NULL));jc(pthread_mutex_init(&_mutex,NULL));jc(pthread_cond_init(&_changed,NULL));
 _ring=malloc(JOURNAL_CAPACITY);if(!_ring){journal_error(error,ENOMEM,@"Log buffer allocation");return nil;}
 _fd=open(URL.fileSystemRepresentation,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(_fd<0){journal_error(error,errno,@"Create session log");return nil;}
 void *retained=(__bridge_retained void *)self;int rc=pthread_create(&_thread,NULL,journal_writer,retained);
 if(rc){id released=CFBridgingRelease(retained);(void)released;close(_fd);_fd=-1;journal_error(error,rc,@"Start log writer");return nil;}_started=YES;return self;
}
- (void)dealloc {if(_started&&!_joined)abort();if(_fd>=0)close(_fd);free(_ring);free(_recoveryPath);jc(pthread_cond_destroy(&_changed));jc(pthread_mutex_destroy(&_mutex));jc(pthread_mutex_destroy(&_producer));}
- (void)checkpoint {
 NativeRecovery copy;jc(pthread_mutex_lock(&_mutex));copy=_recovery;uint64_t written=_written;jc(pthread_mutex_unlock(&_mutex));int error=_recoveryPath?recovery_save(&copy,_recoveryPath,written,journal_ns()):ENOMEM;jc(pthread_mutex_lock(&_mutex));_recoveryError=error;jc(pthread_mutex_unlock(&_mutex));
}
- (void)writer {
 unsigned char block[JOURNAL_CHUNK];
 for(;;){jc(pthread_mutex_lock(&_mutex));while(!_count&&!_stopping&&!_error)jc(pthread_cond_wait(&_changed,&_mutex));if(_error||(!_count&&_stopping)){jc(pthread_mutex_unlock(&_mutex));break;}
  /* Coalesce small records for at most 20 ms; flush/snapshot/finish wake early.
     Repeated producer signals never extend this fixed deadline. */
  struct timespec until;clock_gettime(CLOCK_REALTIME,&until);until.tv_nsec+=20000000;if(until.tv_nsec>=1000000000){until.tv_sec++;until.tv_nsec-=1000000000;}
  while(_count<JOURNAL_CHUNK&&!_stopping&&!_error&&_written>=_flush_mark){int rc=pthread_cond_timedwait(&_changed,&_mutex,&until);if(rc==ETIMEDOUT)break;jc(rc);}
  size_t n=_count<JOURNAL_CHUNK?_count:JOURNAL_CHUNK,first=JOURNAL_CAPACITY-_head;if(first>n)first=n;memcpy(block,_ring+_head,first);memcpy(block+first,_ring,n-first);_head=(_head+n)%JOURNAL_CAPACITY;_count-=n;jc(pthread_cond_broadcast(&_changed));jc(pthread_mutex_unlock(&_mutex));
  size_t done=0;int failure=0;while(done<n){size_t take=n-done;
#if defined(NATIVE_JOURNAL_TEST)
   jc(pthread_mutex_lock(&_mutex));uint64_t limit=_failAfter,written=_written;jc(pthread_mutex_unlock(&_mutex));if(limit&&written+done>=limit){failure=ENOSPC;break;}if(limit&&take>limit-written-done)take=(size_t)(limit-written-done);if(self.testWriteLimit&&take>self.testWriteLimit)take=self.testWriteLimit;
#endif
   ssize_t got=write(_fd,block+done,take);if(got<0&&errno==EINTR)continue;if(got<=0){failure=got<0?errno:EIO;break;}done+=(size_t)got;
  }
  jc(pthread_mutex_lock(&_mutex));_written+=done;if(failure){_error=failure;_lost+=_count+n-done;_count=0;}jc(pthread_cond_broadcast(&_changed));jc(pthread_mutex_unlock(&_mutex));if(failure)break;uint64_t now=journal_ns();if(now-_recoveryAt>=5000000000ull){_recoveryAt=now;[self checkpoint];}
 }
 [self checkpoint];
}
- (void)appendBytes:(const void *)bytes length:(size_t)length category:(const char *)category {
 if(!bytes&&length)return;const char *prefix=category?:"";size_t plen=strlen(prefix);if(plen>32)plen=32;
 const void *parts[]={"[",prefix,"] ",bytes,"\n"};size_t sizes[]={category?1:0,plen,category?2:0,length,category?1:0};
 jc(pthread_mutex_lock(&_producer));jc(pthread_mutex_lock(&_mutex));
 if(bytes&&length)recovery_add(&_recovery,bytes,length);
 for(unsigned p=0;p<5;p++){const unsigned char *from=parts[p];size_t left=sizes[p];while(left){if(_error||_stopping){_lost+=left;for(unsigned q=p+1;q<5;q++)_lost+=sizes[q];goto done;}
   if(_count==JOURNAL_CAPACITY){uint64_t at=journal_ns();_waits++;jc(pthread_cond_wait(&_changed,&_mutex));_wait_ns+=journal_ns()-at;continue;}
   size_t at=(_head+_count)%JOURNAL_CAPACITY,n=JOURNAL_CAPACITY-at;if(n>JOURNAL_CAPACITY-_count)n=JOURNAL_CAPACITY-_count;if(n>left)n=left;memcpy(_ring+at,from,n);_count+=n;_accepted+=n;from+=n;left-=n;jc(pthread_cond_signal(&_changed));
  }}
 done:jc(pthread_mutex_unlock(&_mutex));jc(pthread_mutex_unlock(&_producer));
}
- (void)appendText:(NSString *)text category:(const char *)category {const char *bytes=(text?:@"").UTF8String;if(bytes)[self appendBytes:bytes length:strlen(bytes) category:category];}
- (BOOL)flush:(NSError **)error {
 jc(pthread_mutex_lock(&_producer));jc(pthread_mutex_lock(&_mutex));uint64_t mark=_accepted;_flush_mark=mark;jc(pthread_cond_broadcast(&_changed));jc(pthread_mutex_unlock(&_producer));
 while(_written<mark&&!_error)jc(pthread_cond_wait(&_changed,&_mutex));int failure=_error;if(!failure&&_fd>=0&&fsync(_fd))failure=errno;jc(pthread_mutex_unlock(&_mutex));if(failure)return journal_error(error,failure,@"Flush complete session log");return YES;
}
- (void)finish {
 /* One owner closes the writer after native producers join; snapshots retain
    self and use an independent read descriptor. No file is deleted here. */
 jc(pthread_mutex_lock(&_producer));jc(pthread_mutex_lock(&_mutex));if(_stopping){jc(pthread_mutex_unlock(&_mutex));jc(pthread_mutex_unlock(&_producer));return;}_stopping=YES;jc(pthread_cond_broadcast(&_changed));jc(pthread_mutex_unlock(&_mutex));jc(pthread_mutex_unlock(&_producer));
 if(_started)jc(pthread_join(_thread,NULL));jc(pthread_mutex_lock(&_mutex));if(_fd>=0){if(fsync(_fd)&&!_error)_error=errno;close(_fd);_fd=-1;}_joined=YES;jc(pthread_cond_broadcast(&_changed));jc(pthread_mutex_unlock(&_mutex));
}
- (BOOL)snapshotToURL:(NSURL *)URL error:(NSError **)error {
 /* Wait only for the prefix accepted at entry; new appends may continue.
    The copy uses a fixed 64 KiB block, never a whole-file NSData/NSString. */
 jc(pthread_mutex_lock(&_producer));jc(pthread_mutex_lock(&_mutex));uint64_t mark=_accepted;_flush_mark=mark;jc(pthread_cond_broadcast(&_changed));jc(pthread_mutex_unlock(&_producer));while(_written<mark&&!_error)jc(pthread_cond_wait(&_changed,&_mutex));uint64_t prefix=_written<mark?_written:mark;int failure=_error;jc(pthread_mutex_unlock(&_mutex));
 if(!NativeLogCopyPrefix(self.URL,URL,prefix,error))return NO;
 NSString *footer=[NSString stringWithFormat:@"\n[export] Snapshot prefix %llu bytes. %@\n%@\n",(unsigned long long)prefix,failure?@"INCOMPLETE: logging encountered an I/O failure.":@"Later session messages are outside this snapshot.",[self summary]];
 int fd=open(URL.fileSystemRepresentation,O_WRONLY|O_APPEND|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return journal_error(error,errno,@"Open log export footer");const char *b=footer.UTF8String;size_t left=strlen(b);while(left){ssize_t n=write(fd,b,left);if(n<0&&errno==EINTR)continue;if(n<=0){int e=n<0?errno:EIO;close(fd);return journal_error(error,e,@"Write export footer");}b+=n;left-=n;}if(fsync(fd)){int e=errno;close(fd);return journal_error(error,e,@"Flush log export");}close(fd);return YES;
}
- (NSString *)summary {jc(pthread_mutex_lock(&_mutex));uint64_t a=_accepted,w=_written,l=_lost,waits=_waits,ns=_wait_ns;int e=_error,recoveryError=_recoveryError;size_t queued=_count;jc(pthread_mutex_unlock(&_mutex));return [NSString stringWithFormat:@"Session log: accepted/written %llu/%llu bytes; pending %zu; producer waits %llu (%.3f ms); unrecorded %llu bytes; %@. Fixed 2 MiB queue, no file-size quota; storage availability applies. Recovery snapshot I/O errno %d (0 OK).",(unsigned long long)a,(unsigned long long)w,queued,(unsigned long long)waits,ns/1e6,(unsigned long long)l,e?[NSString stringWithFormat:@"I/O ERROR %d (%s)",e,strerror(e)]:@"I/O OK",recoveryError];}
#if defined(NATIVE_JOURNAL_TEST)
- (void)testFailAfter:(uint64_t)bytes {jc(pthread_mutex_lock(&_mutex));_failAfter=bytes;jc(pthread_mutex_unlock(&_mutex));}
#endif
@end

static pthread_mutex_t active_mutex=PTHREAD_MUTEX_INITIALIZER;
static NativeSessionLog *active_log;
static _Atomic bool capture_active;
void NativeJournalActivate(NativeSessionLog *log){jc(pthread_mutex_lock(&active_mutex));active_log=log;atomic_store_explicit(&capture_active,log!=nil,memory_order_release);jc(pthread_mutex_unlock(&active_mutex));}
void native_journal_capture(const char *category,const void *bytes,size_t length){if(!atomic_load_explicit(&capture_active,memory_order_acquire))return;jc(pthread_mutex_lock(&active_mutex));NativeSessionLog *log=active_log;jc(pthread_mutex_unlock(&active_mutex));if(log)[log appendBytes:bytes length:length category:category];}
BOOL NativeLogCopyPrefix(NSURL *source,NSURL *destination,uint64_t bytes,NSError **error){
 int in=open(source.fileSystemRepresentation,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(in<0)return journal_error(error,errno,@"Open saved log");struct stat st;if(fstat(in,&st)){int e=errno;close(in);return journal_error(error,e,@"Read log size");}if(!S_ISREG(st.st_mode)||st.st_size<0){close(in);return journal_error(error,EINVAL,@"Read regular log file");}if(bytes==UINT64_MAX)bytes=(uint64_t)st.st_size;if(bytes>(uint64_t)st.st_size){close(in);return journal_error(error,EIO,@"Log prefix unavailable");}
 int out=open(destination.fileSystemRepresentation,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(out<0){int e=errno;close(in);return journal_error(error,e,@"Create text export");}
 unsigned char block[JOURNAL_CHUNK];uint64_t done=0;int failure=0;while(done<bytes){size_t take=bytes-done<JOURNAL_CHUNK?(size_t)(bytes-done):JOURNAL_CHUNK;ssize_t n=pread(in,block,take,(off_t)done);if(n<0&&errno==EINTR)continue;if(n<=0){failure=n<0?errno:EIO;break;}size_t written=0;while(written<(size_t)n){ssize_t k=write(out,block+written,(size_t)n-written);if(k<0&&errno==EINTR)continue;if(k<=0){failure=k<0?errno:EIO;break;}written+=(size_t)k;}if(failure)break;done+=(size_t)n;}
 if(!failure&&fsync(out))failure=errno;close(out);close(in);if(failure){unlink(destination.fileSystemRepresentation);return journal_error(error,failure,@"Export log prefix");}return YES;
}
