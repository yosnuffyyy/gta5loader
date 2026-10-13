import fs from 'node:fs';
import assert from 'node:assert/strict';
const file=process.argv[2]+'/wasm2c/wasm-rt-impl.c';
let code=fs.readFileSync(file,'utf8');
const old='void wasm_rt_trap(wasm_rt_trap_t code) {\n  assert(code != WASM_RT_TRAP_NONE);';
assert.equal(code.split(old).length,2,'Pinned runtime trap entry changed');
// The default hook is inert. A native wait installs a thread-local temporary
// checkpoint so mailbox traps can unlink its stack waiter before propagating.
// Do not replace/copy WABT's opaque outer jmp_buf (including ARM64 PAC state).
code=code.replace(old,`__attribute__((weak)) void native_wait_trap_redirect(wasm_rt_trap_t code) {
  (void)code;
}
void wasm_rt_trap(wasm_rt_trap_t code) {
  assert(code != WASM_RT_TRAP_NONE);
  native_wait_trap_redirect(code);`);
fs.writeFileSync(file,code);
console.log('PASS: pinned runtime supports wait-local trap cleanup without replacing its outer checkpoint.');
