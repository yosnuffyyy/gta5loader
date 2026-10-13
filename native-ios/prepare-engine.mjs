import fs from 'node:fs';
import crypto from 'node:crypto';
import assert from 'node:assert/strict';
import path from 'node:path';
const root=process.argv[2]||'.', out=process.argv[3]||'native-build';
const manifest=JSON.parse(fs.readFileSync(path.join(root,'offline-manifest.json')));
const entry=manifest.files.find(f=>f.path==='/b/8b0b5899ed/game.wasm');
assert.equal(entry.sha256,'e65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const chunks=entry.setupChunks.map((p,i)=>{
 assert(/^\/gta5loader\/engine32\/game\.wasm\.\d{3}\.[a-f0-9]{12}\.bin$/.test(p));
 const bytes=fs.readFileSync(path.join(root,p.slice('/gta5loader/'.length)));
 assert.equal(hash(bytes),entry.chunks[i],p);return bytes;
});
const bytes=Buffer.concat(chunks);assert.equal(bytes.length,entry.size);assert.equal(hash(bytes),entry.sha256);
fs.mkdirSync(out,{recursive:true});
fs.writeFileSync(path.join(out,'engine-original.wasm'),bytes);
console.log('Verified and assembled v7 engine:',bytes.length,'bytes',hash(bytes));
function leb(n){const a=[];do{let b=n%128;n=Math.floor(n/128);a.push(b|(n?128:0));}while(n);return Buffer.from(a);}
function reader(b,p=0){return {p,u(){let n=0,s=1,c;do{c=b[this.p++];assert.notEqual(c,undefined);n+=(c&127)*s;s*=128;}while(c&128);return n;},str(){let n=this.u();let s=b.subarray(this.p,this.p+n).toString();this.p+=n;return s;}};}
const sections=[],names=new Map();let r=reader(bytes,8);
while(r.p<bytes.length){let offset=r.p,id=bytes[r.p++],n=r.u(),start=r.p,end=start+n;assert(end<=bytes.length);sections.push({id,offset,start,end});if(id===0){let nr=reader(bytes,start);if(nr.str()==='name'){while(nr.p<end){let sub=bytes[nr.p++],length=nr.u(),stop=nr.p+length;if(sub===1){let count=nr.u();for(let i=0;i<count;i++){let index=nr.u();names.set(nr.str(),index);}}nr.p=stop;}}}r.p=end;}
const probes=['memcmp','strcmp','strlen','sinf','cosf','fopen','fclose','fread','fwrite','__fseeko','__ftello'].map(name=>{assert(names.has(name));return {name:name.replace(/^__/,''),sourceName:name,index:names.get(name)};});
// Native 0.3 calls existing generated C symbols; no additional Wasm exports
// or engine instructions are needed. Pin names as well as generated signatures.
for(const [name,index] of [['__pthread_create',90879],['__pthread_join',90884],['__errno_location',90695],['wasm_httpfs_lookup',194],['wasm_httpfs_pread',198]])assert.equal(names.get(name),index,'Thread ABI changed: '+name);
const ex=sections.find(s=>s.id===7),er=reader(bytes,ex.start),oldCount=er.u();
const additions=probes.map(({name,index})=>{let n=Buffer.from('native_probe_'+name);return Buffer.concat([leb(n.length),n,Buffer.from([0]),leb(index)]);});
const payload=Buffer.concat([leb(oldCount+probes.length),bytes.subarray(er.p,ex.end),...additions]);
const probeBytes=Buffer.concat([bytes.subarray(0,ex.offset),Buffer.from([7]),leb(payload.length),payload,bytes.subarray(ex.end)]);
fs.writeFileSync(path.join(out,'engine-probe.wasm'),probeBytes);
fs.mkdirSync(path.join(out,'evidence'),{recursive:true});
fs.writeFileSync(path.join(out,'evidence','engine-inventory.json'),JSON.stringify({originalSHA256:hash(bytes),probeSHA256:hash(probeBytes),modification:'Add eleven exports only; engine instructions unchanged',probes},null,2));
const module=await WebAssembly.compile(probeBytes),memory=new WebAssembly.Memory({initial:8192,maximum:65536,shared:true});
const imports={};for(const imp of WebAssembly.Module.imports(module)){imports[imp.module]||={};imports[imp.module][imp.name]=imp.kind==='memory'?memory:(()=>{throw Error('Unexpected reference import '+imp.module+'.'+imp.name);});}
const instance=await WebAssembly.instantiate(module,imports),e=instance.exports;
const a=512*1048576-4096,b=a+1024,view=new Uint8Array(memory.buffer);
view.set([65,108,112,104,97,0],a);view.set([65,108,112,104,97,0],b);
assert.equal(Number(e.native_probe_strlen(BigInt(a))),5);
assert.equal(e.native_probe_strcmp(BigInt(a),BigInt(b)),0);
assert.equal(e.native_probe_memcmp(BigInt(a),BigInt(b),6n),0);
view[b+4]=98;assert(e.native_probe_strcmp(BigInt(a),BigInt(b))<0);
assert(e.native_probe_memcmp(BigInt(a),BigInt(b),6n)<0);
const trig=[0,0.25,-0.5,1,3.25,100].map(x=>({x,sin:e.native_probe_sinf(x),cos:e.native_probe_cosf(x)}));
fs.writeFileSync(path.join(out,'evidence','reference.json'),JSON.stringify({strings:'PASS',trig},null,2));
const c=`static const struct {float x,s,c;} probe_trig[]={\n${trig.map(v=>`{${v.x.toExponential(9)}f,${v.sin.toExponential(9)}f,${v.cos.toExponential(9)}f}`).join(',\n')}\n};\n`;
fs.writeFileSync(path.join(out,'probe_reference.h'),c);
console.log('PASS: unchanged engine string/memory/math functions in WebAssembly reference.');
