'use strict';
// Stage A: export-only instrumentation. No engine instructions are patched.
const fs = require('node:fs'), path = require('node:path');
const assert = require('node:assert/strict'), crypto = require('node:crypto');
const hash = b => crypto.createHash('sha256').update(b).digest('hex');
const ENGINE = 'e65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e';
const b = fs.readFileSync(process.argv[2]), out = process.argv[3];
assert.equal(hash(b), ENGINE, 'Only the verified original engine is accepted');
assert(out, 'Output directory required');
function reader(start, end) {
  return {p:start, end, u(){ let n=0n,s=0n,c; do {assert(this.p<this.end);c=b[this.p++];n|=BigInt(c&127)<<s;s+=7n;assert(s<=70n);}while(c&128);assert(n<=BigInt(Number.MAX_SAFE_INTEGER));return Number(n);},
    byte(){assert(this.p<this.end);return b[this.p++];}, str(){const n=this.u();assert(this.p+n<=this.end);const v=b.subarray(this.p,this.p+n).toString('utf8');this.p+=n;return v;}};
}
const leb = n => {const a=[];do{const c=n%128;n=Math.floor(n/128);a.push(c|(n?128:0));}while(n);return Buffer.from(a);};
const str = s => {const a=Buffer.from(s);return Buffer.concat([leb(a.length),a]);};
const vt = x => ({127:'i32',126:'i64',125:'f32',124:'f64',123:'v128',112:'funcref',111:'externref'}[x] || assert.fail('Unexpected type '+x));
const sections=[],types=[],imports=[],functions=[],exportList=[],names=new Map(),globalNames=new Map(),memories=[];
for(let at=8;at<b.length;){const offset=at,id=b[at++],r=reader(at,b.length),length=r.u(),start=r.p,end=start+length;assert(end<=b.length);sections.push({id,offset,start,end,sha256:hash(b.subarray(start,end))});at=end;}
for(const s of sections){const r=reader(s.start,s.end);
 if(s.id===1){const n=r.u();for(let i=0;i<n;i++){assert.equal(r.byte(),0x60);types.push({params:Array.from({length:r.u()},()=>vt(r.byte())),results:Array.from({length:r.u()},()=>vt(r.byte()))});}}
 if(s.id===2){const n=r.u();for(let i=0;i<n;i++){const module=r.str(),name=r.str(),kind=r.byte();if(kind===0){const type=r.u();imports.push({module,name,kind:'function',index:functions.length,type,...types[type]});functions.push(type);}else if(kind===2){const flags=r.u(),min=r.u(),max=flags&1?r.u():null;const m={module,name,kind:'memory',memory64:!!(flags&4),shared:!!(flags&2),min,max};imports.push(m);memories.push(m);}else assert.fail('Unexpected import kind '+kind);}}
 if(s.id===3){const n=r.u();for(let i=0;i<n;i++)functions.push(r.u());}
 if(s.id===7){const n=r.u();for(let i=0;i<n;i++){const name=r.str(),kind=r.byte(),index=r.u();exportList.push({name,kind,index,...(kind===0?types[functions[index]]:{})});}}
 if(s.id===0 && r.str()==='name'){while(r.p<r.end){const sub=r.byte(),n=r.u(),end=r.p+n;assert(end<=r.end);if(sub===1||sub===7){const count=r.u();for(let i=0;i<count;i++){const index=r.u(),name=r.str();(sub===1?names:globalNames).set(name,index);}}r.p=end;}}
}
const probes=[['init','__wasm_init_memory',88,[],[]],['shadow','CParaboloidShadow::GetLightShadowIndex(CLightSource const&, bool)',44528,['i64','i32'],['i32']],
 ['main','main',103,['i32','i64'],['i32']],['mode_now',null,566,[],['i32']],['mode',null,568,[],['i32']],['choice',null,569,[],['i32']],
 ['script_flag',null,49773,['i64'],[]]];
// The exact module digest pins all bodies; named probes additionally assert names.
const reverse=new Map([...names].map(([n,i])=>[i,n]));
for(const p of probes){const [alias,expected,index,params,results]=p;assert.deepEqual(types[functions[index]],{params,results},alias);p[1]=reverse.get(index);if(expected)assert.equal(p[1],expected,alias);}
for(const n of ['memcmp','strcmp','strlen','sinf','cosf']){const index=names.get(n);assert.notEqual(index,undefined);probes.push([n,n,index,types[functions[index]].params,types[functions[index]].results]);}
const hooks=['GtaThread::Update(int)','CExplosionManager::AddExplosion(CExplosionManager::CExplosionArgs&, CProjectile*, bool)','CTimeWarper::SetTargetTimeWarp(TW_Dir)','CTextFile::GetInternal(unsigned int)','graphics_commands::HasStreamedTxdLoaded(char const*)','rage::sysMemGrowBuddyAllocator::GrowHeap()','stats_commands::CommandStatSetInt(int, int, bool)','opus_decode_frame','rage::datResourceInfo::GenerateMap(rage::datResourceMap&) const','CScenarioManager::PerformCollisionChecksForScenarioPoint(CScenarioManager::CollisionCheckParams const&)','CMovieMgr::PreloadMovie(char const*, bool)'];
const patchTargets=hooks.map(name=>{const index=names.get(name);return {name,found:index!==undefined,index,...(index===undefined?{}:types[functions[index]])};});
const section=sections.find(s=>s.id===7),er=reader(section.start,section.end),count=er.u();
assert(!exportList.some(x=>x.name.startsWith('migration_')));
assert.equal(globalNames.get('__stack_pointer'),0);assert.equal(globalNames.get('__tls_base'),1);
const additions=probes.map(([alias,,index])=>Buffer.concat([str('migration_'+alias),Buffer.from([0]),leb(index)]));
additions.push(Buffer.concat([str('migration_stack'),Buffer.from([3]),leb(0)]));
const payload=Buffer.concat([leb(count+additions.length),b.subarray(er.p,section.end),...additions]);
const instrumented=Buffer.concat([b.subarray(0,section.offset),Buffer.from([7]),leb(payload.length),payload,b.subarray(section.end)]);
fs.mkdirSync(out,{recursive:true});
fs.writeFileSync(path.join(out,'engine-probe.wasm'),instrumented);
const report={engineSHA256:ENGINE,probeSHA256:hash(instrumented),engineBytes:b.length,probeBytes:instrumented.length,memory:memories,imports,exports:exportList,patchTargets,probes:probes.map(([alias,name,index,params,results])=>({alias,name,index,params,results})),sections:sections.filter(s=>s.id!==0).map(({id,sha256})=>({id,sha256})),modifications:'Additional exports only. Code, data, element, type and import sections unchanged.',gameplayVerified:false};
fs.writeFileSync(path.join(out,'engine-abi.json'),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({engineSHA256:ENGINE,probeSHA256:report.probeSHA256,functions:functions.length,imports:imports.length,memory:memories,probes:report.probes,patchTargets}));
