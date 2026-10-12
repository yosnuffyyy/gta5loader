import fs from 'node:fs';
import path from 'node:path';
import {spawn,execFileSync} from 'node:child_process';
const root='native-build';
const generated=fs.readdirSync(root+'/generated').filter(n=>/^gta.*\.c$/.test(n)).sort().map(n=>root+'/generated/'+n);
if(generated.length!==128)throw Error('Expected all 128 engine source partitions');
const runtime=['wasm-rt-impl.c','wasm-rt-mem-impl.c','wasm-rt-exceptions-impl.c'].map(n=>root+'/wabt/wasm2c/'+n);
const common=['-std=gnu11','-fno-optimize-sibling-calls','-frounding-math',
 '-DWASM_RT_USE_MMAP=1','-DWASM_RT_MEMCHECK_BOUNDS_CHECK=1','-DWASM_RT_MEMCHECK_GUARD_PAGES=0','-DWASM_RT_SKIP_SIGNAL_RECOVERY=1',
 '-I'+root+'/wabt/wasm2c','-I'+root+'/wabt/third_party/simde','-Inative-ios','-I'+root,'-I'+root+'/generated','-include','native-ios/native_wait.h'];
function run(cmd,args){return new Promise((resolve,reject)=>{const child=spawn(cmd,args,{stdio:'inherit'});child.on('error',reject);child.on('exit',(code,signal)=>code===0?resolve():reject(Error(cmd+' failed '+(signal||code))));});}
for(const target of ['macos','ios']){
 const folder=root+'/'+target;fs.mkdirSync(folder,{recursive:true});
 const sdk=execFileSync('xcrun',['--sdk',target==='ios'?'iphoneos':'macosx','--show-sdk-path'],{encoding:'utf8'}).trim();
 const arch=target==='ios'?['-target','arm64-apple-ios17.0']:[];
 const flags=[...common,'-isysroot',sdk,...arch,target==='ios'?'-O1':'-O0'];
 const sources=[...generated,...runtime,'native-ios/native_wait.c',root+'/generated/probe.c'];
 let next=0,finished=0;
 // Bound concurrent compiler memory; each partition contains hundreds of functions.
 await Promise.all(Array.from({length:2},async()=>{
  for(;;){const index=next++;if(index>=sources.length)return;
   const source=sources[index],object=folder+'/'+path.basename(source)+'.o';
   await run('clang',[...flags,...(target==='macos'?['-DNATIVE_PROBE_CLI']:[]),'-c',source,'-o',object]);
   finished++;if(finished%8===0||finished===sources.length)console.log(target+': compiled '+finished+'/'+sources.length);
  }
 }));
 const objects=sources.map(s=>folder+'/'+path.basename(s)+'.o');
 if(target==='macos'){
  await run('clang',['-isysroot',sdk,...objects,'-o',folder+'/native-probe']);
  const result=execFileSync(folder+'/native-probe',[],{encoding:'utf8',timeout:120000});
  fs.writeFileSync(root+'/evidence/native-engine-test.txt',result);console.log(result);
 }else{
  fs.writeFileSync(folder+'/objects.txt',objects.join('\n')+'\n');
 }
}
