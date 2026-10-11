import {checkGameSupport} from './platform-check.js';
import {formatBytes,sha256} from './offline-common.js';
const el=id=>document.getElementById(id);
let worker,manifest,busy=false,supported=false,ready=false,wake,storedBytes=0,spaceAvailable=true;
function message(text){el('status').textContent=text;}
function controls(){el('check').disabled=busy;el('download').disabled=busy||!supported||!spaceAvailable;el('download').hidden=manifest?.installSource==='folder';el('import').disabled=busy||!supported||!spaceAvailable;el('pause').hidden=!busy;el('play').hidden=!ready||busy||!supported;el('offline-test').hidden=!ready||busy;}
function progress(data){
  ready=data.ready;storedBytes=data.bytes;el('progress').value=data.totalBytes?data.bytes/data.totalBytes*100:0;
  el('bytes').textContent=formatBytes(data.bytes)+' / '+formatBytes(data.totalBytes);
  el('count').textContent=data.complete.toLocaleString()+' / '+data.total.toLocaleString()+' files';
  if(data.current)el('current').textContent=data.current;
  el('state-badge').textContent=ready?'Installed':busy?'Installing':'Not installed';el('state-badge').classList.toggle('ready',ready);
  controls();
}
async function storageInfo(){
  try{
    const estimate=await navigator.storage.estimate();
    const remaining=Math.max(0,(manifest?.totalBytes||0)-storedBytes);
    spaceAvailable=ready||!Number.isFinite(estimate.quota)||remaining+64*1024*1024<=Math.max(0,estimate.quota-(estimate.usage||0));
    el('space').textContent='Browser storage: '+formatBytes(estimate.usage||0)+' used; estimated quota '+formatBytes(estimate.quota||0)+'. '+(spaceAvailable?'The quota is not a guarantee of free disk space.':'This quota is too small for the remaining '+formatBytes(remaining)+'. Use the Home Screen app in regular Safari, free device storage, and check again.');
  }catch{spaceAvailable=true;el('space').textContent='Storage quota could not be estimated. Allow at least 22 GB of free space.';}
  controls();return spaceAvailable;
}
async function holdAwake(){try{wake=await navigator.wakeLock?.request('screen');}catch{}}
async function releaseAwake(){try{await wake?.release();}catch{}wake=null;}
async function persistence(){
  try{const kept=await navigator.storage.persisted()||await navigator.storage.persist();el('persistence').textContent=kept?'Persistent storage granted. Keep the app installed and do not clear its website data.':'Persistent storage was not granted. Open from the Home Screen and keep free storage; iOS may remove website data under pressure.';}catch{el('persistence').textContent='Storage persistence could not be confirmed.';}
}
async function start(files){
  if(busy||!supported)return;
  if(!files&&manifest.installSource==='folder'){message('This setup page installs from a local web folder. Choose Import a local web folder.');return;}
  busy=true;controls();await persistence();
  if(!await storageInfo()){busy=false;controls();message('Not enough browser storage for the complete game. See the storage estimate below.');return;}
  message(files?'Importing and verifying local files…':'Downloading and verifying the game…');
  el('test-result').textContent='';holdAwake();
  worker.postMessage({command:'install',...(files?{files}:{} )});
}
el('download').onclick=()=>start();
el('import').onclick=()=>el('folder').click();
el('folder').onchange=()=>{if(el('folder').files.length)start([...el('folder').files]);el('folder').value='';};
el('pause').onclick=()=>{message('Finishing the current storage checkpoint…');worker.postMessage({command:'pause'});};
el('check').onclick=async()=>{
  el('check').disabled=true;message('Checking browser, graphics and storage support…');
  const result=await checkGameSupport();
  if(!navigator.storage?.getDirectory)result.failures.push('This browser cannot store the game in its private file system.');
  supported=result.ok&&result.failures.length===0;
  await persistence();await storageInfo();
  message(supported?(ready?'Installed. You can play from this device’s stored files.':spaceAvailable?'Device feature check passed. Choose how to install the files below.':'Browser features passed, but its storage quota is too small for this game. See the estimate below.') : result.failures.join(' '));
  controls();
};
el('offline-test').onclick=async()=>{
  el('offline-test').disabled=true;el('test-result').textContent='Checking stored engine and game data without a network fallback…';
  try{
    const samples=[manifest.files.find(f=>f.path.endsWith('/game.wasm')),manifest.files.find(f=>f.path.startsWith('/data/')&&f.size>manifest.chunkBytes)].filter(Boolean);
    for(const file of samples){
      const length=Math.min(file.size,manifest.chunkBytes);
      const response=await fetch(file.path+'?local-check=1',{headers:{Range:`bytes=0-${length-1}`},cache:'no-store'});
      if(response.status!==206||response.headers.get('X-GTA-Source')!=='phone-storage'||await sha256(await response.arrayBuffer())!==file.chunks[0])throw Error('Local file check failed: '+file.path);
    }
    el('test-result').textContent='Passed: engine and game-data bytes were served and verified from this device. To test a completely offline launch, enable Airplane Mode, reopen this Home Screen app, and tap Play offline.';
  }catch(e){el('test-result').textContent=e.message+' Resume installation to repair missing files.';}
  finally{el('offline-test').disabled=false;}
};
document.addEventListener('visibilitychange',()=>{if(busy&&!document.hidden)holdAwake();});
async function boot(){
  if(!isSecureContext||!navigator.serviceWorker)throw Error('Open this setup page over HTTPS on the iPhone. A file or ordinary Wi-Fi HTTP address cannot install the offline app.');
  const existing=await navigator.serviceWorker.getRegistration('/gta5loader/');
  const own=existing?.active?.scriptURL===new URL('/gta5loader/offline-sw.js',location.href).href;
  // Reopening an installed app must not depend on a successful network update.
  const registration=own?existing:await navigator.serviceWorker.register('/gta5loader/offline-sw.js',{type:'module',scope:'/gta5loader/',updateViaCache:'none'});
  if(own)registration.update().catch(()=>{});
  if(registration.waiting){throw Error('An update is ready. Close all game/installer tabs for this address, then reopen the Home Screen app.');}
  await new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(Error('Offline launcher setup did not finish. Check the setup connection and reload this page.')),60000);
    navigator.serviceWorker.ready.then(r=>{clearTimeout(timer);resolve(r);},e=>{clearTimeout(timer);reject(e);});
  });
  if(!navigator.serviceWorker.controller){await new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(Error('Offline handler could not take control. Reload this page.')),15000);
    navigator.serviceWorker.addEventListener('controllerchange',()=>{clearTimeout(timer);resolve();},{once:true});
  });}
  // GitHub Pages cannot set COOP/COEP. The installed service worker adds
  // them to the next navigation; a controlled reload enables shared memory.
  const isolationURL=new URL(location.href);
  if(!globalThis.crossOriginIsolated){
    if(isolationURL.searchParams.get('offline-isolation')==='1')throw Error('This browser did not enable the isolation required by the game. Close this app and try current Safari.');
    isolationURL.searchParams.set('offline-isolation','1');
    message('Enabling offline browser support. This page will reload once…');
    location.replace(isolationURL.href);return;
  }
  if(isolationURL.searchParams.has('offline-isolation')){
    isolationURL.searchParams.delete('offline-isolation');history.replaceState(null,'',isolationURL.href);
  }
  manifest=await (await fetch('/gta5loader/offline-manifest.json')).json();
  worker=new Worker('/gta5loader/install-worker.js',{type:'module'});
  worker.onmessage=({data})=>{
    if(['status','progress','complete'].includes(data.type))progress(data);
    if(data.type==='status'){message(data.ready?'Installed on this device. Checking browser support…':'Offline launcher ready. Check this device before installing '+formatBytes(data.totalBytes)+'.');if(data.ready)el('check').click();}
    if(['complete','paused','error'].includes(data.type)){
      busy=false;releaseAwake();controls();storageInfo();
      message(data.type==='complete'?'Installed and verified. Your PC and internet connection are no longer needed for the stored game.':data.type==='paused'?'Paused. Verified files and chunks are kept; use Install to resume, or choose the local folder again.':data.message);
      el('state-badge').textContent=data.type==='complete'?'Installed':data.type==='paused'?'Paused':'Needs attention';
    }
  };
  worker.onerror=()=>{busy=false;releaseAwake();controls();message('The storage worker stopped. Reload this app and resume; completed checkpoints are retained.');};
  worker.postMessage({command:'status'});controls();storageInfo();
}
boot().catch(e=>{message(e.message);el('state-badge').textContent='Setup required';});
