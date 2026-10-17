'use strict';
// Execute the pinned producer functions with added test exports. Optional
// in-memory segment constants / original-availability reference replacements are
// explicit below; the pinned production engine file is never modified.
const fs = require('node:fs');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const filename=process.env.NATIVE_MEMORY_ENGINE;
if(!filename)throw Error('Set NATIVE_MEMORY_ENGINE to the pinned engine-original.wasm');
const wasm={b:fs.readFileSync(filename),sections:[]};
assert.equal(wasm.b.subarray(0,8).toString('hex'),'0061736d01000000');
for(let at=8;at<wasm.b.length;){const offset=at,id=wasm.b[at++],size=readUleb(wasm.b,at);const start=size.offset,end=start+size.value;assert(end<=wasm.b.length);wasm.sections.push({id,offset,start,end});at=end;}
const engineHash = crypto.createHash('sha256').update(wasm.b).digest('hex');
assert.equal(engineHash, 'e65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e');
function uleb(value) {
  const result = [];
  do { const byte = value % 128; value = Math.floor(value / 128); result.push(byte | (value ? 128 : 0)); } while (value);
  return Buffer.from(result);
}
function readUleb(buffer, offset) {
  let value = 0, shift = 0, byte;
  do { byte = buffer[offset++]; assert.notEqual(byte, undefined); value += (byte & 127) * 2 ** shift; shift += 7; } while (byte & 128);
  return { value, offset };
}
function name(value) { const bytes = Buffer.from(value); return Buffer.concat([uleb(bytes.length), bytes]); }
const extraExports = [['testInitMemory',0,88],['testCtor',0,3035],['testAlloc',0,3038],['testFree',0,3039],['testAvailable',0,3042],['testLargest',0,3043],['testChildLargest',0,3021],['testUsed',0,3040],['testHeap',0,3041],['testMalloc',0,91000],['testStack',3,0]];
function testModuleBytes() {
  const section = wasm.sections.find(s => s.id === 7);
  const count = readUleb(wasm.b, section.start);
  const payload = Buffer.concat([
    uleb(count.value + extraExports.length), wasm.b.subarray(count.offset, section.end),
    ...extraExports.map(([label, kind, index]) => Buffer.concat([name(label), Buffer.from([kind]), uleb(index)])),
  ]);
  return Buffer.concat([wasm.b.subarray(0, section.offset), Buffer.from([7]), uleb(payload.length), payload, wasm.b.subarray(section.end)]);
}
async function createRuntime(limit=33554432,restoreAvailable=false) {
  assert(limit>=16777216&&limit<=268435456&&limit%8192===0);
  const memory = new WebAssembly.Memory({ initial: 8192, maximum: 65536, shared: true });
  let moduleBytes=testModuleBytes();
  if(limit!==33554432){const before=Buffer.from('428080809000','hex'),after=Buffer.from([0x42,...Array.from({length:5},(_,i)=>Number((BigInt(limit)>>BigInt(i*7))&127n)|(i<4?128:0))]);let matches=0;for(const at of [2653489,2653499]){assert(wasm.b.subarray(at,at+6).equals(before));}const section=wasm.sections.find(s=>s.id===7);const delta=moduleBytes.length-wasm.b.length;for(const at of [2653489,2653499]){const pos=at>section.end?at+delta:at;after.copy(moduleBytes,pos);matches++;}assert.equal(matches,2);}
  if(restoreAvailable){const reference=JSON.parse(fs.readFileSync(require('node:path').join(__dirname,'available-reference.json'))),body=Buffer.from(reference.hex,'hex');assert.equal(body.length,279);assert.equal(crypto.createHash('sha256').update(body).digest('hex'),reference.bodySHA256);body.copy(moduleBytes,reference.offset+moduleBytes.length-wasm.b.length);}
  const module = await WebAssembly.compile(moduleBytes);
  const imports = {}, externalCalls = [];
  for (const item of WebAssembly.Module.imports(module)) {
    imports[item.module] ??= {};
    if (item.kind === 'memory') imports[item.module][item.name] = memory;
    else if (item.kind === 'function') imports[item.module][item.name] = (...args) => {
      if(externalCalls.length<32)externalCalls.push({ module: item.module, name: item.name, args: args.map(String) });
      if(item.name==='emscripten_resize_heap'){const pages=Math.ceil(Number(args[0])/65536);try{if(pages>memory.buffer.byteLength/65536)memory.grow(pages-memory.buffer.byteLength/65536);return 1;}catch{return 0;}}
      if(item.name==='fd_write'){let v=new DataView(memory.buffer),n=0n;for(let i=0;i<Number(args[2]);i++)n+=v.getBigUint64(Number(args[1])+i*16+8,true);v.setBigUint64(Number(args[3]),n,true);return 0;}
      if(item.name==='emscripten_stack_unwind_buffer')return 0;
      if(item.name==='emscripten_stack_snapshot')return 0n;
      if(item.name==='emscripten_get_now')return performance.now();
      throw new Error('Unexpected external call: ' + item.module + '.' + item.name);
    };
    else throw new Error('Unexpected import kind: ' + item.kind);
  }
  const instance = await WebAssembly.instantiate(module, imports);
  instance.exports.testInitMemory();
  const view = new DataView(memory.buffer);
  const bytes = new Uint8Array(memory.buffer);
  const stack = 96 * 1024 * 1024;
  instance.exports.testStack.value = BigInt(stack);
  const io = {
    u8: (at, value) => view.setUint8(at, value),
    u16: (at, value) => view.setUint16(at, value, true),
    i16: (at, value) => view.setInt16(at, value, true),
    i32: (at, value) => view.setInt32(at, value, true),
    u64: (at, value) => view.setBigUint64(at, BigInt(value), true),
    f32: (at, value) => view.setFloat32(at, value, true),
    readI32: at => view.getInt32(at, true),
    readU64: at => view.getBigUint64(at, true),
    readF32: at => view.getFloat32(at, true),
    clear: (at, length) => bytes.fill(0, at, at + length),
  };
  return { ...io, instance, memory, view, bytes, stack, externalCalls, engineHash };
}
module.exports = { createRuntime, engineHash };
