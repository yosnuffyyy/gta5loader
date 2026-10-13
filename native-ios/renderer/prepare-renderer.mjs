import fs from 'node:fs';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
const dir='native-ios/renderer',out='native-build/wgpu';
const pin=JSON.parse(fs.readFileSync(dir+'/backend.json'));
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
fs.mkdirSync(out,{recursive:true});
const evidence={...pin,downloaded:[],shaderFixtures:JSON.parse(fs.readFileSync(dir+'/shader-fixtures.json'))};
for(const r of evidence.shaderFixtures.records){if(hash(fs.readFileSync(dir+'/'+r.stage+'.wgsl'))!==r.packedSHA256)throw Error('Shader fixture changed: '+r.stage);}
for(const target of ['macos','ios']){
 const asset=pin[target];const r=await fetch('https://github.com/gfx-rs/wgpu-native/releases/download/'+pin.version+'/'+asset.asset);if(!r.ok)throw Error('Backend download '+r.status);
 const bytes=Buffer.from(await r.arrayBuffer());if(bytes.length!==asset.bytes||hash(bytes)!==asset.sha256)throw Error('Backend digest/length mismatch '+target);
 const zip=out+'/'+target+'.zip';fs.writeFileSync(zip,bytes);execFileSync('ditto',['-x','-k',zip,out+'/'+target]);
 for(const f of ['include/webgpu/webgpu.h','include/webgpu/wgpu.h','lib/libwgpu_native.a'])if(!fs.statSync(out+'/'+target+'/'+f).isFile())throw Error('Missing backend '+f);
 if(fs.readFileSync(out+'/'+target+'/wgpu-native-meta/wgpu-native-git-tag','utf8').trim()!==pin.version)throw Error('Backend version mismatch');
 evidence.downloaded.push({target,...asset,staticLibraryBytes:fs.statSync(out+'/'+target+'/lib/libwgpu_native.a').size});
}
for(const name of ['LICENSE.MIT','LICENSE.APACHE']){
 const r=await fetch('https://raw.githubusercontent.com/gfx-rs/wgpu-native/'+pin.commit+'/'+name);if(!r.ok)throw Error('Backend license unavailable');fs.writeFileSync(out+'/'+name,await r.text());
}
fs.writeFileSync('native-build/evidence/renderer-backend.json',JSON.stringify(evidence,null,2)+'\n');
console.log('PASS: pinned native Metal backend downloads and game shader fixture digests.');
