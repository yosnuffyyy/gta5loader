// Source preservation/reflection test. Does not compile WGSL or claim GPU pixels.
import fs from 'node:fs';
import crypto from 'node:crypto';
import assert from 'node:assert/strict';
const [dir='native-build/shader-audit/shaders',renderer='native-ios/renderer',out='native-build/evidence/inter-stage-source-test.json']=process.argv.slice(2);
const pins=JSON.parse(fs.readFileSync(renderer+'/shader-audit-pins.json'));
const indexBytes=fs.readFileSync(dir+'/index.json'),index=JSON.parse(indexBytes);
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
assert.equal(sha(Buffer.from(JSON.stringify(index))),pins.canonicalIndexSHA256);
const packs=pins.packs.map(p=>{const b=fs.readFileSync(dir+'/'+p.file);assert.equal(b.length,p.bytes);assert.equal(sha(b),p.sha256);return b;});
const linker=fs.readFileSync(renderer+'/inter_stage.inc','utf8');
const patterns=[...linker.matchAll(/@"((?:[^"\\]|\\.)*)"/g)].map(m=>JSON.parse('"'+m[1]+'"'));
const vertexPattern=patterns.find(s=>s.startsWith('@vertex'));
const fragmentPattern=patterns.find(s=>s.startsWith('@fragment'));
assert(vertexPattern&&fragmentPattern,'Production entry reflection patterns not found');
// Independent balanced delimiter walk, compared with production regex ranges.
function closing(s,start,left,right){assert.equal(s[start],left);let depth=1;for(let i=start+1;i<s.length;i++){if(s[i]===left)depth++;else if(s[i]===right&&!--depth)return i;}throw Error('Unclosed interface delimiter');}
function range(s,vertex){const entry=s.indexOf(vertex?'@vertex':'@fragment');assert(entry>=0);const fn=s.indexOf('fn main',entry);assert(fn>=entry);const start=s.indexOf('(',fn),end=closing(s,start,'(',')');
 if(!vertex)return [start+1,end];const arrow=s.indexOf('->',end);assert(arrow>end);const brace=s.indexOf('{',arrow),type=s.slice(arrow+2,brace).trim();
 if(type.startsWith('@builtin')){let at=arrow+2;while(/\s/.test(s[at]))at++;return [at,brace];}const st=s.indexOf('struct '+type+' ');assert(st>=0,type);const open=s.indexOf('{',st);return [open+1,closing(s,open,'{','}')];}
function productionRange(s,vertex){const m=new RegExp(vertex?vertexPattern:fragmentPattern,'d').exec(s);assert(m,'Production interface regex failed');if(!vertex)return m.indices[1];const type=m[1].trim();if(type.includes('@builtin'))return m.indices[1];const st=new RegExp('struct\\s+'+type+'\\s*\\{([^}]*)\\}','d').exec(s);assert(st,'Production output struct regex failed');return st.indices[1];}
function locations(s,r){const a=[...s.slice(...r).matchAll(/@location\((\d+)u?\)/g)].map(m=>+m[1]);assert.equal(new Set(a).size,a.length);assert(a.every(n=>n<32));return a;}
function plan(loc){const taken=new Set(loc.filter(n=>n<15)),map=new Map();for(const old of loc.filter(n=>n>=15).sort((a,b)=>a-b)){const free=Array.from({length:15},(_,i)=>i).find(n=>!taken.has(n));assert.notEqual(free,undefined,'Interface cannot fit 15 variables');taken.add(free);map.set(old,free);}return map;}
function relabel(s,r,map){return s.slice(0,r[0])+s.slice(...r).replace(/@location\((\d+)(u?)\)/g,(text,n,suffix)=>map.has(+n)?'@location('+map.get(+n)+suffix+')':text)+s.slice(r[1]);}
const interfaces=new Map();let inspected=0,graphics=0,high=0,maxOutputs=0;
for(const [hash,rec] of Object.entries(index)){if(!/^[a-f0-9]{16}$/.test(hash)||!rec.ok)continue;inspected++;const [p,offset,bytes]=rec.p;const s=packs[p].subarray(offset,offset+bytes).toString('utf8');if(rec.stage==='cs')continue;graphics++;const vertex=rec.stage==='vs';const a=range(s,vertex),b=productionRange(s,vertex);assert.deepEqual(a,b,hash+' reflection range');const loc=locations(s,a),map=plan(loc),changed=relabel(s,a,map);
 assert.equal(changed.slice(0,a[0]),s.slice(0,a[0]));assert.equal(changed.slice(changed.length-(s.length-a[1])),s.slice(a[1]));
 // Reversing only mapped location attributes must recover every source byte:
 // types, interpolation, builtin clip fields and all function bodies included.
 const inverse=new Map([...map].map(([old,n])=>[n,old]));assert.equal(relabel(changed,range(changed,vertex),inverse),s,hash+' semantic/source preservation');assert(locations(changed,range(changed,vertex)).every(n=>n<15));if(vertex){maxOutputs=Math.max(maxOutputs,loc.length);if(map.size)high++;}interfaces.set(hash,{rec,s,a,loc,map,changed});}
assert.equal(inspected,4808);assert.equal(graphics,4779);assert.equal(high,500);
const vs=interfaces.get('7db515e410659eca'),ps=interfaces.get('394c572f727d25e8');assert.deepEqual(vs.loc,[0,1,2,3,4,5,6,15]);assert.deepEqual(ps.loc,[0,1,2,3,4,5,6]);assert.deepEqual([...vs.map],[[15,7]]);assert(vs.changed.includes('@builtin(clip_distances)'));assert(vs.changed.includes('enable clip_distances;'));
const fields=(s,r)=>[...s.slice(...r).matchAll(/@location\((\d+)u?\)\s*(?:@interpolate\([^)]*\)\s*)?(\w+)\s*:\s*(vec[234]<\w+>|\w+)/g)].map(m=>({location:+m[1],name:m[2],type:m[3]}));
const vo=fields(vs.s,vs.a),pi=fields(ps.s,ps.a);assert.equal(vo.length,8);assert.equal(pi.length,7);for(const input of pi)assert.equal(vo.find(o=>o.location===input.location)?.type,input.type,'Exact pair input type mismatch');
const result={result:'PASS_SOURCE_ONLY',modulesInspected:inspected,graphicsInterfaces:graphics,sparseVertexInterfaces:high,maximumVertexOutputCount:maxOutputs,deviceLocationLimit:15,exactPair:{vertex:vs.rec,fragment:ps.rec,vertexOutputs:vo,fragmentInputs:pi,map:[...vs.map],allOutputsRetained:true,clipBuiltinsRetained:true},gpuPipelineValidation:false,pixelVerification:false};
fs.mkdirSync(out.slice(0,out.lastIndexOf('/')),{recursive:true});fs.writeFileSync(out,JSON.stringify(result,null,2)+'\n');
console.log('PASS: source-only varying interface audit: 4808 pinned modules; 4779 graphics interfaces; 500 sparse vertex outputs; exact Prologue pair 15->7 with every source byte preserved except location labels. GPU pipeline/pixel verification remains separate.');
