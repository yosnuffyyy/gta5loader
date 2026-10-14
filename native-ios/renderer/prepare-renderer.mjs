import fs from 'node:fs';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
const dir='native-ios/renderer',out='native-build/wgpu';
const pin=JSON.parse(fs.readFileSync(dir+'/backend.json'));
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
fs.mkdirSync(out,{recursive:true});
const evidence={...pin,downloaded:[],shaderFixtures:JSON.parse(fs.readFileSync(dir+'/shader-fixtures.json'))};
evidence.startupDrawFixtures=JSON.parse(fs.readFileSync(dir+'/draw-fixtures.json'));
evidence.computeFixtures=JSON.parse(fs.readFileSync(dir+'/compute-fixtures.json'));const computeSources=fs.readFileSync(dir+'/compute_fixture_sources.inc','utf8').split('\n').filter(l=>l.startsWith('{')).map(l=>JSON.parse('['+l.slice(1,l.lastIndexOf('}'))+']'));for(const r of [...evidence.computeFixtures.originalShaders,...evidence.computeFixtures.diagnosticShaders]){const source=computeSources.find(x=>x[0]===r.hash)?.[1];if(!source||Buffer.byteLength(source)!==r.bytes||hash(Buffer.from(source))!==r.sha256)throw Error('Compute fixture digest mismatch '+r.hash);}fs.copyFileSync(dir+'/compute-fixtures.json','native-build/evidence/compute-fixtures.json');
const startupHeader=fs.readFileSync(dir+'/draw_fixture_sources.inc','utf8');
for(const r of evidence.startupDrawFixtures.records){const m=startupHeader.match(new RegExp('draw_fixture_'+r.stage+'\\[\\]=(.*);'));if(!m)throw Error('Missing original startup shader '+r.stage);const b=Buffer.from(JSON.parse(m[1]));if(b.length!==r.bytes||hash(b)!==r.sourceSHA256)throw Error('Startup shader digest mismatch '+r.stage);}
fs.copyFileSync(dir+'/draw-fixtures.json','native-build/evidence/draw-fixtures.json');
const apiResponse=await fetch('https://raw.githubusercontent.com/gfx-rs/wgpu-native/'+pin.commit+'/src/unimplemented.rs');if(!apiResponse.ok)throw Error('Cannot audit backend API coverage');
const unimplemented=await apiResponse.text();
const calls=[...new Set([...['native_renderer.c','native_gpu.m',...fs.readdirSync(dir).filter(n=>n.endsWith('.inc'))].map(p=>fs.readFileSync(dir+'/'+p,'utf8')).join('\n').matchAll(/\b(wgpu[A-Z]\w*)\s*\(/g)].map(m=>m[1]))].sort();
for(const name of calls)if(new RegExp('fn\\s+'+name+'\\s*\\(').test(unimplemented))throw Error('Backend export is unimplemented: '+name);
evidence.exportCoverage={calls,unimplementedSourceSHA256:hash(unimplemented),result:'No fixture call targets an unimplemented backend export; not a full behavioral coverage claim'};
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
