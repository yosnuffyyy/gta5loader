'use strict';
// Execute the pinned original fwTimer::Update. This diagnoses the existing
// clamp; it does not certify a replacement simulation scheduler.
const fs=require('fs'),assert=require('assert/strict');
process.env.NATIVE_SHADOW_ENGINE=process.env.NATIVE_TIMING_ENGINE;
let code=fs.readFileSync(__dirname+'/../renderer/producer/producer-runtime.cjs','utf8')
.replace("['testInitMemory', 0, 88],","['testTimer',0,8924], ['testTicks',0,2809], ['testInitMemory', 0, 88],")
.replace('const imports = {}, externalCalls = [];','let clock=0; const imports = {}, externalCalls = [];')
.replace('externalCalls.push({ module:',"if(item.name==='wasm_now_ms')return clock; externalCalls.push({ module:")
.replace('return { ...io,','return {setClock:t=>clock=t, ...io,');
const Module=require('module'),mod=new Module(__filename);mod.filename=__filename;mod.paths=module.paths;mod._compile(code,__filename);
mod.exports.createRuntime().then(r=>{
 const min=r.readF32(6632100),max=r.readF32(6632104);assert(Math.abs(min-1/30)<1e-7&&Math.abs(max-1/15)<1e-7);
 // GetTicks converts monotonic milliseconds to nanoseconds. Timer's platform
 // conversion scalar is initialized by the native engine constructor to 1e-9.
 r.f32(9779848,1e-9);let clock=1000;r.setClock(clock);r.instance.exports.testTimer();const rows=[];
 for(const fps of [5,10,15,30]){let before=r.readI32(12321248),sum=0;for(let i=0;i<fps*10;i++){clock+=1000/fps;r.setClock(clock);r.instance.exports.testTimer();sum+=r.readF32(12321260);}const expected=10*Math.min(1,fps/15);assert(Math.abs(sum-expected)<1e-4);rows.push({renderCadenceFPS:fps,realSeconds:10,engineDeltaSum:sum,engineIntegerMilliseconds:r.readI32(12321248)-before});}
 // Script-pause flag: timer is still called, but gameplay time must stop.
 r.u8(12321461,1);let paused=r.readI32(12321248);for(let i=0;i<30;i++){clock+=1000/30;r.setClock(clock);r.instance.exports.testTimer();}assert.equal(r.readI32(12321248),paused);
 r.u8(12321461,0);clock+=1000/30;r.setClock(clock);r.instance.exports.testTimer();assert(r.readI32(12321248)>paused);
 const result={result:'PASS: original timer behavior reproduced',engineSHA256:r.engineHash,sourceFunction:8924,minimumDelta:min,maximumDelta:max,rows,pauseResume:'PASS',limitation:'Not a slow-motion fix. Original 66.7-ms clamp loses elapsed simulation time below 15 updates/s. Arbitrary clamp removal or extra physics ticks are not validated; animation/cutscene-specific clocks require device traces.'};
 fs.writeFileSync(process.argv[2],JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
}).catch(e=>{console.error(e);process.exitCode=1;});
