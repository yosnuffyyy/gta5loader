'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { createRuntime } = require('./producer-runtime.cjs');
(async () => {
  const r = await createRuntime();
  const light = 64 * 1024 * 1024, list = light + 1024;
  const token = 0x123400n;
  const tests = [];
  function check(label, expected, run) {
    const actual = run(); assert.equal(actual, expected, label);
    tests.push({ label, expected, actual });
  }
  function reset() {
    r.clear(light, 2048); r.clear(23076016, 8 * 304 * 2);
    r.clear(23080880, 14976 * 2); r.i32(23074832, 0);
  }
  function get(skipValidation = 0) { return r.instance.exports.testGetShadowIndex(BigInt(light), skipValidation); }
  reset();
  check('null light identity', -1, get);
  r.u64(light + 136, token);
  check('no shadow flags', -1, get);
  for (const bank of [0, 1]) for (let slot = 0; slot < 8; ++slot) {
    reset(); r.i32(23074832, bank); r.u64(light + 136, token); r.i32(light + 100, 256);
    const entry = 23076016 + bank * 2432 + slot * 304;
    r.u64(entry + 272, token);
    check(`active bank ${bank} slot ${slot}`, slot, get);
    r.i16(entry + 284, -1);
    check(`active bank ${bank} slot ${slot}, absent map index`, slot, get);
    r.u8(entry + 281, 8);
    check(`dirty bank ${bank} slot ${slot}`, -1, get);
    r.u8(entry + 281, 0); r.u64(entry + 272, token + 1n);
    check(`different identity bank ${bank} slot ${slot}`, -1, get);
  }
  for (const bank of [0, 1]) for (const index of [0, 1, 7, 43, 51]) {
    reset(); r.i32(23074832, bank); r.u64(light + 136, token); r.i32(light + 100, 128); r.i32(light + 144, index);
    const entry = 23080880 + bank * 14976 + index * 288;
    r.u64(entry + 224, token);
    check(`cached bank ${bank} index ${index}`, index + 8, get);
    r.u64(entry + 224, token + 1n);
    check(`cached identity mismatch bank ${bank} index ${index}`, -1, get);
  }
  reset(); r.u64(light + 136, token); r.i32(light + 100, 128); r.i32(light + 144, -1);
  check('unassigned cache index', -1, get);
  const forwardTests = [];
  function setupForward() {
    reset(); r.clear(r.stack - 2048, 2048);
    r.u8(23110924, 0);
    r.u64(list, light); r.u64(light + 136, token); r.i32(light + 100, 256);
    r.i32(light + 96, 1); r.f32(light + 152, 10);
    r.u64(23076016 + 272, token); r.i32(23076016, 2); r.i16(23076016 + 284, -1);
    r.f32(23076016 + 4, 1); r.f32(23076016 + 8, 10);
    r.u8(19448416, 1); // Settings singleton already constructed for this isolated fixture.
    r.i32(19446824 + 40, 1); r.i32(19446824 + 92, 1);
    r.u64(Number(r.instance.exports.testBuildingListAddress.value), 0);
    r.i32(12400928, 2); r.u64(12432392, 0);
    r.u16(12382544, 20); r.u16(12382544 + 72, 21);
    r.i32(23074656, 1); r.i32(23074688, 2);
    r.u64(12371976, 0x1111); r.u64(23074624, 0x2222);
    r.u64(12432448 + 20 * 8, 0xdead); r.u64(12432448 + 21 * 8, 0xbeef);
  }
  function forward(label, selector, first, second) {
    r.instance.exports.testSetForwardLights(BigInt(list), 1);
    const actual = { selector: r.readF32(r.stack - 996),
      first: Number(r.readU64(12432448 + 20 * 8)), second: Number(r.readU64(12432448 + 21 * 8)) };
    assert.deepEqual(actual, { selector, first, second }, label);
    assert.equal(r.instance.exports.testStack.value, BigInt(r.stack), label + ' stack restoration');
    forwardTests.push({ label, ...actual });
  }
  function mapPair(mapIndex, first, second) {
    const entry = 23059616 + mapIndex * 288;
    const header1 = light + 4096, header2 = header1 + 32, array1 = header2 + 32, array2 = array1 + 32;
    r.u64(header1, array1); r.u16(header1 + 8, 1); r.u64(array1, first);
    r.u64(header2, array2); r.u16(header2 + 8, 1); r.u64(array2, second);
    r.i32(entry + 256, 0); r.u64(entry + 264, header1);
    r.u64(entry + 272, header2);
  }
  setupForward(); forward('active type2 entry, map=-1', 2, 0, 0);
  r.u8(23076016 + 281, 8); forward('same entry becomes dirty', 0, 0x1111, 0x2222);
  r.u8(23076016 + 281, 0); forward('same entry becomes active again', 2, 0, 0);
  for (const bank of [0, 1]) for (const mapIndex of [0, 1, 7, 43, 51]) {
    setupForward(); r.i32(23074832, bank);
    const entry = 23076016 + bank * 2432;
    r.u64(entry + 272, token); r.i32(entry, 2); r.f32(entry + 4, 1); r.f32(entry + 8, 10);
    r.i16(entry + 284, mapIndex); mapPair(mapIndex, 0x3333, 0x4444);
    forward(`active bank ${bank} map ${mapIndex}`, 2, 0x4444, 0x3333);
    // Preserve an explicitly null map pointer: the producer does not invent a cube.
    mapPair(mapIndex, 0, 0x4444);
    forward(`active bank ${bank} map ${mapIndex}, cube pointer null`, 2, 0x4444, 0);
  }
  setupForward(); r.u64(light + 136, 0); forward('no light identity uses defaults', 0, 0x1111, 0x2222);
  setupForward(); r.u8(23110924, 1); forward('shadow bypass flag uses defaults', 0, 0x1111, 0x2222);
  for (let i = 0; i < 128; ++i) {
    setupForward();
    if (i % 3 === 0) r.u8(23076016 + 281, 8);
    else if (i % 3 === 1) { r.i16(23076016 + 284, 0); mapPair(0, 0x3333, 0x4444); }
    forward(`repeated changing light state ${i}`, i % 3 === 0 ? 0 : 2,
      i % 3 === 0 ? 0x1111 : i % 3 === 1 ? 0x4444 : 0,
      i % 3 === 0 ? 0x2222 : i % 3 === 1 ? 0x3333 : 0);
  }
  const output = { result: 'PASS', engineHash: r.engineHash, originalFunctionBodiesUnchanged: true,
    constructedStateTests: tests.length, tests, forwardTests, forwardStateTests: forwardTests.length, externalCalls: r.externalCalls,
    physicalCaptureReproduced: false, productionFixValidated: false };
  fs.writeFileSync(process.env.NATIVE_SHADOW_REPORT || path.join(__dirname, 'producer-test-result.json'), JSON.stringify(output, null, 2));
  console.log(JSON.stringify({ ...output, tests: undefined, forwardTests: undefined }));
})().catch(error => { console.error(error); process.exitCode = 1; });
