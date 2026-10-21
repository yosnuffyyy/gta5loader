'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),crypto=require('node:crypto');
(async()=>{
 const b=fs.readFileSync(process.argv[2]);assert.equal(crypto.createHash('sha256').update(b).digest('hex'),'6386a4047a5c9c5d5384b1127b4dbd37d76970abfca620c9632bd9335c1b6d7b');
 const module=await WebAssembly.compile(b),memory=new WebAssembly.Memory({initial:8192,maximum:65536,shared:true}),imports={};
 for(const i of WebAssembly.Module.imports(module)){imports[i.module]??={};imports[i.module][i.name]=i.kind==='memory'?memory:()=>{throw Error('Unexpected import '+i.name);};}
 const i=await WebAssembly.instantiate(module,imports),e=i.exports;e.migration_init();e.migration_stack.value=96n*1024n*1024n;
 const trig=[0,.25,-.5,1,3.25,100].map(x=>({x,sin:e.migration_sinf(x),cos:e.migration_cosf(x)}));
 const report={origin:'original engine / Node WebAssembly reference',trig};fs.writeFileSync(process.argv[3],JSON.stringify(report,null,2));
 if(process.argv[4]){const native=JSON.parse(fs.readFileSync(process.argv[4]));assert.deepEqual(native.engineChecks.trig,trig);console.log('PASS: original math outputs exactly match independent WebAssembly reference');}
})().catch(e=>{console.error(e);process.exitCode=1;});
