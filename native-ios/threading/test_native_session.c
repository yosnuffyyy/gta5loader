#include "native_session.h"
#include <assert.h>
static unsigned cases;
#define NS(n) ((uint64_t)(n)*SESSION_SECOND)
#define EXPECT(x) do{assert(x);cases++;}while(0)
static void live(NativeSession *s){session_begin(s,NS(100));session_hook(s,NS(101));session_log(s,"Currently running script thread \"Prologue1\"");session_log(s,"Animated camera intial frame: Position=(5313,-5211,83)");session_hook(s,NS(102));EXPECT(session_check(s,NS(102),100,1,30)==SESSION_OK);EXPECT(s->phase==SESSION_LIVE);}
int main(void){NativeSession s;
 session_begin(&s,NS(100));EXPECT(!session_check(&s,NS(279),999999,9999,100));EXPECT(session_check(&s,NS(280),999999,9999,100)==SESSION_STARTUP_TIMEOUT);EXPECT(s.phase==SESSION_STARTUP);
 session_begin(&s,NS(100));session_log(&s,"Animated camera intial frame");EXPECT(!session_check(&s,NS(102),1,1,0));EXPECT(s.phase==SESSION_STARTUP);session_hook(&s,NS(103));EXPECT(!session_check(&s,NS(103),1,1,0));EXPECT(s.phase==SESSION_LIVE);
 live(&s);for(unsigned i=103;i<=1403;i++){session_hook(&s,NS(i));EXPECT(!session_check(&s,NS(i),i*100,i,30));}EXPECT(s.phase==SESSION_LIVE); /* >21 minutes */
 live(&s);EXPECT(session_check(&s,NS(162),999,100,30)==SESSION_ENGINE_STALL);EXPECT(session_check(&s,NS(163),1000,101,31)==SESSION_ENGINE_STALL); /* first fatal retained */
 live(&s);session_hook(&s,NS(162));EXPECT(session_check(&s,NS(162),999,1,30)==SESSION_FRAME_STALL); /* spinning engine is not frame progress */
 live(&s);for(unsigned i=103;i<600;i+=20)EXPECT(!session_check(&s,NS(i),i,1,i));session_hook(&s,NS(601));EXPECT(!session_check(&s,NS(601),1000,2,600)); /* long loading with completed I/O */
 live(&s);session_suspend(&s,NS(110),true);EXPECT(!session_check(&s,NS(5000),100,1,30));session_suspend(&s,NS(5000),false);session_hook(&s,NS(5001));EXPECT(!session_check(&s,NS(5001),200,2,30));EXPECT(session_time(&s,NS(5001))==NS(11));
 session_begin(&s,NS(100));session_suspend(&s,NS(200),true);EXPECT(!session_check(&s,NS(10000),1,1,0));session_suspend(&s,NS(10000),false);EXPECT(!session_check(&s,NS(10079),1,1,0));EXPECT(session_check(&s,NS(10080),1,1,0)==SESSION_STARTUP_TIMEOUT);
 live(&s);session_stop(&s);EXPECT(!session_check(&s,NS(999999),0,0,0));session_begin(&s,NS(1000000));EXPECT(s.phase==SESSION_STARTUP&&!s.scene_marker&&!s.hooks&&!s.fault);
 live(&s);session_hook(&s,NS(200));EXPECT(!session_check(&s,NS(200),300,2,30)); /* long cutscene / engine pause still pumps */
 EXPECT(session_delta(1,2)==0);char text[512];session_summary(&s,NS(200),text,sizeof(text));EXPECT(strstr(text,"Prologue1")&&strstr(text,"not visual"));
 printf("PASS: native session watchdog %u assertions; controlled clock, >21min live engine, startup/stalls, loading, cutscene/pause, focus suspension, cancellation, restart and first-fatal retention. No physical gameplay claim.\n",cases);return 0;
}
