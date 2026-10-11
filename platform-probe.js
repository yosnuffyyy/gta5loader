// One-page allocation with this converted engine's shared i32 type and 4 GiB maximum.
// Import validation verifies that shared wasm32 memory can be linked.
export const MEMORY32_PROBE=new Uint8Array([
  0,97,115,109,1,0,0,0,2,18,1,3,101,110,118,6,109,101,109,111,114,121,2,3,1,128,128,4
]);
export function probeMemory32() {
  const module=new WebAssembly.Module(MEMORY32_PROBE);
  const memory=new WebAssembly.Memory({initial:1,maximum:65536,shared:true});
  new WebAssembly.Instance(module,{env:{memory}});
  return memory.buffer instanceof SharedArrayBuffer;
}
export async function probePlatform({gpu=true}={}) {
  const failures=[];
  try {if(!probeMemory32())throw Error();}catch{failures.push('Shared 32-bit WebAssembly memory is unavailable. Close all setup tabs and reopen the Home Screen app while online, then check again.');}
  if(gpu) {
    let device;
    try {
      if(!globalThis.navigator?.gpu)throw Error('WebGPU is unavailable in browser workers.');
      if(!new OffscreenCanvas(1,1).getContext('webgpu'))throw Error('Offscreen WebGPU rendering is unavailable.');
      const adapter=await navigator.gpu.requestAdapter({powerPreference:'high-performance'});
      if(!adapter)throw Error('No compatible WebGPU graphics adapter was found.');
      if(!adapter.features.has('texture-compression-bc'))throw Error('The GPU/browser lacks BC compressed textures required by this game.');
      device=await adapter.requestDevice({requiredFeatures:['texture-compression-bc']});
      device.pushErrorScope('validation');
      const texture=device.createTexture({size:[4,4],format:'bc1-rgba-unorm',usage:GPUTextureUsage.TEXTURE_BINDING});
      const error=await device.popErrorScope();texture.destroy();
      if(error)throw Error('The browser could not create the game’s BC texture format.');
    }catch(e){failures.push(e.message || 'The WebGPU check failed.');}
    finally{device?.destroy();}
  }
  return {ok:failures.length===0,failures};
}
if(typeof WorkerGlobalScope!=='undefined' && globalThis instanceof WorkerGlobalScope) {
  globalThis.onmessage=async e=>postMessage(await probePlatform(e.data));
}
