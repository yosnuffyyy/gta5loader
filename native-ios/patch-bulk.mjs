import fs from 'node:fs';
import assert from 'node:assert/strict';
const root=process.argv[2],writer=root+'/src/c-writer.cc';let code=fs.readFileSync(writer,'utf8');
function replace(from,to){assert.equal(code.split(from).length,2,from);code=code.replace(from,to);}
replace('Write("memory_fill(",','Write(memory->page_limits.is_shared ? "memory_fill_shared(" : "memory_fill(",');
replace('Write("memory_copy(",','Write(dest_memory->page_limits.is_shared ? (src_memory->page_limits.is_shared ? "memory_copy_shared_shared(" : "memory_copy_shared_regular(") : (src_memory->page_limits.is_shared ? "memory_copy_regular_shared(" : "memory_copy("),');
replace('Write("memory_init(",','Write(dest_memory->page_limits.is_shared ? "memory_init_shared(" : "memory_init(",');
replace('Write("LOAD_DATA(",','Write(memory->page_limits.is_shared ? "LOAD_DATA_SHARED(" : "LOAD_DATA(",');
fs.writeFileSync(writer,code);
const file=root+'/src/template/wasm2c.declarations.c';let source=fs.readFileSync(file,'utf8');
let extension=`\n#if defined(WASM_RT_C11_AVAILABLE)\n#include <stdatomic.h>\nstatic inline void memory_fill_shared(wasm_rt_shared_memory_t *mem,u64 d,u32 val,u64 n){\n RANGE_CHECK(mem,d,n);for(u64 i=0;i<n;i++)atomic_store_explicit(mem->data+d+i,(u8)val,memory_order_relaxed);\n}\nstatic inline void load_data_shared(wasm_rt_shared_memory_t *mem,u64 d,const u8 *src,u64 n){\n RANGE_CHECK(mem,d,n);for(u64 i=0;i<n;i++)atomic_store_explicit(mem->data+d+i,src[i],memory_order_relaxed);\n}\n#define LOAD_DATA_SHARED(m,o,i,s) load_data_shared(&(m),o,i,s)\nstatic inline void memory_init_shared(wasm_rt_shared_memory_t *mem,const u8 *src,u32 size,u64 d,u32 s,u32 n){\n if(UNLIKELY((u64)s+n>size))TRAP(OOB);RANGE_CHECK(mem,d,n);\n if(n)load_data_shared(mem,d,src+s,n);\n}\n`;
for(const [dest,src] of [[true,true],[true,false],[false,true]]){
 const suffix=(dest?'shared':'regular')+'_'+(src?'shared':'regular');
 const load=src?'atomic_load_explicit(src->data+s+i,memory_order_relaxed)':'src->data[s+i]';
 const write=dest?`atomic_store_explicit(dst->data+d+i,${load},memory_order_relaxed);`:`dst->data[d+i]=${load};`;
 extension+=`static inline void memory_copy_${suffix}(wasm_rt_${dest?'shared_':''}memory_t *dst,const wasm_rt_${src?'shared_':''}memory_t *src,u64 d,u64 s,u64 n){\n RANGE_CHECK(dst,d,n);RANGE_CHECK(src,s,n);\n if((const void*)dst->data==(const void*)src->data && d>s && d-s<n){for(u64 i=n;i--;) {${write}}}\n else {for(u64 i=0;i<n;i++){${write}}}\n}\n`;
}
fs.writeFileSync(file,source+extension+'#endif\n');
console.log('Added bounds-checked shared bulk memory operations with overlap semantics.');
