'use strict';
// Execute the pinned original producer functions. Only exports are added to the
// in-memory test module; original function bodies and production files are intact.
const fs = require('node:fs');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const filename=process.env.NATIVE_SHADOW_ENGINE;
if(!filename)throw Error('Set NATIVE_SHADOW_ENGINE to the pinned engine-original.wasm');
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
const extraExports = [
  ['testInitMemory', 0, 88], ['testGetShadowIndex', 0, 44528],
  ['testSetForwardLights', 0, 44278], ['testSetGlobalTexture', 0, 9881],
  ['testStack', 3, 0], ['testThreadTypeAddress', 3, 16], ['testBuildingListAddress', 3, 46],
];
function testModuleBytes() {
  const section = wasm.sections.find(s => s.id === 7);
  const count = readUleb(wasm.b, section.start);
  const payload = Buffer.concat([
    uleb(count.value + extraExports.length), wasm.b.subarray(count.offset, section.end),
    ...extraExports.map(([label, kind, index]) => Buffer.concat([name(label), Buffer.from([kind]), uleb(index)])),
  ]);
  return Buffer.concat([wasm.b.subarray(0, section.offset), Buffer.from([7]), uleb(payload.length), payload, wasm.b.subarray(section.end)]);
}
async function createRuntime() {
  const memory = new WebAssembly.Memory({ initial: 8192, maximum: 65536, shared: true });
  const module = await WebAssembly.compile(testModuleBytes());
  const imports = {}, externalCalls = [];
  for (const item of WebAssembly.Module.imports(module)) {
    imports[item.module] ??= {};
    if (item.kind === 'memory') imports[item.module][item.name] = memory;
    else if (item.kind === 'function') imports[item.module][item.name] = (...args) => {
      externalCalls.push({ module: item.module, name: item.name, args: args.map(String) });
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
if (require.main === module) createRuntime().then(r => {
  console.log(JSON.stringify({ engineHash, initialized: true, externalCalls: r.externalCalls,
    stack: String(r.instance.exports.testStack.value), threadTypeAddress: String(r.instance.exports.testThreadTypeAddress.value),
    buildingListAddress: String(r.instance.exports.testBuildingListAddress.value) }));
}).catch(error => { console.error(error); process.exitCode = 1; });
