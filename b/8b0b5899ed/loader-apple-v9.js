// Worker that hosts the engine ("main runtime thread" of the Emscripten module). importScripts('game.js') runs the static constructors
// and main() here; the loop blocks this worker, which is fine (nothing else runs on it). game.js also runs in every pthread worker.
'use strict';
// Same-size native constants and removal of debug-only trailing metadata.
// Source remains the installer's verified v7 engine; no whole-file copy or download.
function appleEngineResponse(response,patches,sourceLength,outputLength,truncateAt){
 let offset=0,checked=0;
 const body=response.body.pipeThrough(new TransformStream({
  transform(chunk,controller){
   let out=chunk;
   for(const p of patches){
    const begin=Math.max(offset,p.offset),end=Math.min(offset+chunk.length,p.offset+p.before.length);
    if(begin>=end)continue;
    if(out===chunk)out=Uint8Array.from(chunk);
    for(let at=begin;at<end;at++){
     const i=at-offset,j=at-p.offset;
     if(chunk[i]!==p.before[j])throw Error('Wrong engine version. Return to setup and update engine support.');
     if(p.after)out[i]=p.after[j];checked++;
    }
   }
   const count=Math.max(0,Math.min(out.length,truncateAt-offset));
   if(count)controller.enqueue(out.subarray(0,count));
   offset+=chunk.length;
  },
  flush(){if(offset!==sourceLength||checked!==patches.reduce((n,p)=>n+p.before.length,0))throw Error('Incomplete installed engine. Return to setup to repair it.');}
 }));
 const headers=new Headers(response.headers);headers.delete('Content-Encoding');headers.set('Content-Length',String(outputLength));headers.set('X-Uncompressed-Length',String(outputLength));
 return new Response(body,{status:response.status,statusText:response.statusText,headers});
}

let started = false;
const startupChannel = new BroadcastChannel('game-progress');
const reportStartup = (label) => startupChannel.postMessage({startup:label,label});
let failed=false;
function failStartup(error) {
	if(failed)return;failed=true;
	const detail=error?.message ? error.message+(error.stack?'\n'+error.stack:'') : String(error);
	startupChannel.postMessage({error:'Engine startup failed: '+detail.slice(0,700)});
}
addEventListener('error',e=>failStartup(e.error||e.message));
addEventListener('unhandledrejection',e=>failStartup(e.reason));
self.onmessage = (ev) => {
	if (started) return;
	started = true;
	const m = ev.data;
	// ?cores=N (debug): pretend to be a machine with N logical cores; the engine sizes its worker pools from it
	if (m.cores) Object.defineProperty(navigator, 'hardwareConcurrency', { value: m.cores });
	// The GPU worker must be up before the engine blocks this thread: Chrome fetches the script of a worker nested in a worker through
	// the parent's event loop. It idles until the engine sends its start message (wgpu_backend.cpp: wgpu_start_worker).
	const gq = new URLSearchParams();		// the GPU worker reads its options from its own URL
	if (m.slots) gq.set('slots', m.slots);
	if (m.syncPipelines) gq.set('syncpipelines', '1');
	if (m.remoteLog) gq.set('log', '1');		// ?log=1 on the page: its log lines go to the dev server (off by default)
	if (m.lowMemory) gq.set('low', '1');		// the low-memory profile: its own pipeline seed (wgpu_worker.js loadRecipes)
	if (m.noPack) gq.set('nopack', '1');		// ?nopack=1: shaders one by one instead of from the packs (diagnostics)
	if (m.shotRt) gq.set('shotrt', '1');		// ?shotrt=1 (with ?shot=N): every render target of one frame after the world is shown goes to the dev server (diagnosis)
	if (m.gpuLimits) gq.set('limits', m.gpuLimits);		// ?limits=name:value,...: a smaller device (diagnosis of weaker GPUs)
	const B = m.base || '';		// the page's URL prefix for everything it loads (index.html BASE): /b/<build> on the PHP host, empty otherwise
	if(m.compactMemory)gq.set('compact','1');
	const gpu = new Worker(B + '/wgpu-apple-v9.js' + (gq.toString() ? '?' + gq : ''));
	const io = new Worker(B + '/io-apple-v9.js');
	io.onerror=e=>failStartup('File worker: '+e.message);
	io.onmessageerror=()=>failStartup('The browser could not receive a file-worker message.');
	gpu.onerror=e=>failStartup('Graphics worker: '+e.message);
	gpu.onmessageerror=()=>failStartup('The browser could not receive a graphics-worker message.');
	reportStartup('Starting graphics and file workers');
	io.postMessage({ init: true, localManifest:new URL('../../offline-manifest.json',self.location.href).href, base: self.location.origin + '/data/', noStore: !!m.noStore, record: !!m.record, trace: !!m.trace, log: !!m.remoteLog, noHints: !!m.noHints, bootset: m.lowMemory ? 'bootset_low.json' : 'bootset.json' });		// starts the prefetch of the boot read set at once		// HTTP reads of all engine threads (platform/file/httpfs_wasm.cpp); same reason to create it up front
	let loaded = 0;
	const workersTimer=setTimeout(()=>failStartup('Graphics/file workers did not become ready within 60 seconds.'),60000);
	const ready=()=>{if(++loaded===2){clearTimeout(workersTimer);if(!failed)start().catch(failStartup);}};
	io.onmessage = (e) => { if (e.data.loaded) ready(); };
	gpu.onmessage = (e) => {
		if (e.data.loaded) ready();
	};
	// An overloaded host answers 503/500/429 (2026-10-06): retried up to 8 times with growing, jittered waits (~20 s in all) instead of compiling an error page.
	const fetchWasm = async () => {
		for (let attempt = 0; ; attempt++) {
			let res = null, err = null;
			try { res = await fetch(B + '/game.wasm'); } catch (e) { err = e; }
			if (res && res.ok) return appleEngineResponse(res,[{"offset":2653489,"before":[66,128,128,128,144,0],"after":[66,128,128,128,160,0]},{"offset":2653499,"before":[66,128,128,128,144,0],"after":[66,128,128,128,160,0]},{"offset":59349275,"before":[0,199,189,160,3,4,110,97,109,101],"after":null}],66172903,59349275,59349275);
			if (attempt >= 7) { if (res) throw new Error('HTTP ' + res.status + ' for game.wasm'); throw err; }
			bc0.postMessage({ label: 'The server is busy, retrying (' + (attempt + 1) + ')' });
			await new Promise((r) => setTimeout(r, Math.min(5000, 300 * 2 ** attempt) * (0.5 + Math.random())));
		}
	};
	const bc0 = new BroadcastChannel('game-progress');
	const start = async () => {
		// A single small read from verified phone storage supplies every pthread.
		// Blob workers avoid repeated nested script loads through Safari's offline
		// handler. The controller bridge is part of this bundle, with no imports.
		reportStartup('Preparing local engine worker script');
		let source;
		for(let attempt=0;attempt<3;attempt++){
			try{
				const response=await fetch(B+'/game.js',{signal:AbortSignal.timeout(30000)});
				if(!response.ok)throw Error('HTTP '+response.status+' while reading the engine worker script.');
				source=await response.text();
				if(!source.startsWith('// Offline controller bridge bundled')||source.length>2000000)
					throw Error('Engine support files are out of date. Return to setup and tap Update engine support.');
				break;
			}catch(error){if(attempt===2)throw error;}
		}
		const options={remoteLog:!!m.remoteLog,fullConsole:!!m.fullConsole,compactMemory:!!m.compactMemory};
		const workerScriptUrl=URL.createObjectURL(new Blob([
			'var Module='+JSON.stringify(options)+';\n',source
		],{type:'text/javascript'}));
		// Keep this URL alive: the engine can create additional pthreads later.
		source=null;
		const bc = new BroadcastChannel('game-progress');
		self.Module = {
			compactMemory: !!m.compactMemory,
			workerStartupConcurrency: m.lowMemory ? 2 : 4,
			onWorkerStartup: (loaded,total) => reportStartup('Starting engine workers ('+loaded+' / '+total+')'),
			onStartupError: failStartup,
			onRuntimeInitialized: () => reportStartup('Engine initialized; opening game data'),
			// Download game.wasm ourselves to report progress (0-20 %); emscripten hands the compiled module on to the pthread workers.
			instantiateWasm: (imports, done) => {
				// Streaming compilation: the module compiles while it downloads (no compile wait after the download, no 2 x 63 MB buffer copy, and Chrome can cache the
					// compiled code of a fetched response for the next visit). Progress counts the consumed stream. Failure falls back to download-then-compile.
				const buffered = async () => {
					const res = await fetchWasm();
					reportStartup('Reading the engine from local storage');
					const bytes = await res.arrayBuffer();
					bc.postMessage({ pct: 20, label: 'Compiling the engine' });
					reportStartup('Compiling the engine');
					return WebAssembly.instantiate(bytes, imports);
				};
				const streaming = async () => {
					const res = await fetchWasm();
					const total = +(res.headers.get('X-Uncompressed-Length') || res.headers.get('Content-Length')) || 0;		// the body is decoded while it is read: count decoded bytes
					// Count the stream consumed by the compiler; do not clone/tee a
					// second 66 MB response while a phone is compiling the first one.
					let got=0,last=0;
					const body=res.body.pipeThrough(new TransformStream({
						transform(chunk,controller){got+=chunk.byteLength;controller.enqueue(chunk);if(performance.now()-last>150){last=performance.now();bc.postMessage({pct:total?20*got/total:0,label:'Reading and compiling engine ('+(got/1048576|0)+' MB)'});}},
						flush(){bc.postMessage({pct:20,label:'Compiling the engine'});reportStartup('Compiling the engine');}
					}));
					return WebAssembly.instantiateStreaming(new Response(body,{headers:res.headers}), imports);
				};
				(async () => {
					let r;
					// An allocation/compile/runtime failure must not start another full compile.
     r = typeof WebAssembly.instantiateStreaming==='function' ? await streaming() : await buffered();
					bc.postMessage({ pct: 22, label: 'Starting the engine' });
					reportStartup('Preparing engine workers');
					done(r.instance, r.module);
				})().catch(failStartup);
				return {};
			},
			canvas: m.canvas,
			controllerState: m.controllerState,
			arguments: m.args,
			wgpuWorker: gpu,
			ioWorker: io,
			fullConsole: !!m.fullConsole,		// ?console=1: every engine line in the browser console (default: errors, warnings, the port's lines)
			remoteLog: !!m.remoteLog,		// ?log=1: every engine line to the dev server's log (prejs.js; default: none)
			wgpuShaderUrl: B + '/shaders',
			wgpuShotEvery: m.shotEvery,
			keepThreads: m.keepThreads|0,		// bit mask of engine thread groups that are NOT skipped (1 replay, 2 net, 4 bink), see ipc_wasm.cpp
			mainScriptUrlOrBlob: workerScriptUrl,
			locateFile: (p) => B + '/' + p,
			onExit: (code) => { console.error('EXIT ' + code); self.__log('EXIT ' + code); self.__logFlush(); if (code) self.__crash('exit', 'EXIT ' + code); },
			onAbort: (what) => { console.error('ABORT ' + what); self.__log('ABORT ' + what); self.__logFlush(); self.__crash('abort', 'ABORT ' + what); },
			// Savegames and settings kept in IndexedDB by io_worker.js (platform/file/userdata_wasm.cpp sends them): put them back into the in-memory /userdata before the engine looks at it. The run
			// dependency holds main() until they are in (or 8 s, so a broken IndexedDB cannot keep the game from starting). Only /userdata/ paths are accepted.
			preRun: [() => {
				if (typeof FS === 'undefined' || typeof addRunDependency !== 'function' || typeof indexedDB === 'undefined') return;
				addRunDependency('userdata');
				let finished = false, db=null, tx=null;
				const finish = (note) => { if (finished) return; finished = true; clearTimeout(timer);if(db)db.close(); self.__log('[loader] userdata: ' + note); self.__logFlush(); removeRunDependency('userdata'); };
				const timer=setTimeout(()=>{if(tx){try{tx.abort();}catch{}}finish('timed out');},8000);
				try {
					const r = indexedDB.open('gta5-userdata', 1);
					r.onupgradeneeded = () => r.result.createObjectStore('files');
					r.onerror = () => finish('IndexedDB unavailable: ' + r.error);
					r.onsuccess = () => {
      db=r.result;if(finished){db.close();return;}
						let n = 0, bytes = 0;
						try {
							tx=db.transaction('files','readonly');
       const req=tx.objectStore('files').openCursor();
							req.onerror = () => finish('read failed: ' + req.error);
							req.onsuccess = () => {
								if(finished)return;
								const c = req.result;
								if (!c) { finish('restored ' + n + ' file(s), ' + bytes + ' bytes'); return; }
								const path = String(c.key);
								try {
									if (path.startsWith('/userdata/') && c.value && c.value.data) {
										FS.mkdirTree(path.slice(0, path.lastIndexOf('/')));
										FS.writeFile(path, c.value.data);
										FS.utime(path, c.value.mtime, c.value.mtime);
										n++; bytes += c.value.data.length;
									}
								} catch (e) { self.__log('[loader] userdata: ' + path + ' not restored: ' + e); }
								c.continue();
							};
						} catch (e) { finish('read failed: ' + e); }
					};
				} catch (e) { finish('failed: ' + e); }
			}],
		};
		// __log/__logFlush come from prejs.js, which only exists once game.js is loaded: note the start line in the console.
		console.log('loader: starting engine: ' + m.args.join(' ') + ' (cores ' + m.cores + ')');
		importScripts(B + '/game.js');
	};
};
