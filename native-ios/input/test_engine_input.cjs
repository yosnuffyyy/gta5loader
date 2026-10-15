'use strict';
const fs=require('node:fs'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const file=process.env.NATIVE_INPUT_ENGINE;if(!file)throw Error('Set NATIVE_INPUT_ENGINE to the pinned original engine');
const b=fs.readFileSync(file),sha=x=>crypto.createHash('sha256').update(x).digest('hex');
assert.equal(sha(b),'e65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e');
const leb=n=>{const a=[];do{const c=n%128;n=Math.floor(n/128);a.push(c|(n?128:0));}while(n);return Buffer.from(a);};
function reader(p){return {p,u(){let x=0,m=1,c;do{c=b[this.p++];assert.notEqual(c,undefined);x+=(c&127)*m;m*=128;}while(c&128);return x;},str(){let n=this.u(),s=b.subarray(this.p,this.p+n).toString();this.p+=n;return s;}};}
const sections=[],names=new Map(),bodies=[];for(let r=reader(8);r.p<b.length;){let offset=r.p,id=b[r.p++],n=r.u(),start=r.p,end=start+n;assert(end<=b.length);sections.push({offset,id,start,end});if(id===10){let count=r.u();for(let i=0;i<count;i++){let size=r.u(),at=r.p;bodies.push(b.subarray(at,at+size));r.p+=size;}}if(id===0){if(r.str()==='name'){while(r.p<end){let sub=b[r.p++],length=r.u(),stop=r.p+length;if(sub===1){let count=r.u();for(let i=0;i<count;i++){let index=r.u();names.set(r.str(),index);}}r.p=stop;}}}r.p=end;}
for(const [name,index] of [['rage::ioPad::Update(bool)',10454],['rage::ioPad::UpdateDebug()',10456],['CPad::CPadState::Update(rage::ioPad*)',65452],['CControl::Update()',65359],['CControlMgr::Update()',65444]])assert.equal(names.get(name),index);
/* Exact production hook instructions; no speculative insertion point. 85 imports. */
assert.equal(bodies[65359-85].subarray(9203,9210).toString('hex'),'427f20007d100a');
assert.equal(bodies[65444-85].subarray(4971,4978).toString('hex'),'42afa2f079100a');
const ex=sections.find(s=>s.id===7),r=reader(ex.start),count=r.u();
const additions=[['inputInit',0,88],['inputUpdate',0,10454],['inputDebug',0,10456],['inputConvert',0,65452],['inputStack',3,0]].map(([s,k,i])=>Buffer.concat([leb(Buffer.byteLength(s)),Buffer.from(s),Buffer.from([k]),leb(i)]));
const payload=Buffer.concat([leb(count+additions.length),b.subarray(r.p,ex.end),...additions]);const testBytes=Buffer.concat([b.subarray(0,ex.offset),Buffer.from([7]),leb(payload.length),payload,b.subarray(ex.end)]);
/* Only export section differs. */assert(b.subarray(ex.end).equals(testBytes.subarray(ex.offset+1+leb(payload.length).length+payload.length)));
(async()=>{
 const module=await WebAssembly.compile(testBytes),memory=new WebAssembly.Memory({initial:8192,maximum:65536,shared:true}),imports={},external=[];
 for(const i of WebAssembly.Module.imports(module)){imports[i.module]??={};imports[i.module][i.name]=i.kind==='memory'?memory:(...args)=>{external.push(i.name);throw Error('Unexpected engine import '+i.name+' '+args);};}
 const e=(await WebAssembly.instantiate(module,imports)).exports;e.inputInit();e.inputStack.value=96n*1024n*1024n;
 const bytes=new Uint8Array(memory.buffer),v=new DataView(memory.buffer),pad=12840656,out=100000;v.setInt32(6666144,2048,true);
 let fixture,source;if(process.argv[2]){fixture=fs.readFileSync(process.argv[2]);source='production C bridge bytes';}else{fixture=Buffer.alloc(17*56);source='synthetic protocol bytes (C bridge not compiled locally)';const bits=[6,5,7,4,2,3,0,1,8,11,9,10,12,14,15,13];bits.forEach((bit,i)=>{fixture.writeUInt32LE(1<<bit,i*56+12);fixture.fill(128,i*56+20,i*56+24);fixture[i*56+24]=bit===0?255:0;fixture[i*56+25]=bit===1?255:0;fixture[i*56+50]=fixture[i*56+55]=1;});const p=16*56;fixture.fill(128,p+20,p+24);fixture[p+20]=193;fixture[p+21]=18;fixture[p+22]=237;fixture[p+23]=62;fixture[p+24]=102;fixture[p+25]=179;fixture[p+50]=fixture[p+55]=1;}
 assert.equal(fixture.length,17*56);const masks=[64,32,128,16,4,8,1,2,256,2048,512,1024,4096,16384,32768,8192],offsets=[52,48,40,44,16,28,20,32,76,72,24,36,56,60,64,68];let checks=0;
 function step(frame){e.inputUpdate(BigInt(pad));e.inputDebug(BigInt(pad));for(const [at,n] of [[12,4],[20,6],[50,1],[55,1]])bytes.set(frame.subarray(at,at+n),pad+at);e.inputConvert(BigInt(out),BigInt(pad));}
 const neutral=Buffer.alloc(56);neutral.fill(128,20,24);neutral[50]=neutral[55]=1;
 for(let i=0;i<16;i++){const frame=fixture.subarray(i*56,(i+1)*56);assert.equal(frame.readUInt32LE(12),masks[i]);step(neutral);step(frame);assert.equal(v.getInt32(out+offsets[i],true),255,'CPad button '+i);step(frame);assert.equal(v.getUint32(pad+16,true),masks[i],'original held history '+i);step(neutral);assert.equal(v.getInt32(out+offsets[i],true),0,'released CPad '+i);checks+=4;}
 step(fixture.subarray(16*56));assert(v.getInt32(out,true)>0&&v.getInt32(out+4,true)<0&&v.getInt32(out+8,true)>0&&v.getInt32(out+12,true)<0);assert.equal(v.getInt32(out+20,true),102);assert.equal(v.getInt32(out+32,true),179);checks+=6;step(neutral);assert.equal(v.getInt32(out+32,true),0);checks++;assert.deepEqual(external,[]);
 console.log(JSON.stringify({result:'PASS',checks,source,engineSHA256:sha(b),engineInstructionsModified:false,originalFunctionsExecuted:['ioPad::Update','ioPad::UpdateDebug','CPadState::Update'],hookInstructionsVerified:true,externalCalls:0,physicalGameplayVerified:false}));
})().catch(e=>{console.error(e);process.exitCode=1;});
