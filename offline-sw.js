import {ISOLATION,sha256,localResponse} from './offline-common.js';
import {dataDirectory,fileName} from './offline-store.js';
const RELEASE = '52acc2603bbfea8f0a6807ba6e3f41e25ac1829dec417e7433f156807fdeab13';
const APP_BASE='/gta5loader/';
const appURL=path=>APP_BASE+path.replace(/^\//,'');
const CACHE='gta-mobile-shell-'+RELEASE;
let manifestPromise,dirPromise;
async function manifest() {
  return manifestPromise ||= (async()=>{
    const cached=await (await caches.open(CACHE)).match(appURL('/offline-manifest.json'));
    if(!cached)throw Error('Offline launcher metadata is missing.');
    const m=await cached.json();if(m.id!==RELEASE)throw Error('Offline launcher version mismatch.');
    m.byPath=new Map(m.files.map(f=>[f.path,f]));m.setupPaths=new Set(m.files.flatMap(f=>f.setupChunks||[]));return m;
  })();
}
self.addEventListener('install',event=>event.waitUntil((async()=>{
  const response=await fetch(appURL('/offline-manifest.json'),{cache:'no-store'});if(!response.ok)throw Error('Missing install manifest');
  const text=await response.text(),m=JSON.parse(text);if(m.id!==RELEASE)throw Error('Manifest does not match this worker');
  const cache=await caches.open(CACHE);
  // The launcher is small. Game archives are installed separately into OPFS.
  for(const file of m.shell){
    const r=await fetch(appURL(file.path),{cache:'no-store'});if(!r.ok)throw Error('Missing launcher file: '+file.path);
    const bytes=await r.arrayBuffer();if(bytes.byteLength!==file.size||await sha256(bytes)!==file.sha256)throw Error('Launcher checksum mismatch: '+file.path);
    await cache.put(appURL(file.path),new Response(bytes,{headers:r.headers}));
  }
  await cache.put(appURL('/offline-manifest.json'),new Response(text,{headers:{...ISOLATION,'Content-Type':'application/json'}}));
})()));
self.addEventListener('activate',event=>event.waitUntil(self.clients.claim()));
function errorResponse(message,status=503) {return new Response(message,{status,headers:{...ISOLATION,'Content-Type':'text/plain; charset=utf-8','X-GTA-Source':'phone-storage'}});}
async function serve(request) {
  const url=new URL(request.url);
  if(url.origin!==self.location.origin)return errorResponse('External requests are not used by this offline edition.',403);
  if((url.pathname==='/log'||url.pathname===APP_BASE+'log')&&request.method==='POST')return new Response(null,{status:204,headers:ISOLATION});
  if(request.method!=='GET'&&request.method!=='HEAD')return errorResponse('Method not supported',405);
  let path;try{path=decodeURIComponent(url.pathname);}catch{return errorResponse('Invalid path',400);}
  if(path.startsWith(APP_BASE))path='/'+path.slice(APP_BASE.length);
  if(path==='/favicon.ico')return new Response(null,{status:204,headers:ISOLATION});
  if(path==='/')path='/index.html';
  const m=await manifest();
  // Only the installer requests the network, explicitly, for verified chunks.
  if(url.searchParams.get('__install')===m.id&&(m.byPath.has(path)||m.setupPaths.has(url.pathname)))return fetch(request);
  const shell=await (await caches.open(CACHE)).match(appURL(path));
  if(shell){const headers=new Headers(shell.headers);for(const [k,v] of Object.entries(ISOLATION))headers.set(k,v);headers.set('X-GTA-Source','phone-storage');return new Response(request.method==='HEAD'?null:shell.body,{status:200,headers});}
  const file=m.byPath.get(path);if(!file)return errorResponse('This file is not in the offline package: '+path,404);
  try{
    const dir=await (dirPromise ||= dataDirectory(m));
    const stored=await (await dir.getFileHandle(fileName(file))).getFile();
    if(stored.size!==file.size)throw Error('Incomplete file');
    return localResponse(stored,request,path);
  }catch{dirPromise=undefined;return errorResponse('This game file is not installed. Open the launcher and resume installation: '+path);}
}
self.addEventListener('fetch',event=>event.respondWith(serve(event.request).catch(()=>errorResponse('Offline storage could not be opened. Return to the launcher.'))));
