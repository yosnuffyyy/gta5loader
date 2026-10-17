import fs from 'node:fs';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
const dir=process.argv[2],out=process.argv[3];assert(dir&&out);
const files=fs.readdirSync(dir).filter(n=>/^gta.*\.c$/.test(n));
function find(symbol){const hits=[];for(const name of files){const code=fs.readFileSync(dir+'/'+name,'utf8');let at=0;for(const line of code.split(String.fromCharCode(10))){if(line.startsWith('u32 '+symbol+'(w2c_gta* instance')||line.startsWith('u64 '+symbol+'(w2c_gta* instance')){assert(line.trimEnd().endsWith('{'));const open=at+line.lastIndexOf('{');let level=1,end=open+1;for(;end<code.length&&level;end++){if(code[end]==='{')level++;else if(code[end]==='}')level--;}assert.equal(level,0);hits.push({name,code,at,end,text:code.slice(at,end)});}at+=line.length+1;}}assert.equal(hits.length,1,'Pinned generated definition '+symbol);return hits[0];}
const grow=find('w2c_gta_f3036'),alloc=find('w2c_gta_f3038');
assert.equal(grow.text.match(/33554432(?:ull|u|U|UL|ULL)?/g)?.length,2,'Growth cap sites');
let clone=grow.text.replace('w2c_gta_f3036(w2c_gta* instance, u64 var_p0)','native_stream_grow(w2c_gta* instance, u64 var_p0, u64 native_segment_limit)');
assert.notEqual(clone,grow.text,'Growth signature');clone=clone.replace(/33554432(?:ull|u|U|UL|ULL)?/g,'native_segment_limit');
const prefix='w2c_gta_f3036(instance, ';assert.equal(alloc.text.split(prefix).length,2,'Only request-side GrowHeap changes');const start=alloc.text.indexOf(prefix),end=alloc.text.indexOf(')',start);assert(end>start);const argument=alloc.text.slice(start+prefix.length,end);assert(argument.startsWith('var_')&&!argument.includes(','));assert(alloc.text.includes('u64 var_p1'),'Request parameter');
const fixed=alloc.text.slice(0,start)+`native_stream_grow(instance, ${argument}, native_stream_growth_limit(var_p1))`+alloc.text.slice(end+1);
const legacy='#ifdef NATIVE_PROBE_CLI\n'+alloc.text.replace('w2c_gta_f3038(','native_stream_try_legacy(')+'\n#endif\n';
/* Apply descending offsets when both functions share a source partition. */
const edits=new Map();for(const [f,text] of [[grow,grow.text+'\n'+clone],[alloc,fixed+'\n'+legacy]]){if(!edits.has(f.name))edits.set(f.name,[]);edits.get(f.name).push({...f,text});}
for(const [name,list] of edits){let code=list[0].code;for(const x of list.sort((a,b)=>b.at-a.at))code=code.slice(0,x.at)+x.text+code.slice(x.end);fs.writeFileSync(dir+'/'+name,code);}
fs.appendFileSync(dir+'/gta-impl.h','\n#include "memory/native_memory.h"\n');
const hash=s=>crypto.createHash('sha256').update(s).digest('hex');fs.writeFileSync(out,JSON.stringify({scope:'Native C request-side growth adaptation; original Wasm/data unchanged. Constructor and largest-block growth remain 32 MiB. Original total budget, locks, allocation/free algorithms and 32 segment slots retained.',functions:[3036,3038],growSHA256:hash(grow.text),trySHA256:hash(alloc.text),originalSegmentCap:33554432,maximumRequestSegment:268435456,request37314560Segment:134217728},null,2)+'\n');
console.log('PASS: pinned native allocator adaptation: request-aware 32/64/128/256 MiB segments; original budget, locks and allocator algorithms retained.');
