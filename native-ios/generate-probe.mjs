import fs from 'node:fs';
import assert from 'node:assert/strict';
const root=process.argv[2]||'native-build';
const header=fs.readFileSync(root+'/generated/gta.h','utf8');
const internal=fs.readFileSync(root+'/generated/gta-impl.h','utf8');
for(const declaration of ['u32 w2c_gta_f90879(w2c_gta*, u64, u64, u64, u64);','u32 w2c_gta_f90884(w2c_gta*, u64);','u64 w2c_gta_f90695(w2c_gta*);'])assert(internal.includes(declaration),'Thread binding ABI differs: '+declaration);
const declarations=[...header.matchAll(/\/\* import: '([^']+)' '([^']+)' \*\/\s+(void|u32|u64|f32|f64) (w2c_\w+)\(([^;]+)\);/g)];
assert.equal(declarations.length,85,'Unexpected engine host interface');
const implemented=JSON.parse(fs.readFileSync('native-ios/host-imports.json','utf8'));
for(const key of Object.keys(implemented))assert(declarations.some(d=>d[2]===key),'Unknown host import '+key);
let source=`#include "native_host.h"\n#include "threading/native_threads.h"\n#include "wasm-rt-impl.h"\n#include "wasm-rt-exceptions.h"\n#include "probe_reference.h"\n#include <stdio.h>\n#include <string.h>\n#include <math.h>\n#include <stdlib.h>\n#include <unistd.h>\nwasm_rt_shared_memory_t* w2c_env_memory(struct w2c_env *env){return &env->memory;}\n`;
for(const d of declarations){
 const args=d[5].split(',').map((s,i)=>s.trim()+' arg'+i).join(', ');
 const env=d[1]==='env'?'arg0':'arg0->env';
 const body=implemented[d[2]]||`host_unavailable(${env},"${d[1]}.${d[2]}");`;
 source+=`${d[3]} ${d[4]}(${args}){${body}}\n`;
}
// wasm2c already generates ABI-correct weak tail-call forwarding adapters.
// Do not replace them with trap-only adapters: libc can tail-call imports.
fs.writeFileSync(root+'/evidence/host-coverage.json',JSON.stringify(declarations.map(d=>({module:d[1],name:d[2],status:implemented[d[2]]?'checkpoint implementation':'explicit unsupported trap'})),null,2));
source+=`
int native_engine_probe(char *report,size_t capacity){

 wasm_rt_init();
 struct w2c_env *env=calloc(1,sizeof(*env));
 struct w2c_wasi__snapshot__preview1 wasi={.env=env};
 w2c_gta *engine=calloc(1,sizeof(*engine));
 if(!env||!engine){free(env);free(engine);snprintf(report,capacity,"FAIL: host allocation");return 1;}
 host_init(env);env->engine=engine;
 // Bounds checks and a stable mmap base; this does not request executable RAM.
 wasm_rt_allocate_memory_shared(&env->memory,8192,65536,false,65536);
 volatile int instantiated=0;
 wasm_rt_trap_t trap=wasm_rt_impl_try();
 if(trap){
  snprintf(report,capacity,"FAIL: trap %d%s%s",trap,env->failure?" in unsupported host call ":"",env->failure?env->failure:"");
  if(instantiated)wasm2c_gta_free(engine);
  wasm_rt_free_memory_shared(&env->memory);pthread_mutex_destroy(&env->memory.mem_lock);free(engine);free(env);wasm_rt_free_thread();return 1;
 }
 wasm2c_gta_instantiate(engine,env,&wasi);instantiated=1;
 uint64_t a=512ull*1048576-4096,b=a+1024;
 unsigned char *mem=(unsigned char*)env->memory.data;
 memcpy(mem+a,"Alpha",6);memcpy(mem+b,"Alpha",6);
 int failed=0;
 failed|=w2c_gta_native_probe_strlen(engine,a)!=5;
 failed|=w2c_gta_native_probe_strcmp(engine,a,b)!=0;
 failed|=w2c_gta_native_probe_memcmp(engine,a,b,6)!=0;
 mem[b+4]='b';
 failed|=(int32_t)w2c_gta_native_probe_strcmp(engine,a,b)>=0;
 failed|=(int32_t)w2c_gta_native_probe_memcmp(engine,a,b,6)>=0;
 for(unsigned i=0;i<sizeof(probe_trig)/sizeof(probe_trig[0]);i++){
  float s=w2c_gta_native_probe_sinf(engine,probe_trig[i].x),c=w2c_gta_native_probe_cosf(engine,probe_trig[i].x);
  failed|=!isfinite(s)||!isfinite(c)||fabsf(s-probe_trig[i].s)>0.000001f||fabsf(c-probe_trig[i].c)>0.000001f;
 }
 failed|=w2c_gta_htons(engine,0x1234)!=0x3412;
 failed|=wasm_rt_grow_memory_shared(&env->memory,1)!=8192;
 failed|=w2c_gta_native_probe_strlen(engine,a)!=5;
 snprintf(report,capacity,"%s: native engine instantiation, strings, memory comparison, math, byte order and shared-memory growth.\\n91,026 engine functions compiled.\\nThis is a diagnostic prototype; Use the runtime test for constructor and file-host validation. Gameplay remains pending.",failed?"FAIL":"PASS");
 wasm2c_gta_free(engine);wasm_rt_free_memory_shared(&env->memory);pthread_mutex_destroy(&env->memory.mem_lock);free(engine);free(env);wasm_rt_free_thread();
 return failed?1:0;
}
#ifdef NATIVE_PROBE_CLI
typedef struct {char **argv;int result;} CliTask;
static void cli_progress(void *unused,const char *line){(void)unused;puts(line);fflush(stdout);}
static void *cli_run(void *arg){CliTask *t=arg;char report[4096];t->result=native_engine_probe(report,sizeof(report));puts(report);if(!t->result){t->result=native_runtime_probe(t->argv[1],t->argv[2],cli_progress,NULL,report,sizeof(report));puts(report);}return NULL;}
int main(int argc,char **argv){if(argc!=3)return 2;CliTask task={.argv=argv};pthread_t thread;pthread_attr_t attr;if(pthread_attr_init(&attr))return 2;if(pthread_attr_setstacksize(&attr,4*1024*1024))return 2;int error=pthread_create(&thread,&attr,cli_run,&task);pthread_attr_destroy(&attr);if(error||pthread_join(thread,NULL))return 2;return task.result;}
#endif
`;
fs.writeFileSync(root+'/generated/probe.c',source);
console.log('Generated native host bridge with explicit unsupported imports and real tail-call forwarding.');
