'use strict';
// Original engine main initializes and publishes its ring. Stop at this exact
// host boundary, before archive loading. No source/decoded audio is invented.
const fs=require('fs'),assert=require('assert/strict');
let code=fs.readFileSync(__dirname+'/../renderer/producer/producer-runtime.cjs','utf8')
.replace("['testInitMemory', 0, 88],","['testMain',0,103], ['testInitMemory', 0, 88],")
.replace('externalCalls.push({ module:',"if(item.name==='wasm_input_publish_js')return; externalCalls.push({ module:");
const Module=require('module'),mod=new Module(__filename);mod.filename=__filename;mod.paths=module.paths;mod._compile(code,__filename);
mod.exports.createRuntime().then(r=>{
 r.i32(8348848,-1); // Reference main's non-graphics diagnostic mode.
 assert.throws(()=>r.instance.exports.testMain(0,0n),/Unexpected external call: env.wasm_audio_publish_js/);
 assert.equal(r.externalCalls.length,1);assert.deepEqual(r.externalCalls[0].args,['8153472','16384']);
 const at=8153472,capacity=r.readI32(at+8);assert.equal(capacity,16384);assert(at+64+capacity*8<=r.bytes.length);
 assert.equal(r.readI32(at),0);assert.equal(r.readI32(at+4),0);
 const report={result:'PASS',engineSHA256:r.engineHash,originalMain:103,ring:at,capacity,channels:2,frameBytes:8,headerBytes:64,scope:'Original engine publish boundary and initialized header; not bank decoding or physical audio.'};
 fs.writeFileSync(process.argv[2],JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
}).catch(e=>{console.error(e);process.exitCode=1;});
