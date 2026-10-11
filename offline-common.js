export const SHELL_PATHS = ['/index.html','/game.html','/mobile.html','/installer.js','/installer.css','/offline-common.js','/offline-store.js','/install-worker.js','/offline-sw.js','/platform-check.js','/platform-probe.js','/app.webmanifest','/b/8b0b5899ed/title/logo.png'];
export const ISOLATION = {
  'Cross-Origin-Opener-Policy':'same-origin',
  'Cross-Origin-Embedder-Policy':'require-corp',
  'Cross-Origin-Resource-Policy':'same-origin',
  'X-Content-Type-Options':'nosniff',
  'Content-Security-Policy':"default-src 'self' blob: data:; script-src 'self' 'unsafe-inline' 'wasm-unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; worker-src 'self' blob:; object-src 'none'; base-uri 'self'"
};
export function mimeType(path) {
  const ext=path.slice(path.lastIndexOf('.')).toLowerCase();
  return ({'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.json':'application/json','.webmanifest':'application/manifest+json','.wasm':'application/wasm','.woff':'font/woff','.png':'image/png','.webp':'image/webp','.svg':'image/svg+xml','.wgsl':'text/plain; charset=utf-8','.txt':'text/plain; charset=utf-8'})[ext] || 'application/octet-stream';
}
export function parseRange(value,size) {
  if(!value)return null;
  const m=/^bytes=(\d*)-(\d*)$/.exec(value);
  if(!m||(!m[1]&&!m[2])||size===0)return {invalid:true};
  let start=0,end=size-1;
  if(!m[1]){const suffix=Number(m[2]);if(!Number.isSafeInteger(suffix)||suffix<=0)return {invalid:true};start=Math.max(0,size-suffix);}
  else {start=Number(m[1]);if(m[2]){const requested=Number(m[2]);if(!Number.isSafeInteger(requested))return {invalid:true};end=Math.min(end,requested);}}
  if(!Number.isSafeInteger(start)||start<0||start>=size||end<start)return {invalid:true};
  return {start,end,length:end-start+1};
}
export function localResponse(blob,request,path) {
  const headers=new Headers({...ISOLATION,'Content-Type':mimeType(path),'Cache-Control':'no-store','Accept-Ranges':'bytes','X-GTA-Source':'phone-storage'});
  const range=parseRange(request.headers.get('Range'),blob.size);
  if(range?.invalid){headers.set('Content-Range','bytes */'+blob.size);return new Response(null,{status:416,headers});}
  const length=range?range.length:blob.size;headers.set('Content-Length',String(length));
  if(range)headers.set('Content-Range',`bytes ${range.start}-${range.end}/${blob.size}`);
  const body=request.method==='HEAD'?null:range?blob.slice(range.start,range.end+1):blob;
  return new Response(body,{status:range?206:200,headers});
}
export async function sha256(bytes) {
  return [...new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))].map(n=>n.toString(16).padStart(2,'0')).join('');
}
export function folderPath(file) {
  const relative=file.webkitRelativePath || file.name;
  const parts=relative.split('/');
  const web=parts.indexOf('web');
  const path='/'+parts.slice(web>=0?web+1:1).join('/');
  if(path==='/'||parts.some(p=>p==='..'||p==='.'||p.includes('\\')))return null;
  return path;
}
export function formatBytes(bytes) {return (bytes/1e9).toFixed(2)+' GB';}
