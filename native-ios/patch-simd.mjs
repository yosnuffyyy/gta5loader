import fs from 'node:fs';
import assert from 'node:assert/strict';
const root=process.argv[2];
const writer=root+'/src/c-writer.cc';let code=fs.readFileSync(writer,'utf8');
for(const kind of ['SimdLoadLaneExpr','SimdStoreLaneExpr','LoadSplatExpr','LoadZeroExpr']){
 const start=code.indexOf('void CWriter::Write(const '+kind+'& expr) {');assert(start>=0);
 const end=code.indexOf('\nvoid CWriter::',start+1);assert(end>start);
 let fn=code.slice(start,end);
 const from=kind.includes('Lane')?'func, expr.val,':'" = ", func,';
 const to=kind.includes('Lane')?'GetMemoryAPIString(*memory, std::string(func) + std::to_string(expr.val)),':'" = ", GetMemoryAPIString(*memory, func),';
 assert.equal(fn.split(from).length,2,kind);fn=fn.replace(from,to);
 code=code.slice(0,start)+fn+code.slice(end);
}
fs.writeFileSync(writer,code);
const file=root+'/src/template/wasm2c_simd.declarations.c';let template=fs.readFileSync(file,'utf8');
assert(!template.includes('native_shared_simd_read'));
// These non-atomic Wasm accesses may tear; relaxed byte operations avoid C data
// races and support unaligned addresses without pretending a vector is atomic.
let extra=`\n#if defined(WASM_RT_C11_AVAILABLE)\n#if WABT_BIG_ENDIAN\n#error Native SIMD extension currently targets little-endian Apple hardware.\n#endif\n#include <stdatomic.h>\nstatic inline void native_shared_simd_read(wasm_rt_shared_memory_t *mem,u64 addr,void *out,size_t size){\n uint8_t *p=out;for(size_t i=0;i<size;i++)p[i]=atomic_load_explicit(mem->data+addr+i,memory_order_relaxed);\n}\nstatic inline void native_shared_simd_write(wasm_rt_shared_memory_t *mem,u64 addr,const void *in,size_t size){\n const uint8_t *p=in;for(size_t i=0;i<size;i++)atomic_store_explicit(mem->data+addr+i,p[i],memory_order_relaxed);\n}\n`;
const definitions=new Map();
for(const m of template.matchAll(/^DEFINE_SIMD_(LOAD_FUNC|LOAD_LANE|STORE|STORE_LANE)\(([^\n]+)\)/gm)){
 const args=m[2].split(',').map(v=>v.trim());definitions.set(args[0],{kind:m[1],args});
}
assert.equal(definitions.size,74);
for(const {kind,args} of definitions.values()){
 const [name,func,t,lane]=kind==='STORE'?[args[0],'simde_wasm_v128_store',args[1],null]:args;
 const isLoad=kind.startsWith('LOAD'),isLane=kind.endsWith('LANE'),hasValue=!isLoad||isLane;
 const ret=isLoad?'v128':'void';
 extra+=`static inline ${ret} ${name}_shared_unchecked(uint8_t *base,wasm_rt_shared_memory_t *mem,u64 addr${hasValue?',v128 value':''}){\n ${t} temporary;\n`;
 if(isLoad){extra+=` native_shared_simd_read(mem,addr,&temporary,sizeof(temporary));\n return ${func}(&temporary${isLane?',value,'+lane:''});\n`;}
 else{extra+=` ${func}(&temporary,value${isLane?','+lane:''});\n native_shared_simd_write(mem,addr,&temporary,sizeof(temporary));\n`;}
 extra+='}\n';
 extra+=hasValue?`DEF_MEM_CHECKS1(${name}_shared,_shared_,${t},${isLoad?'return':''},${ret},v128)\n`:`DEF_MEM_CHECKS0(${name}_shared,_shared_,${t},return,${ret})\n`;
}
fs.writeFileSync(file,template+extra+'#endif\n');
console.log('Added 74 checked shared-memory SIMD helpers and corrected lane/splat/zero dispatch.');
