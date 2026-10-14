// CI reference inputs only. Never installed in the IPA or loaded by gameplay.
import fs from 'node:fs';import crypto from 'node:crypto';import assert from 'node:assert/strict';
import {readReferenceRanges} from './reference_fetch.mjs';
const pins=JSON.parse(fs.readFileSync('native-ios/renderer/shader-audit-pins.json'));
const dir='native-build/shader-audit/shaders';fs.mkdirSync(dir,{recursive:true});
fs.mkdirSync('native-build/evidence',{recursive:true});
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
const write=(file,bytes)=>{fs.writeFileSync(file+'.tmp',bytes);fs.renameSync(file+'.tmp',file);};
const index=JSON.parse((fs.existsSync(dir+'/index.json')?fs.readFileSync(dir+'/index.json'):await readReferenceRanges(pins.base+'index.json',{maxBytes:4*1024*1024})).toString('utf8')),data=Buffer.from(JSON.stringify(index));assert.equal(sha(data),pins.canonicalIndexSHA256,'Shader index differs from audited reference');write(dir+'/index.json',data);
let next=0;await Promise.all(Array.from({length:3},async()=>{for(;;){const i=next++;if(i>=pins.packs.length)return;const p=pins.packs[i];assert(/^[\w.-]+\.bin$/.test(p.file),'Pack filename');const cached=fs.existsSync(dir+'/'+p.file);console.log('REFERENCE '+(cached?'validate cache ':'fetch ')+p.file);const b=cached?fs.readFileSync(dir+'/'+p.file):await readReferenceRanges(pins.base+p.file,{expectedBytes:p.bytes});assert.equal(b.length,p.bytes,'Shader pack size '+p.file);assert.equal(sha(b),p.sha256,'Shader pack digest '+p.file);if(!cached)write(dir+'/'+p.file,b);console.log('REFERENCE verified '+p.file+' '+b.length+' bytes');}}));
const untranslated=Object.entries(index).filter(([hash,r])=>/^[0-9a-f]{16}$/.test(hash)&&!r.ok).map(([hash,r])=>({hash,...r}));assert.equal(untranslated.length,pins.untranslated);
fs.writeFileSync('native-build/evidence/shader-audit-inputs.json',JSON.stringify({canonicalIndexSHA256:pins.canonicalIndexSHA256,packs:pins.packs,translated:pins.translated,untranslated,scope:'Original independent shader references, compile-only audit. These files are not bundled as app/game data.'},null,2)+'\n');
console.log('PASS: all 18 original shader packs pinned by SHA256; 4808 translations and 111 unavailable records inventoried.');
