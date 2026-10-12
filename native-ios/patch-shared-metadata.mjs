import fs from 'node:fs';
import assert from 'node:assert/strict';
const file=process.argv[2]+'/wasm2c/wasm-rt.h';
let s=fs.readFileSync(file,'utf8');
const begin=s.indexOf('/** A shared Memory object. */'),end=s.indexOf('} wasm_rt_shared_memory_t;',begin);
assert(begin>0&&end>begin);
let part=s.slice(begin,end);
for(const field of ['pages','size']){
 const old=`uint64_t ${field};`;
 assert.equal(part.split(old).length,2,'Pinned shared-memory metadata changed');
 part=part.replace(old,`_Atomic uint64_t ${field};`);
}
fs.writeFileSync(file,s.slice(0,begin)+part+s.slice(end));
const helper=process.argv[2]+'/wasm2c/wasm-rt-mem-impl-helper.inc';
let code=fs.readFileSync(helper,'utf8');
const old='  memory->pages = new_pages;\n  memory->size = new_size;';
assert.equal(code.split(old).length,2,'Pinned memory-growth publication changed');
// A reader observing the new page count must already see the bounds increase.
code=code.replace(old,'  memory->size = new_size;\n  memory->pages = new_pages;');
fs.writeFileSync(helper,code);
console.log('Shared page count and committed byte size now use atomic publication.');
