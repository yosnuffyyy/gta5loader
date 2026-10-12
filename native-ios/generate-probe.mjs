import fs from 'node:fs';
import assert from 'node:assert/strict';
const root=process.argv[2]||'native-build';
const header=fs.readFileSync(root+'/generated/gta.h','utf8');
const declarations=[...header.matchAll(/\/\* import: '([^']+)' '([^']+)' \*\/\s+(void|u32|u64|f32|f64) (w2c_\w+)\(([^;]+)\);/g)];
assert.equal(declarations.length,85,'Unexpected engine host interface');
let source=`#include "gta.h"\n#include "wasm-rt-impl.h"\n#include "probe.h"\n#include "probe_reference.h"\n#include <stdio.h>\n#include <string.h>\n#include <math.h>\n#include <stdlib.h>\n#include <stdatomic.h>\nstruct w2c_env {wasm_rt_shared_memory_t memory;};\nstruct w2c_wasi__snapshot__preview1 {int unused;};\nstatic _Thread_local const char *missing_import;\nstatic _Noreturn void unavailable(const char *name){missing_import=name;wasm_rt_trap(WASM_RT_TRAP_UNREACHABLE);}\nwasm_rt_shared_memory_t* w2c_env_memory(struct w2c_env *env){return &env->memory;}\n`;
for(const d of declarations){
 const args=d[5].split(',').map((s,i)=>s.trim()+' arg'+i).join(', ');
 source+=`${d[3]} ${d[4]}(${args}){unavailable("${d[1]}.${d[2]}");}\n`;
}
const tail=[...header.matchAll(/void (wasm_tailcall_w2c_\w+)\(void \*\*instance_ptr, void \*tail_call_stack, wasm_rt_tailcallee_t \*next\);/g)];
assert.equal(tail.length,85);
for(const d of tail)source+=`void ${d[1]}(void **instance_ptr,void *tail_call_stack,wasm_rt_tailcallee_t *next){unavailable("${d[1]}");}\n`;
source+=`
int native_engine_probe(char *report,size_t capacity){
 missing_import=NULL;
 wasm_rt_init();
 struct w2c_env *env=calloc(1,sizeof(*env));
 struct w2c_wasi__snapshot__preview1 wasi={0};
 w2c_gta *engine=calloc(1,sizeof(*engine));
 if(!env||!engine){free(env);free(engine);snprintf(report,capacity,"FAIL: host allocation");return 1;}
 // Bounds checks and a stable mmap base; this does not request executable RAM.
 wasm_rt_allocate_memory_shared(&env->memory,8192,65536,false,65536);
 volatile int instantiated=0;
 wasm_rt_trap_t trap=wasm_rt_impl_try();
 if(trap){
  snprintf(report,capacity,"FAIL: trap %d%s%s",trap,missing_import?" in unsupported host call ":"",missing_import?missing_import:"");
  if(instantiated)wasm2c_gta_free(engine);
  wasm_rt_free_memory_shared(&env->memory);free(engine);free(env);return 1;
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
 snprintf(report,capacity,"%s: native engine instantiation, strings, memory comparison, math, byte order and shared-memory growth.\\n91,026 engine functions compiled.\\nThis is a diagnostic prototype; game startup, renderer and host services remain to be ported.",failed?"FAIL":"PASS");
 wasm2c_gta_free(engine);wasm_rt_free_memory_shared(&env->memory);free(engine);free(env);
 return failed?1:0;
}
#ifdef NATIVE_PROBE_CLI
int main(void){char report[2048];int code=native_engine_probe(report,sizeof(report));puts(report);return code;}
#endif
`;
fs.writeFileSync(root+'/generated/probe.c',source);
console.log('Generated diagnostic host: unsupported imports fail explicitly.');
