const fs=require('fs'),crypto=require('crypto'),assert=require('assert/strict'),Module=require('module');
const contract=require('./settings-contract.json');
const observed=fs.readFileSync(__dirname+'/effective_settings.h','utf8');for(const a of contract.parameters)assert(observed.includes(`{"${a.name}",${a.destination}u,${a.kind==='f32.store'},${a.kind==='i32.store8'?1:4}}`),a.name+' runtime readback address');
const engine=process.env.NATIVE_MODE_ENGINE;if(!engine)throw Error('NATIVE_MODE_ENGINE required');process.env.NATIVE_SHADOW_ENGINE=engine;
let source=fs.readFileSync(__dirname+'/../renderer/producer/producer-runtime.cjs','utf8').replace("['testInitMemory', 0, 88],","['testLock',0,10041],['testMain',0,103],['testFind',0,2834],['testCtor',0,2835],['testInitMemory',0,88],").replace('async function createRuntime() {','async function createRuntime(hook) {').replace('externalCalls.push({ module: item.module','if(hook)return hook(item,args,memory);externalCalls.push({ module: item.module');
const extra=`
function augment(input){
 let entries=[];for(let at=8;at<input.length;){let start=at,id=input[at++],n=readUleb(input,at);at=n.offset+n.value;entries.push({start,id,body:n.offset,end:at});}
 const code=entries.find(x=>x.id===10),f=entries.find(x=>x.id===3),ex=entries.find(x=>x.id===7);let n=readUleb(input,code.body),p=n.offset,locals;
 for(let i=0;i<n.value;i++){let l=readUleb(input,p);if(i+85===29099){let d=readUleb(input,l.offset),q=d.offset;for(let j=0;j<d.value;j++)q=readUleb(input,q).offset+1;locals=input.subarray(l.offset,q);}p=l.offset+l.value;}
 let fc=readUleb(input,f.body),q=fc.offset,type;for(let i=0;i<fc.value;i++){let x=readUleb(input,q);if(i+85===29099)type=x.value;q=x.offset;}
 const contract=JSON.parse(require('fs').readFileSync(${JSON.stringify(__dirname+'/settings-contract.json')},'utf8'));
 const chunks=contract.parameters.map(a=>{const part=wasm.b.subarray(a.offset,a.offset+a.length);if(require('crypto').createHash('sha256').update(part).digest('hex')!==a.sha256)throw Error(a.name+' bytecode changed');return part;});
 const body=Buffer.concat([locals,...chunks,Buffer.from([65,0,11])]),functionIndex=fc.value+85,ec=readUleb(input,ex.body);
 return Buffer.concat([input.subarray(0,8),...entries.map(e=>{let payload;if(e.id===3)payload=Buffer.concat([uleb(fc.value+1),input.subarray(fc.offset,e.end),uleb(type)]);else if(e.id===10)payload=Buffer.concat([uleb(n.value+1),input.subarray(n.offset,e.end),uleb(body.length),body]);else if(e.id===7)payload=Buffer.concat([uleb(ec.value+1),input.subarray(ec.offset,e.end),name('testSettings'),Buffer.from([0]),uleb(functionIndex)]);else return input.subarray(e.start,e.end);return Buffer.concat([Buffer.from([e.id]),uleb(payload.length),payload]);})]);
}
`;
source=source.replace('async function createRuntime(hook) {',extra+'\nasync function createRuntime(hook) {').replace('WebAssembly.compile(testModuleBytes())','WebAssembly.compile(augment(testModuleBytes()))');
const mod=new Module(__filename);mod.filename=__filename;mod.paths=module.paths;mod._compile(source,__filename);
(async()=>{
 const stop=Error('publication'),r=await mod.exports.createRuntime((item,args,memory)=>{if(item.name==='wasm_input_publish_js')throw stop;throw Error('Unexpected '+item.name);});
 const fixtures=JSON.parse(fs.readFileSync(process.argv[2],'utf8')),rows=[];let checks=0;
 for(const fixture of fixtures){
  let cursor=101000;const args=['native-probe',...fixture.args];args.forEach((s,i)=>{r.u64(100000+i*8,cursor);r.bytes.set(Buffer.from(s+'\0'),cursor);cursor+=s.length+1;});r.u64(100000+args.length*8,0);r.i32(8348848,-1);
  try{r.instance.exports.testMain(args.length,100000n)}catch(e){if(e!==stop)throw e;}
  const all=[...contract.parameters,{name:'frameLimit',param:12619080,namePointer:323958,helpPointer:1100029}];
  // Original main establishes argv. Bind original FindCommandLineArg's exact
  // returned value pointer to the constructed sysParam, as startup does. This
  // isolates consumers; it does not pretend to execute the whole CApp/world.
  for(const a of all){r.instance.exports.testCtor(BigInt(a.param),BigInt(a.namePointer),BigInt(a.helpPointer));r.bytes.set(Buffer.from('-'+a.name+'\0'),115000);r.u64(116000,0);assert.equal(r.instance.exports.testFind(115000n,116000n),1,a.name);r.u64(a.param+16,r.readU64(116000));checks++;}
  r.bytes.fill(0xa5,19446824,19447000);r.instance.exports.testSettings(120000n,0,0,0);r.instance.exports.testLock(99,0);
  const effective={};for(const a of contract.parameters){const requested=Number(args.find(x=>x.startsWith('-'+a.name+'=')).split('=')[1]);let expected=a.name==='shadowQuality'?requested<0?0:requested+1:requested;
   const value=a.kind==='f32.store'?r.readF32(a.destination):a.kind==='i32.store8'?r.bytes[a.destination]:r.readI32(a.destination);assert(Math.abs(value-expected)<1e-6,`${a.name}: requested ${requested}, expected ${expected}, actual ${value}`);effective[a.name]=value;checks++;
  }
  const divisor=Number(args.find(x=>x.startsWith('-frameLimit=')).split('=')[1]);assert.equal(r.readI32(12622836),divisor);checks++;
  rows.push({mode:fixture.mode,preset:fixture.preset,width:fixture.width,height:fixture.height,divisor,effective});
 }
 const result={result:'PASS',checks,engineSHA256:r.engineHash,rows,scope:'Production C settings argv → original main/FindCommandLineArg → original sysParam numeric readers → hash-verified unmodified startup override instruction slices and SetFrameLock. Test-only sysParam binding. No engine instruction changes in production.',limitations:'Not complete world startup, physical MSAA/FXAA pixels, GPU memory budgeting or achieved FPS. Higher caps unavailable; original timer unchanged.'};
 fs.writeFileSync(process.argv[3],JSON.stringify(result,null,2)+'\n');console.log(`PASS: ${checks} original-engine settings checks across ${rows.length} C-generated launch configurations.`);
})().catch(e=>{console.error(e);process.exitCode=1});
