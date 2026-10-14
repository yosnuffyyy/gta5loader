import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {spawn,execFileSync} from 'node:child_process';
const root='native-build';
const generated=fs.readdirSync(root+'/generated').filter(n=>/^gta.*\.c$/.test(n)).sort().map(n=>root+'/generated/'+n);
if(generated.length!==128)throw Error('Expected all 128 engine source partitions');
const runtime=['wasm-rt-impl.c','wasm-rt-mem-impl.c','wasm-rt-exceptions-impl.c'].map(n=>root+'/wabt/wasm2c/'+n);
const common=['-std=gnu11','-Werror=incompatible-pointer-types','-fno-optimize-sibling-calls','-frounding-math',
 '-DWASM_RT_USE_MMAP=1','-DWASM_RT_MEMCHECK_BOUNDS_CHECK=1','-DWASM_RT_MEMCHECK_GUARD_PAGES=0','-DWASM_RT_SKIP_SIGNAL_RECOVERY=1',
 '-I'+root+'/wabt/wasm2c','-I'+root+'/wabt/third_party/simde','-Inative-ios','-I'+root,'-I'+root+'/generated','-include','native-ios/native_wait.h'];
function run(cmd,args){return new Promise((resolve,reject)=>{const child=spawn(cmd,args,{stdio:'inherit'});child.on('error',reject);child.on('exit',(code,signal)=>code===0?resolve():reject(Error(cmd+' failed '+(signal||code))));});}
const hash=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
const dependencies=[];
for(const folder of [root+'/generated',root+'/wabt/wasm2c','native-ios',root]){
 for(const name of fs.readdirSync(folder).filter(n=>/\.(h|inc)$/.test(n)).sort())dependencies.push(folder+'/'+name,hash(fs.readFileSync(folder+'/'+name)));
}
const compiler=execFileSync('clang',['--version'],{encoding:'utf8'});
const dependencyHash=hash(JSON.stringify({dependencies,compiler,simde:'WABT-ff0ef7e0009402740c805a9744c09b05be063e48'}));
const cache=root+'/object-cache';fs.mkdirSync(cache,{recursive:true});
for(const target of ['macos','ios']){
 const folder=root+'/'+target;fs.mkdirSync(folder,{recursive:true});
 const sdk=execFileSync('xcrun',['--sdk',target==='ios'?'iphoneos':'macosx','--show-sdk-path'],{encoding:'utf8'}).trim();
 const arch=target==='ios'?['-target','arm64-apple-ios17.0']:[];
 const flags=[...common,'-isysroot',sdk,...arch,target==='ios'?'-O1':'-O0'];
 const sources=[...generated,...runtime,'native-ios/native_wait.c','native-ios/native_host.c','native-ios/native_runtime.c','native-ios/threading/native_threads.c','native-ios/asset_io/native_asset_index.m','native-ios/asset_io/native_assets.c','native-ios/renderer/gpu_ring.c','native-ios/renderer/native_gpu.m','native-ios/renderer/native_scene.c',root+'/generated/probe.c'];
 let next=0,finished=0,reused=0;
 // Bound concurrent compiler memory; each partition contains hundreds of functions.
 await Promise.all(Array.from({length:2},async()=>{
  for(;;){const index=next++;if(index>=sources.length)return;
   const source=sources[index],object=folder+'/'+path.basename(source)+'.o';
   const args=[...flags,...(source.endsWith('.m')?['-fobjc-arc']:[]),...(source.endsWith('native_gpu.m')?['-I'+root+'/wgpu/'+target+'/include']:[]),...(target==='macos'?['-DNATIVE_PROBE_CLI']:[]),'-c',source,'-o',object];
   const item={dependencyHash,args,sourceHash:hash(fs.readFileSync(source))};
// Extra header applies only to host consumers. Engine cache keys stay unchanged.
if(!generated.includes(source)&&!runtime.includes(source)){
 item.hostHeaders=['native-ios/threading/native_pump.h','native-ios/threading/native_log.h','native-ios/threading/native_threads.h','native-ios/asset_io/native_assets.h','native-ios/platform/native_paths.h','native-ios/renderer/gpu_ring.h','native-ios/renderer/native_gpu.h','native-ios/renderer/native_launch.h',...fs.readdirSync('native-ios/renderer').filter(n=>/\.(h|inc)$/.test(n)).sort().map(n=>'native-ios/renderer/'+n)].map(p=>[p,hash(fs.readFileSync(p))]);
}
const key=hash(JSON.stringify(item));
   const cached=cache+'/'+key+'.o',record=cache+'/'+key+'.sha256';
   if(fs.existsSync(cached)&&fs.existsSync(record)&&hash(fs.readFileSync(cached))===fs.readFileSync(record,'utf8')){fs.copyFileSync(cached,object);reused++;}
   else{await run('clang',args);fs.copyFileSync(object,cached);fs.writeFileSync(record,hash(fs.readFileSync(object)));}
   finished++;if(finished%8===0||finished===sources.length)console.log(target+': compiled '+finished+'/'+sources.length);
  }
 }));
 console.log(target+': reused '+reused+' verified compiler objects');
 const objects=sources.map(s=>folder+'/'+path.basename(s)+'.o');
 if(target==='macos'){
  await run('clang',['-isysroot',sdk,...objects,root+'/wgpu/macos/lib/libwgpu_native.a','-Wl,-dead_strip','-framework','Foundation','-framework','Metal','-framework','QuartzCore','-framework','CoreGraphics','-framework','IOKit','-lc++','-o',folder+'/native-probe']);
  let result='';
  for(let pass=1;pass<=2;pass++){
   const output=execFileSync(folder+'/native-probe',[root+'/fixtures/web',root+'/fixtures/saves'],{encoding:'utf8',timeout:120000});
   if(pass===2&&!output.includes('probe save from the previous run is intact'))throw Error('Persistent save was not verified in the second process');
   result+='PROCESS '+pass+'\n'+output;console.log(output);
  }
  fs.writeFileSync(root+'/evidence/native-engine-test.txt',result);
 }else{
  fs.writeFileSync(folder+'/objects.txt',objects.join('\n')+'\n');
 }
}
