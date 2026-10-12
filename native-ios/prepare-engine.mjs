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
