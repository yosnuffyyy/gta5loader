const fs=require('fs'),assert=require('assert/strict'),Module=require('module');
process.env.NATIVE_SHADOW_ENGINE=process.env.NATIVE_MODE_ENGINE;
let source=fs.readFileSync(__dirname+'/../renderer/producer/producer-runtime.cjs','utf8')
 .replace("['testInitMemory', 0, 88],", "['testMain',0,103],['testScript',0,49773],['testFindArg',0,2834],['testModeNow',0,566],['testMode',0,568],['testChoice',0,569],['testInitMemory',0,88],")
 .replace('async function createRuntime() {','async function createRuntime(hook) {')
 .replace('externalCalls.push({ module: item.module','if(hook)return hook(item,args,memory); externalCalls.push({ module: item.module');
const m=new Module(__filename);m.filename=__filename;m.paths=module.paths;m._compile(source,__filename);
(async()=>{
 const fixture=process.argv[3]?fs.readFileSync(process.argv[3]):null;if(fixture)assert.equal(fixture.length,12*444);
 const rows=[];let checks=0;
 for(let run=0;run<13;run++){
  const control=run===12,fixtureRun=control?3:run,mode=control?2:[1,2,1,2][Math.floor(run/3)],quality=control?0:run%3,width=[640,480,320][quality],height=[360,270,180][quality];
  const stop=Error('original main input publication boundary');let r,published=0,output='';
  r=await m.exports.createRuntime((item,args,memory)=>{
   const v=new DataView(memory.buffer),bytes=new Uint8Array(memory.buffer);
   if(item.name==='wasm_input_publish_js'){published=Number(args[0]);assert.equal(published,6583436);if(fixture)bytes.set(fixture.subarray(fixtureRun*444,(fixtureRun+1)*444),published);else{v.setUint32(published+280,1,true);v.setUint32(published+432,width,true);v.setUint32(published+436,height,true);Atomics.store(new Int32Array(memory.buffer,published+440,1),0,mode);}throw stop;}
   if(item.name==='fd_write'){let n=0;for(let j=0;j<Number(args[2]);j++){const at=Number(args[1])+j*16,p=Number(v.getBigUint64(at,true)),len=Number(v.getBigUint64(at+8,true));assert(p+len<=bytes.length);output+=Buffer.from(bytes.subarray(p,p+len)).toString();n+=len;}v.setBigUint64(Number(args[3]),BigInt(n),true);return 0;}
   throw Error('Unexpected import '+item.name);
  });
  /* Enter original main with real argv, stop only at host publication. GPU
     construction is outside this fixture: verified disabled before main. */
  r.i32(8348848,-1);const argv=100000,strings=101000,args=['native-probe','-nonetwork',...(mode===1||control?['-noautoload']:[]),...(mode===2?['-playercoords=94.95,-742.96,44.75']:[])];let cursor=strings;
  args.forEach((s,i)=>{r.u64(argv+i*8,cursor);r.bytes.set(Buffer.from(s+'\0'),cursor);cursor+=s.length+1;});r.u64(argv+args.length*8,0);
  try{r.instance.exports.testMain(args.length,BigInt(argv));assert.fail('main did not publish');}catch(e){if(e!==stop)throw e;}
  assert.equal(published,6583436);assert.equal(r.readI32(published+280),1);assert.equal(r.readI32(published+432),width);assert.equal(r.readI32(published+436),height);
  /* The old implementation wrote mode at mouse-focus+280 and then forced
     requested-mode+440 to 1. Reproduce that failure before testing the fix. */
  if(mode===2){r.i32(published+440,1);assert.equal(r.instance.exports.testModeNow(),1);r.i32(published+440,2);}
  assert.equal(r.readI32(6583880),-1,'must not pre-populate cached choice');assert.equal(r.instance.exports.testModeNow(),mode);
  assert.equal(r.instance.exports.testChoice(),mode);assert.equal(r.instance.exports.testMode(),mode);assert(output.includes('[wasm] start mode: '+(mode===2?'sandbox':'story')));
  function scriptFlag(name){const info=110000,result=111000,parameters=112000,text=113000;r.u64(info,result);r.u64(info+16,parameters);r.u64(parameters,text);r.bytes.set(Buffer.from(name+'\0'),text);r.i32(result,-1);r.instance.exports.testScript(BigInt(info));return r.readI32(result);}
  /* Execute original script native, not getters: this is what startup scripts
     see when choosing release Story versus the built-in offline Sandbox. */
  assert.equal(scriptFlag('DoReleaseStartup'),mode===1?1:0);assert.equal(scriptFlag('WasmSandbox'),mode===2?1:0);
  function findArg(name){r.bytes.set(Buffer.from(name+'\0'),114000);r.u64(115000,0);return r.instance.exports.testFindArg(114000n,115000n);}
  assert.equal(findArg('-noautoload'),mode===1||control?1:0);assert.equal(findArg('-playercoords'),mode===2?1:0);
  if(mode===2){const p=Number(r.readU64(115000));assert.equal(Buffer.from(r.bytes.subarray(p,r.bytes.indexOf(0,p))).toString(),'94.95,-742.96,44.75');}
  checks+=13;rows.push({mode,quality,width,height,controlSandboxWithNoautoload:control,block:published,focus:r.readI32(published+280),requested:r.readI32(published+440),cached:r.readI32(6583880),storyScriptFlag:mode===1,sandboxScriptFlag:mode===2,noautoload:args.includes('-noautoload')});
 }
 const result={result:'PASS',checks,rows,origin:fixture?'production C publication bytes → original main → original startup/script decisions':'synthetic publication bytes → original main → original startup/script decisions',engineInstructionsModified:false,worldGameplayVerified:false,limitations:'Main is stopped at publication; original StartChoice and GET_COMMANDLINE_PARAM_EXISTS run afterward. No archives, player/world simulation, audio hardware or full mission scripts execute here.'};
 fs.writeFileSync(process.argv[2],JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
})().catch(e=>{console.error(e);process.exitCode=1;});
