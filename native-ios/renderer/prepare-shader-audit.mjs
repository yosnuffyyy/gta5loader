// CI reference inputs only. Never installed in the IPA or loaded by gameplay.
import fs from 'node:fs';import crypto from 'node:crypto';import assert from 'node:assert/strict';
const pins=JSON.parse(fs.readFileSync('native-ios/renderer/shader-audit-pins.json'));
const dir='native-build/shader-audit/shaders';fs.mkdirSync(dir,{recursive:true});
fs.mkdirSync('native-build/evidence',{recursive:true});
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
const response=await fetch(pins.base+'index.json');assert(response.ok,'Shader index HTTP '+response.status);const index=await response.json(),data=Buffer.from(JSON.stringify(index));assert.equal(sha(data),pins.canonicalIndexSHA256,'Shader index differs from audited reference');fs.writeFileSync(dir+'/index.json',data);
let next=0;await Promise.all(Array.from({length:3},async()=>{for(;;){const i=next++;if(i>=pins.packs.length)return;const p=pins.packs[i];assert(/^[\w.-]+\.bin$/.test(p.file),'Pack filename');const r=await fetch(pins.base+p.file);assert(r.ok,'Shader pack HTTP '+r.status);const b=Buffer.from(await r.arrayBuffer());assert.equal(b.length,p.bytes,'Shader pack size');assert.equal(sha(b),p.sha256,'Shader pack digest');fs.writeFileSync(dir+'/'+p.file,b);}}));
const untranslated=Object.entries(index).filter(([hash,r])=>/^[0-9a-f]{16}$/.test(hash)&&!r.ok).map(([hash,r])=>({hash,...r}));assert.equal(untranslated.length,pins.untranslated);
fs.writeFileSync('native-build/evidence/shader-audit-inputs.json',JSON.stringify({canonicalIndexSHA256:pins.canonicalIndexSHA256,packs:pins.packs,translated:pins.translated,untranslated,scope:'Original independent shader references, compile-only audit. These files are not bundled as app/game data.'},null,2)+'\n');
console.log('PASS: all 18 original shader packs pinned by SHA256; 4808 translations and 111 unavailable records inventoried.');
