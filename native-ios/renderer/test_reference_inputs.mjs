// Original, pinned CI shader inputs only. These files are never app/game data.
import fs from 'node:fs';import zlib from 'node:zlib';import crypto from 'node:crypto';import assert from 'node:assert/strict';
const dir='native-ios/renderer',pins=JSON.parse(fs.readFileSync(dir+'/shader-audit-pins.json')),sha=b=>crypto.createHash('sha256').update(b).digest('hex');let compressed=0,raw=0;
const index=zlib.gunzipSync(fs.readFileSync(dir+'/verification-inputs/index.json.gz'),{maxOutputLength:4*1024*1024});assert.equal(sha(JSON.stringify(JSON.parse(index))),pins.canonicalIndexSHA256);
for(const p of pins.packs){const gz=fs.readFileSync(dir+'/verification-inputs/'+p.file+'.gz');assert(gz.length<=2*1024*1024);const b=zlib.gunzipSync(gz,{maxOutputLength:p.bytes});assert.equal(b.length,p.bytes);assert.equal(sha(b),p.sha256);compressed+=gz.length;raw+=b.length;assert.throws(()=>zlib.gunzipSync(gz.subarray(0,gz.length-1),{maxOutputLength:p.bytes}));}
assert.throws(()=>zlib.gunzipSync(zlib.gzipSync(Buffer.alloc(1024)),{maxOutputLength:1023}),/larger|large/i);
console.log('PASS: 18 bundled gzip shader packs and original index; exact pinned sizes/SHA256, truncated inputs and decompression bounds; '+raw+' raw bytes / '+compressed+' compressed pack bytes. CI-only inputs; no network or gameplay success simulated.');
