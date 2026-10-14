const fs=require('fs'),assert=require('assert/strict'),b=fs.readFileSync('work/minimal-engine/game.wasm');
const leb=n=>{const a=[];do{let v=n%128;n=Math.floor(n/128);a.push(v|(n?128:0));}while(n);return Buffer.from(a);};const r={p:8,u(){let n=0,s=1,c;do{c=b[this.p++];n+=(c&127)*s;s*=128;}while(c&128);return n;}};let ex;
while(r.p<b.length){const start=r.p,id=b[r.p++],len=r.u(),p=r.p,end=p+len;if(id===7){let count=r.u();ex={start,end,count,p:r.p};}r.p=end;}
const name=Buffer.from('native_test_statvfs'),payload=Buffer.concat([leb(ex.count+1),b.subarray(ex.p,ex.end),leb(name.length),name,Buffer.from([0]),leb(90925)]);const patched=Buffer.concat([b.subarray(0,ex.start),Buffer.from([7]),leb(payload.length),payload,b.subarray(ex.end)]);
(async()=>{const module=await WebAssembly.compile(patched),memory=new WebAssembly.Memory({initial:8192,maximum:65536,shared:true}),v=new DataView(memory.buffer),imports={};let calls=0;
const fields=[0n,8192n,0x100000011n,0x100000010n,0x10000000fn,0x200000033n,0x100000033n,123n,255n,4096n,3n,0n,0n,0n,0n];
for(const imp of WebAssembly.Module.imports(module)){imports[imp.module]||={};imports[imp.module][imp.name]=imp.kind==='memory'?memory:()=>{throw Error('Unexpected import '+imp.name)};}
imports.env.__syscall_statfs64=(path,size,out)=>{assert.equal(path,1024n);assert.equal(size,120n);for(let i=0;i<fields.length;i++)v.setBigUint64(Number(out)+i*8,fields[i],true);calls++;return 0;};
const e=(await WebAssembly.instantiate(module,imports)).exports;const restore=Object.keys(e).find(n=>/emscripten_stack_restore$/.test(n));assert(restore);e[restore](65536n);
new Uint8Array(memory.buffer,2048,128).fill(0xa5);assert.equal(e.native_test_statvfs(1024n,2056n),0);
const expected=[8192n,4096n,fields[2],fields[3],fields[4],fields[5],fields[6],fields[6],123n,3n,255n,0n,0n,0n];for(let i=0;i<expected.length;i++)assert.equal(v.getBigUint64(2056+8*i,true),expected[i]);assert.equal(v.getBigUint64(2048,true),0xa5a5a5a5a5a5a5a5n);assert.equal(v.getBigUint64(2168,true),0xa5a5a5a5a5a5a5a5n);assert.equal(calls,1);
const result={result:'PASS',engineFunction:90925,engineName:'statvfs',syscallBytes:120,libcBytes:112,blockCountsAbove32Bits:true,differentBlockAndFragmentSizes:true,allOutputFieldsAndGuards:true};fs.writeFileSync('work/native-statfs/wasm-abi-result.json',JSON.stringify(result,null,2)+'\n');console.log(result);
})().catch(e=>{console.error(e);process.exitCode=1});
