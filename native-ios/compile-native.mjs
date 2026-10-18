import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {spawn,execFile,execFileSync} from 'node:child_process';
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
dependencies.push('native-ios/memory/native_memory.h',hash(fs.readFileSync('native-ios/memory/native_memory.h')));
const compiler=execFileSync('clang',['--version'],{encoding:'utf8'});
const dependencyHash=hash(JSON.stringify({dependencies,compiler,simde:'WABT-ff0ef7e0009402740c805a9744c09b05be063e48'}));
const cache=root+'/object-cache';fs.mkdirSync(cache,{recursive:true});
for(const target of ['macos','ios']){
 const folder=root+'/'+target;fs.mkdirSync(folder,{recursive:true});
 const sdk=execFileSync('xcrun',['--sdk',target==='ios'?'iphoneos':'macosx','--show-sdk-path'],{encoding:'utf8'}).trim();
 const arch=target==='ios'?['-target','arm64-apple-ios17.0']:[];
 const flags=[...common,'-isysroot',sdk,...arch,target==='ios'?'-O1':'-O0'];
 // Fail a native-adapter interface error before compiling 128 engine partitions.
 await run('clang',[...flags,'-fobjc-arc','-I'+root+'/wgpu/'+target+'/include',...(target==='macos'?['-DNATIVE_PROBE_CLI']:[]),'-fsyntax-only','native-ios/renderer/native_gpu.m']);
 const sources=[...generated,...runtime,'native-ios/audio/native_audio.m','native-ios/native_wait.c','native-ios/native_host.c','native-ios/diagnostics/native_journal.m','native-ios/diagnostics/native_profile.c','native-ios/native_runtime.c','native-ios/threading/native_threads.c','native-ios/input/native_input.c','native-ios/asset_io/native_asset_index.m','native-ios/asset_io/native_assets.c','native-ios/renderer/gpu_ring.c','native-ios/renderer/native_gpu.m','native-ios/renderer/native_scene.c',root+'/generated/probe.c'];
 let next=0,finished=0,reused=0;
 // Bound concurrent compiler memory; each partition contains hundreds of functions.
 await Promise.all(Array.from({length:2},async()=>{
  for(;;){const index=next++;if(index>=sources.length)return;
   const source=sources[index],object=folder+'/'+path.basename(source)+'.o';
   const args=[...flags,...(source.endsWith('.m')?['-fobjc-arc']:[]),...(source.endsWith('native_gpu.m')?['-I'+root+'/wgpu/'+target+'/include']:[]),...(target==='macos'?['-DNATIVE_PROBE_CLI']:[]),'-c',source,'-o',object];
   const item={dependencyHash,args,sourceHash:hash(fs.readFileSync(source))};
   if(generated.includes(source)&&fs.readFileSync(source,'utf8').includes('audio/native_mixer_profile.h'))item.mixerHeader=hash(fs.readFileSync('native-ios/audio/native_mixer_profile.h')); 
// Extra header applies only to host consumers. Engine cache keys stay unchanged.
if(!generated.includes(source)&&!runtime.includes(source)){
 item.hostHeaders=['native-ios/timing/native_time_trace.h','native-ios/audio/native_audio.h','native-ios/audio/native_pcm.h','native-ios/diagnostics/native_journal.h','native-ios/diagnostics/native_recovery.h','native-ios/diagnostics/native_profile.h',...fs.readdirSync('native-ios/memory').filter(n=>/\.(h|inc)$/.test(n)).sort().map(n=>'native-ios/memory/'+n),'native-ios/input/native_input.h','native-ios/threading/native_session.h','native-ios/threading/native_session_bridge.inc','native-ios/threading/native_pump.h','native-ios/threading/native_log.h','native-ios/threading/native_threads.h','native-ios/asset_io/native_assets.h','native-ios/platform/native_paths.h','native-ios/renderer/gpu_ring.h','native-ios/renderer/native_gpu.h','native-ios/renderer/native_launch.h',...fs.readdirSync('native-ios/renderer').filter(n=>/\.(h|inc)$/.test(n)).sort().map(n=>'native-ios/renderer/'+n)].map(p=>[p,hash(fs.readFileSync(p))]);
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
  await run('clang',['-isysroot',sdk,...objects,root+'/wgpu/macos/lib/libwgpu_native.a','-Wl,-dead_strip','-framework','Foundation','-framework','AVFoundation','-framework','AudioToolbox','-framework','Metal','-framework','QuartzCore','-framework','CoreGraphics','-framework','IOKit','-lc++','-o',folder+'/native-probe']);
  let result='';
  for(let pass=1;pass<=2;pass++){
   let output;try{output=execFileSync(folder+'/native-probe',[root+'/fixtures/web',root+'/fixtures/saves'],{encoding:'utf8',timeout:240000,env:{...process.env,NATIVE_RENDERER_V2_TEST:pass===2?'1':'0'}});}catch(error){const stdout=String(error.stdout||'');fs.writeFileSync(root+'/evidence/native-engine-test-failed.txt',stdout);fs.writeFileSync(root+'/evidence/native-engine-test-failed-stderr.txt',String(error.stderr||''));console.error(stdout.slice(-8192));throw Error('Native regression process '+pass+' failed: status='+error.status+' signal='+error.signal+' code='+error.code+'; full output preserved in evidence');}
   if(pass===2&&!output.includes('probe save from the previous run is intact'))throw Error('Persistent save was not verified in the second process');
   result+='PROCESS '+pass+' EXECUTION '+(pass===2?'V2':'EXISTING')+'\n'+output;console.log(output);
  }
  fs.writeFileSync(root+'/evidence/native-engine-test.txt',result);
  execFileSync(process.execPath,['evidence/renderer-v2/compare.mjs',root+'/evidence/native-engine-test.txt',root+'/evidence/renderer-v2-benchmark.json'],{stdio:'inherit'});
  // Independent compiler processes, bounded to the runner's three cores.
  // Preserve every stdout/stderr even on failure or a compiler timeout.
  const audits=await Promise.allSettled([0,1,2].map(shard=>new Promise((resolve,reject)=>{
   execFile(folder+'/native-probe',['--shader-audit-'+shard,root+'/shader-audit'],{encoding:'utf8',timeout:1200000,maxBuffer:16*1024*1024},(error,stdout,stderr)=>{
    fs.writeFileSync(root+'/evidence/native-shader-audit-'+shard+'.txt',stdout||'');fs.writeFileSync(root+'/evidence/native-shader-audit-'+shard+'-stderr.txt',stderr||'');
    if(error){console.error('Shader audit shard '+shard+' failed; complete diagnostics saved.\n'+String(stdout).slice(-8192)+String(stderr).slice(-2048));reject(error);}else resolve(stdout);
   });
  })));
  if(audits.some(x=>x.status!=='fulfilled'))throw Error('Native shader audit failed; inspect preserved shard evidence');
  const audit=audits.map(x=>x.value).join('\n');if(!audit.includes('PASS: recorded Prologue VS 7db515e410659eca + PS 394c572f727d25e8'))throw Error('Recorded Prologue shader-pair audit missing');const totals={modules:0,pipelines:0,vs:0,ps:0,cs:0,clip:0,failures:0,untranslated:0};
  const matches=[...audit.matchAll(/PASS: original shader audit shard (\d)\/3; modules (\d+); native Metal pipelines (\d+); VS\/PS\/CS (\d+)\/(\d+)\/(\d+); clip shaders (\d+); failures (\d+); untranslated records (\d+)/g)];
  if(matches.length!==3||new Set(matches.map(x=>x[1])).size!==3)throw Error('Incomplete shader audit shard reports');
  for(const m of matches)for(const [i,key] of ['modules','pipelines','vs','ps','cs','clip','failures','untranslated'].entries())totals[key]+=Number(m[i+2]);
  if(totals.modules!==4808||totals.pipelines!==4808||totals.vs!==1112||totals.ps!==3667||totals.cs!==29||totals.clip!==500||totals.failures!==0||totals.untranslated!==111)throw Error('Shader audit coverage mismatch '+JSON.stringify(totals));
  fs.writeFileSync(root+'/evidence/native-shader-audit-test.txt',audit);fs.writeFileSync(root+'/evidence/native-shader-audit-summary.json',JSON.stringify({result:'PASS',...totals,shards:3,maximumConcurrentProgramsPerProcess:1,gameSceneVerified:false},null,2)+'\n');console.log('PASS: complete native Metal shader audit '+JSON.stringify(totals));
 }else{
  fs.writeFileSync(folder+'/objects.txt',objects.join('\n')+'\n');
 }
}
