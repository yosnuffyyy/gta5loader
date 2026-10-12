import fs from 'node:fs';
import assert from 'node:assert/strict';
import {execFileSync as run} from 'node:child_process';
const out='native-build/simd';fs.mkdirSync(out,{recursive:true});
const cases=[];
for(const op of ['v128.load','v128.load8_splat','v128.load16_splat','v128.load32_splat','v128.load64_splat','v128.load8x8_s','v128.load8x8_u','v128.load16x4_s','v128.load16x4_u','v128.load32x2_s','v128.load32x2_u','v128.load32_zero','v128.load64_zero'])cases.push('local.get 0 local.get 1 '+op+' v128.store');
const vector='v128.const i32x4 0x12345678 0x89abcdef 0x77665544 0x11223344';
for(const width of [8,16,32,64])for(let lane=0;lane<128/width;lane++){
 cases.push(`local.get 0 local.get 1 ${vector} v128.load${width}_lane ${lane} v128.store`);
 cases.push(`local.get 0 local.get 1 v128.load v128.store${width}_lane ${lane}`);
}
assert.equal(cases.length,73);
const wat='(module (memory (import "env" "memory") 1 4 shared)\n'+cases.map((body,i)=>`(func (export "op${i}") (param i32 i32) ${body})`).join('\n')+'\n)';
fs.writeFileSync(out+'/simd.wat',wat);
run('native-build/wabt-build/wat2wasm',['--enable-threads',out+'/simd.wat','-o',out+'/simd.wasm'],{stdio:'inherit'});
const bytes=fs.readFileSync(out+'/simd.wasm'),memory=new WebAssembly.Memory({initial:1,maximum:4,shared:true});
const instance=await WebAssembly.instantiate(bytes,{env:{memory}}),view=new Uint8Array(memory.buffer);
const expected=[];for(let i=0;i<cases.length;i++){
 for(let n=0;n<256;n++)view[n]=(n*197+73)&255;
 instance.instance.exports['op'+i](129,1);expected.push([...view.subarray(129,161)]);
}
run('native-build/wabt-build/wasm2c',['--enable-threads','-n','simd',out+'/simd.wasm','-o',out+'/simd.c'],{stdio:'inherit'});
let c=`#include "simd.h"\n#include "wasm-rt-impl.h"\n#include "wasm-rt-exceptions.h"\n#include <assert.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdatomic.h>\nstruct w2c_env{wasm_rt_shared_memory_t memory;};\nwasm_rt_shared_memory_t *w2c_env_memory(struct w2c_env *e){return &e->memory;}\nstatic void (*ops[])(w2c_simd*,u32,u32)={${cases.map((_,i)=>'w2c_simd_op'+i).join(',')}};\nstatic unsigned char expected[][32]={${expected.map(row=>'{'+row.join(',')+'}').join(',')}};\nint main(void){wasm_rt_init();struct w2c_env env={0};wasm_rt_allocate_memory_shared(&env.memory,1,4,false,65536);w2c_simd instance;wasm2c_simd_instantiate(&instance,&env);\n for(unsigned i=0;i<73;i++){for(unsigned n=0;n<256;n++)atomic_store(env.memory.data+n,(n*197+73)&255);ops[i](&instance,129,1);for(unsigned n=0;n<32;n++)assert(atomic_load(env.memory.data+129+n)==expected[i][n]);}\n wasm_rt_trap_t trap=wasm_rt_impl_try();if(!trap){w2c_simd_op0(&instance,100,65530);abort();}assert(trap==WASM_RT_TRAP_OOB);\n trap=wasm_rt_impl_try();if(!trap){w2c_simd_op0(&instance,65530,100);abort();}assert(trap==WASM_RT_TRAP_OOB);\n wasm2c_simd_free(&instance);wasm_rt_free_memory_shared(&env.memory);wasm_rt_free();puts("PASS: 73 shared SIMD load/store, splat, extend, zero and lane cases match WebAssembly; unaligned accesses and bounds traps checked.");}\n`;
fs.writeFileSync(out+'/test.c',c);
const flags=['-std=gnu11','-O1','-Werror=incompatible-pointer-types','-DWASM_RT_USE_MMAP=1','-DWASM_RT_MEMCHECK_BOUNDS_CHECK=1','-DWASM_RT_MEMCHECK_GUARD_PAGES=0','-DWASM_RT_SKIP_SIGNAL_RECOVERY=1','-Inative-build/wabt/wasm2c','-Inative-build/wabt/third_party/simde','-I'+out];
run('clang',[...flags,out+'/test.c',out+'/simd.c',...['wasm-rt-impl.c','wasm-rt-mem-impl.c','wasm-rt-exceptions-impl.c'].map(n=>'native-build/wabt/wasm2c/'+n),'-o',out+'/test'],{stdio:'inherit'});
const report=run(out+'/test',[],{encoding:'utf8',timeout:30000});console.log(report);fs.writeFileSync('native-build/evidence/simd-tests.txt',report);
