// Check the actual capabilities, not a browser name or version guess.
export async function checkGameSupport({gpu=true, timeout=20000}={}) {
  const failures=[];
  if (!globalThis.isSecureContext) failures.push('Open an HTTPS webpage (or localhost on the computer hosting the game). A file or ordinary phone Wi-Fi HTTP address cannot run this build.');
  if (!globalThis.crossOriginIsolated || typeof SharedArrayBuffer==='undefined') failures.push('The server must send Cross-Origin-Opener-Policy: same-origin and Cross-Origin-Embedder-Policy: require-corp.');
  if (typeof Worker==='undefined' || typeof OffscreenCanvas==='undefined' || !HTMLCanvasElement.prototype.transferControlToOffscreen) failures.push('This browser needs Workers and transferable OffscreenCanvas support.');
  if (failures.length) return {ok:false,failures};
  return new Promise(resolve=>{
    let worker, timer;
    const finish=result=>{clearTimeout(timer);worker?.terminate();resolve(result);};
    try {
      worker=new Worker('/gta5loader/platform-probe.js',{type:'module'});
      timer=setTimeout(()=>finish({ok:false,failures:['The browser compatibility check timed out. Reload, or try the current version of Safari, Chrome or Edge.']}),timeout);
      worker.onmessage=e=>finish(e.data);
      worker.onerror=()=>finish({ok:false,failures:['The compatibility worker could not start. Check that all web files are hosted together and the browser is up to date.']});
      worker.postMessage({gpu});
    } catch {
      finish({ok:false,failures:['The browser could not start a background worker.']});
    }
  });
}

export function showCompatibilityFailure(result) {
  const panel=document.createElement('section');panel.id='compatibility-error';
  panel.style.cssText='position:fixed;inset:0;z-index:100;background:#10141c;color:#f4f6fa;padding:max(28px,env(safe-area-inset-top)) max(24px,env(safe-area-inset-right)) 28px max(24px,env(safe-area-inset-left));box-sizing:border-box;overflow:auto;font:16px/1.55 system-ui';
  const title=document.createElement('h1');title.textContent='This browser cannot start the game yet';
  const list=document.createElement('ul');
  for(const failure of result.failures){const item=document.createElement('li');item.textContent=failure;list.append(item);}
  const link=document.createElement('a');link.href='/gta5loader/mobile.html';link.textContent='Open phone launch guide';link.style.color='#9edabb';
  panel.append(title,list,link);document.body.append(panel);
}
