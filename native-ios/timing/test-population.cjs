'use strict';
const fs=require('fs'),assert=require('assert/strict'),Module=require('module');
process.env.NATIVE_SHADOW_ENGINE=process.env.NATIVE_TIMING_ENGINE;
let code=fs.readFileSync(__dirname+'/../renderer/producer/producer-runtime.cjs','utf8').replace("['testInitMemory', 0, 88],","['testPed',0,41514], ['testTraffic',0,61299], ['testParked',0,61300], ['testAmbient',0,41513], ['testTrafficGet',0,80420], ['testParkedGet',0,80421], ['testInitMemory', 0, 88],");
const mod=new Module(__filename);mod.filename=__filename;mod.paths=module.paths;mod._compile(code,__filename);
mod.exports.createRuntime().then(r=>{
 const e=r.instance.exports,rows=[];
 for(const [vehicle,ped] of [[.6,.7],[.4,.5],[.25,.35]]){
  r.f32(7813544,1);r.f32(7813548,1);r.f32(7155652,1);e.testTraffic(vehicle);e.testParked(vehicle);e.testPed(ped);
  assert(Math.abs(r.readF32(7813544)-vehicle)<1e-6);assert(Math.abs(r.readF32(7155652)-ped)<1e-6);
  e.testTraffic(.1);e.testTraffic(vehicle);e.testPed(.1);e.testPed(ped);
  assert(Math.abs(r.readF32(7813544)-.1)<1e-6);assert(Math.abs(r.readF32(7155652)-.1)<1e-6);
  r.f32(7813528,.8);r.f32(7813532,.5);r.u8(28501281,0);
  assert(Math.abs(e.testTrafficGet(0n,0n,0n)-.08)<1e-6);assert(Math.abs(e.testParkedGet()-.5*vehicle)<1e-6);
  rows.push({vehicle,ped,scriptLowerLimitPreserved:true});
 }
 e.testAmbient(.7);assert(Math.abs(r.readF32(7155636)-.7)<1e-6);
 const result={result:'PASS: original population multiplier contracts',engineSHA256:r.engineHash,rows,semantics:'Per-frame script setters retain the minimum requested multiplier. Random traffic getter multiplies persistent, per-frame and optional override factors. Variety and CityDensity are separate settings.',limitation:'No production injection. Frame reset/order and mission-requested ambient traffic preservation not yet established. These are scalar function tests, not spawned population counts or mission validation.'};
 fs.writeFileSync(process.argv[2],JSON.stringify(result,null,2));console.log(JSON.stringify(result));
}).catch(e=>{console.error(e);process.exitCode=1;});
