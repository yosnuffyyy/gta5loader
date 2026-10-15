window.runDone=(async()=>{
 const errors=[],resources=[],cases=[],invalid=[];
 if(!navigator.gpu)return {result:'UNAVAILABLE',reason:'WebGPU is unavailable'};
 const adapter=await navigator.gpu.requestAdapter({powerPreference:'high-performance'});
 if(!adapter)return {result:'UNAVAILABLE',reason:'No WebGPU adapter'};
 const info=adapter.info;
 const adapterInfo={vendor:info?.vendor,architecture:info?.architecture,device:info?.device,description:info?.description,isFallbackAdapter:info?.isFallbackAdapter};
 if(adapterInfo.isFallbackAdapter||/swiftshader|software|warp/i.test(adapterInfo.description||''))return {result:'UNAVAILABLE',reason:'Only software adapter available',adapter:adapterInfo};
 if(!adapter.features.has('texture-compression-bc'))return {result:'UNAVAILABLE',reason:'Captured BC1 resource requires BC feature',adapter:adapterInfo};
 const device=await adapter.requestDevice({requiredFeatures:['texture-compression-bc']});
 device.addEventListener('uncapturederror',e=>errors.push(e.error.message));
 const check=(condition,why)=>{if(!condition)throw Error(why);};
 const texture=d=>{const t=device.createTexture(d);resources.push(t);return t;};
 const buffer=d=>{const b=device.createBuffer(d);resources.push(b);return b;};
 let draws=0,pixels=0,positiveInvalidChecks=0;
 try{
  const input=await (await fetch('/fixture-shader.json')).json();
  const original=device.createShaderModule({label:'original PS 996bb42f448956f2 compilation only',code:input.original});
  const originalErrors=(await original.getCompilationInfo()).messages.filter(x=>x.type==='error');
  check(!originalErrors.length,'Original PS compilation failed: '+originalErrors.map(x=>x.message).join('; '));
  const fragment=device.createShaderModule({label:'unchanged original shadow branch fixture',code:input.source});
  const compile=(await fragment.getCompilationInfo()).messages.filter(x=>x.type==='error');check(!compile.length,'Fixture WGSL: '+compile.map(x=>x.message));
  const vertex=device.createShaderModule({code:'struct Out { @builtin(position) p:vec4f,@location(1) a:vec4f,@location(2) b:vec4f, } @vertex fn main(@builtin(vertex_index) i:u32)->Out {var q=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));return Out(vec4f(q[i],0,1),vec4f(1),vec4f(0));}'});
  const pipeline=await device.createRenderPipelineAsync({layout:'auto',vertex:{module:vertex},fragment:{module:fragment,targets:[{format:'rgba8unorm'}]}});
  const cb3=buffer({size:48,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});
  const cb6=buffer({size:496,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});
  const output=texture({size:[16,16],format:'rgba8unorm',usage:GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.COPY_SRC});
  const stage=buffer({size:4096,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ});
  function depthCube(depths,initialize=true){
   const t=texture({size:[4,4,6],format:'depth32float',usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.RENDER_ATTACHMENT});
   if(initialize){const encoder=device.createCommandEncoder();for(let i=0;i<6;i++){const pass=encoder.beginRenderPass({colorAttachments:[],depthStencilAttachment:{view:t.createView({dimension:'2d',baseArrayLayer:i,arrayLayerCount:1}),depthClearValue:Array.isArray(depths)?depths[i]:depths,depthLoadOp:'clear',depthStoreOp:'store'}});pass.end();}device.queue.submit([encoder.finish()]);}
   return t;
  }
  const cubes=[0,.25,.75,1].map(z=>depthCube(z));
  const empty=depthCube(0,false),sixFaces=depthCube([.25,.75,.25,.75,.25,.75]);
  const spot=texture({size:[4,4],format:'depth32float',usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.RENDER_ATTACHMENT});
  {const encoder=device.createCommandEncoder(),pass=encoder.beginRenderPass({colorAttachments:[],depthStencilAttachment:{view:spot.createView(),depthClearValue:.75,depthLoadOp:'clear',depthStoreOp:'store'}});pass.end();device.queue.submit([encoder.finish()]);}
  const samplers=new Map();
  const funcs=['never','less','equal','less-equal','greater','not-equal','greater-equal','always'];
  const compare=(f,r,z)=>({never:false,less:r<z,equal:r===z,'less-equal':r<=z,greater:r>z,'not-equal':r!==z,'greater-equal':r>=z,always:true})[f]?255:0;
  function sampler(compare,filter){const key=compare+'/'+filter;if(!samplers.has(key))samplers.set(key,device.createSampler({compare,magFilter:filter,minFilter:filter}));return samplers.get(key);}
  const directions=[[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],[0,0,1],[0,0,-1]];
  async function draw({cube,compare:f,ref=.5,type=2,count=1,face=4,filter='nearest',expected,label}){
   const values=new Float32Array(124);values[60]=values[65]=values[70]=1;values[63]=type;values[71]=ref;
   values.set(directions[face].map(x=>-x),72);
   const counts=new Uint32Array(12);counts[8]=count;
   device.queue.writeBuffer(cb3,0,counts);device.queue.writeBuffer(cb6,0,values);
   const group=device.createBindGroup({layout:pipeline.getBindGroupLayout(1),entries:[{binding:3,resource:{buffer:cb3}},{binding:6,resource:{buffer:cb6}},{binding:30,resource:sampler(f,filter)},{binding:46,resource:cube.createView({dimension:'cube',arrayLayerCount:6})},{binding:56,resource:spot.createView()}]});
   const encoder=device.createCommandEncoder();const pass=encoder.beginRenderPass({colorAttachments:[{view:output.createView(),clearValue:[.2,.3,.4,1],loadOp:'clear',storeOp:'store'}]});
   pass.setPipeline(pipeline);pass.setBindGroup(1,group);pass.draw(3);pass.end();
   encoder.copyTextureToBuffer({texture:output},{buffer:stage,bytesPerRow:256},[16,16]);device.queue.submit([encoder.finish()]);
   await stage.mapAsync(GPUMapMode.READ);const bytes=new Uint8Array(stage.getMappedRange());
   for(let y=0;y<16;y++)for(let x=0;x<16;x++)for(let channel=0;channel<4;channel++)check(bytes[y*256+x*4+channel]===(channel===3?255:expected),label+' pixel mismatch at '+x+','+y+' channel '+channel+': '+bytes[y*256+x*4+channel]+' expected '+expected);
   stage.unmap();draws++;pixels+=256;
  }
  const refs=[-.5,0,.125,.25,.5,.75,1,1.5];
  for(const filter of ['nearest','linear'])for(const f of funcs)for(let z=0;z<4;z++)for(const ref of refs)await draw({cube:cubes[z],compare:f,filter,ref,expected:compare(f,ref,[0,.25,.75,1][z]),label:'comparison matrix '+f+'/'+filter+'/'+z+'/'+ref});
  cases.push('512 actual-branch comparison draws: all eight functions, four depths, eight references, nearest/linear');
  for(let face=0;face<6;face++){
   await draw({cube:sixFaces,compare:'less-equal',face,expected:face%2?255:0,label:'normal shadow face '+face});
   await draw({cube:empty,compare:'greater-equal',ref:0,face,expected:255,label:'empty cube GE zero boundary '+face});
   await draw({cube:cubes[3],compare:'less-equal',ref:1,face,expected:255,label:'clear-one LE one boundary '+face});
   await draw({cube:empty,compare:'less-equal',face,type:0,expected:255,label:'explicit no-shadow branch '+face});
   await draw({cube:empty,compare:'less-equal',face,expected:0,label:'empty cube is not neutral for LE '+face});
   await draw({cube:empty,compare:'greater-equal',ref:1.5,face,expected:255,label:'GE nonnegative reference outside unit range '+face});
   await draw({cube:cubes[3],compare:'less-equal',ref:1.5,face,expected:0,label:'clear-one not universally neutral '+face});
  }
  cases.push('42 six-face normal/no-shadow/neutral-boundary/counterexample draws');
  for(let i=0;i<12;i++)await draw({cube:i%3===0?sixFaces:i%3===1?empty:cubes[3],compare:i%3===1?'greater-equal':'less-equal',face:4,expected:i%3===0?0:255,label:'alternating bindings '+i});
  cases.push('12 alternating normal/zero-neutral/one-neutral cube draws');
  const captured=texture({label:'captured source 708 single-layer BC1',size:[4,4,1],mipLevelCount:1,format:'bc1-rgba-unorm',usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_DST});
  device.queue.writeTexture({texture:captured},new Uint8Array(8),{bytesPerRow:8,rowsPerImage:4},[4,4,1]);
  async function expectedError(label,fn){device.pushErrorScope('validation');fn();const error=await device.popErrorScope();check(error,label+' unexpectedly accepted');invalid.push({label,message:error.message});positiveInvalidChecks++;}
  await expectedError('captured 2D view cannot be a six-face cube',()=>captured.createView({dimension:'cube',arrayLayerCount:6}));
  await expectedError('captured 2D BC1 cannot satisfy cube depth layout',()=>device.createBindGroup({layout:pipeline.getBindGroupLayout(1),entries:[{binding:3,resource:{buffer:cb3}},{binding:6,resource:{buffer:cb6}},{binding:30,resource:sampler('less-equal','nearest')},{binding:46,resource:captured.createView()},{binding:56,resource:spot.createView()}]}));
  await draw({cube:sixFaces,compare:'less-equal',face:5,expected:255,label:'valid state survives rejected invalid binding'});
  let temporary=depthCube(0);
  await draw({cube:temporary,compare:'greater-equal',expected:255,label:'new neutral cube'});
  function clearAll(t,depth){const encoder=device.createCommandEncoder();for(let face=0;face<6;face++){const pass=encoder.beginRenderPass({colorAttachments:[],depthStencilAttachment:{view:t.createView({dimension:'2d',baseArrayLayer:face,arrayLayerCount:1}),depthClearValue:depth,depthLoadOp:'clear',depthStoreOp:'store'}});pass.end();}device.queue.submit([encoder.finish()]);}
  clearAll(temporary,1);
  await draw({cube:temporary,compare:'greater-equal',expected:0,label:'same cube queued clear changes comparison'});
  clearAll(temporary,0);
  await draw({cube:temporary,compare:'greater-equal',expected:255,label:'same cube queued restore changes comparison'});
  temporary.destroy();temporary=depthCube(1);
  await draw({cube:temporary,compare:'less-equal',expected:255,label:'replacement neutral cube'});
  await draw({cube:sixFaces,compare:'less-equal',face:4,expected:0,label:'normal shadow survives neutral replacement'});
  cases.push('five queue-ordered clear/reuse/replacement draws; normal shadow remains effective');
  const oneDepth=texture({size:[4,4,1],format:'depth32float',usage:GPUTextureUsage.TEXTURE_BINDING});
  await expectedError('one-layer depth view still fails cube dimension',()=>device.createBindGroup({layout:pipeline.getBindGroupLayout(1),entries:[{binding:3,resource:{buffer:cb3}},{binding:6,resource:{buffer:cb6}},{binding:30,resource:sampler('greater-equal','nearest')},{binding:46,resource:oneDepth.createView({dimension:'2d'})},{binding:56,resource:spot.createView()}]}));
  await device.queue.onSubmittedWorkDone();check(!errors.length,'Uncaptured WebGPU errors: '+errors.join('; '));
  return {result:'PASS',adapter:adapterInfo,backend:'browser WebGPU / Windows; not wgpu-native Metal',exactOriginalShadowBlock:input.exactOriginalShadowBlock,shaderSHA256:input.shaderSHA256,originalPSCompiled:true,draws,pixels,invalidChecks:positiveInvalidChecks,cases,invalid,source708IdentityEstablished:false,productionFallbackValidated:false};
 }finally{for(const r of resources)r.destroy();device.destroy();}
})().catch(error=>({result:'FAIL',message:error.stack||error.message}));
window.runDone.then(result=>{document.body.textContent=JSON.stringify(result);});
