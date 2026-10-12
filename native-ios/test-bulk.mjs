import fs from 'node:fs';
import {execFileSync as run} from 'node:child_process';
const out='native-build/bulk';fs.mkdirSync(out,{recursive:true});
const wat=`(module (memory (import "env" "memory") 1 4 shared)
 (data (i32.const 0) "active data") (data $data "Hello native bulk memory")
 (func (export "fill") (param i32 i32 i32) local.get 0 local.get 1 local.get 2 memory.fill)
 (func (export "copy") (param i32 i32 i32) local.get 0 local.get 1 local.get 2 memory.copy)
 (func (export "init") (param i32 i32 i32) local.get 0 local.get 1 local.get 2 memory.init $data)
 (func (export "drop") data.drop $data))`;
fs.writeFileSync(out+'/bulk.wat',wat);
run('native-build/wabt-build/wat2wasm',['--enable-threads',out+'/bulk.wat','-o',out+'/bulk.wasm'],{stdio:'inherit'});
const memory=new WebAssembly.Memory({initial:1,maximum:4,shared:true});
const {instance}=await WebAssembly.instantiate(fs.readFileSync(out+'/bulk.wasm'),{env:{memory}}),e=instance.exports,view=new Uint8Array(memory.buffer);
const initial=[...view.subarray(0,11)];
e.init(20,0,24);e.copy(23,20,21);e.copy(15,23,21);e.fill(38,511,8);e.fill(65536,0,0);e.copy(65536,65536,0);e.drop();e.init(65536,0,0);
const expected=[...view.subarray(0,80)];
run('native-build/wabt-build/wasm2c',['--enable-threads','-n','bulk',out+'/bulk.wasm','-o',out+'/bulk.c'],{stdio:'inherit'});
fs.writeFileSync(out+'/test.c',`#include "bulk.h"\n#include "wasm-rt-impl.h"\n#include "wasm-rt-exceptions.h"\n#include <assert.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdatomic.h>\nstruct w2c_env{wasm_rt_shared_memory_t memory;};\nwasm_rt_shared_memory_t *w2c_env_memory(struct w2c_env *e){return &e->memory;}\nstatic const unsigned char initial[]={${initial.join(',')}},expected[]={${expected.join(',')}};\nint main(void){wasm_rt_init();struct w2c_env env={0};wasm_rt_allocate_memory_shared(&env.memory,1,4,false,65536);w2c_bulk i;wasm2c_bulk_instantiate(&i,&env);\n for(unsigned n=0;n<sizeof(initial);n++)assert(atomic_load(env.memory.data+n)==initial[n]);\n w2c_bulk_init(&i,20,0,24);w2c_bulk_copy(&i,23,20,21);w2c_bulk_copy(&i,15,23,21);w2c_bulk_fill(&i,38,511,8);w2c_bulk_fill(&i,65536,0,0);w2c_bulk_copy(&i,65536,65536,0);w2c_bulk_drop(&i);w2c_bulk_init(&i,65536,0,0);\n for(unsigned n=0;n<sizeof(expected);n++)assert(atomic_load(env.memory.data+n)==expected[n]);\n wasm_rt_trap_t trap=wasm_rt_impl_try();if(!trap){w2c_bulk_init(&i,20,0,1);abort();}assert(trap==WASM_RT_TRAP_OOB);\n trap=wasm_rt_impl_try();if(!trap){w2c_bulk_copy(&i,1,65535,2);abort();}assert(trap==WASM_RT_TRAP_OOB);\n for(unsigned n=0;n<sizeof(expected);n++)assert(atomic_load(env.memory.data+n)==expected[n]);\n wasm2c_bulk_free(&i);wasm_rt_free_memory_shared(&env.memory);wasm_rt_free();puts("PASS: shared bulk init/fill/copy match WebAssembly, both overlap directions, zero length at boundary, dropped data and bounds trap without partial writes.");}\n`);
run('clang',['-std=gnu11','-O1','-Werror=incompatible-pointer-types','-DWASM_RT_USE_MMAP=1','-DWASM_RT_MEMCHECK_BOUNDS_CHECK=1','-DWASM_RT_MEMCHECK_GUARD_PAGES=0','-DWASM_RT_SKIP_SIGNAL_RECOVERY=1','-Inative-build/wabt/wasm2c','-I'+out,out+'/test.c',out+'/bulk.c',...['wasm-rt-impl.c','wasm-rt-mem-impl.c','wasm-rt-exceptions-impl.c'].map(n=>'native-build/wabt/wasm2c/'+n),'-o',out+'/test'],{stdio:'inherit'});
const report=run(out+'/test',[],{encoding:'utf8',timeout:30000});console.log(report);fs.writeFileSync('native-build/evidence/bulk-tests.txt',report);
