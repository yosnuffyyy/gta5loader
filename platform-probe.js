// One-page allocation with the engine's shared i64 memory type and 16 GiB maximum.
// Import validation distinguishes real memory64 support from an ignored option.
export const MEMORY64_PROBE=new Uint8Array([
  0,97,115,109,1,0,0,0,2,18,1,3,101,110,118,6,109,101,109,111,114,121,2,7,1,128,128,16
]);
export function probeMemory64() {
  const module=new WebAssembly.Module(MEMORY64_PROBE);
  const memory=new WebAssembly.Memory({initial:1n,maximum:262144n,shared:true,address:'i64'});
  new WebAssembly.Instance(module,{env:{memory}});
  return memory.buffer instanceof SharedArrayBuffer;
}
export async function probePlatform({gpu=true}={}) {
  const failures=[];
  try {if(!probeMemory64())throw Error();}catch{failures.push('Shared 64-bit WebAssembly memory is unavailable. Update your browser/OS. Touch controls cannot replace this engine requirement.');}
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
