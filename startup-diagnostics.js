import {shouldPauseRestart} from './restart-policy.js';
export function installStartupDiagnostics() {
  const key='gta-run-state-v5';
  const read=store=>{try{return JSON.parse(store.getItem(key));}catch{return null;}};
  let previous;try{previous=read(sessionStorage)||read(localStorage);}catch{}
  const previousReport=previous?.report||'';
  const lines=[];let lastSave=0,phase='Opening launcher',failure='',world=false,active=false,paused=false;
  let heap=512,requested=0,gpu=null;
  const start=performance.now();
  const button=document.createElement('button');button.textContent='Loading details';
  button.style.cssText='position:fixed;top:max(12px,env(safe-area-inset-top));right:max(12px,env(safe-area-inset-right));z-index:40;padding:10px;border:1px solid #62758a;border-radius:8px;background:#172431;color:#fff;font:14px system-ui';
  const panel=document.createElement('section');panel.hidden=true;
  panel.style.cssText='position:fixed;inset:8% 5%;z-index:50;background:#101720f5;color:#edf4fa;padding:20px;border:1px solid #62758a;border-radius:12px;font:15px/1.4 system-ui;overflow:auto';
  const heading=document.createElement('h2');heading.textContent='Game loading details';
  const status=document.createElement('p'),pre=document.createElement('pre');pre.style.cssText='white-space:pre-wrap;overflow-wrap:anywhere;font:12px/1.4 monospace';
  const copy=document.createElement('button'),close=document.createElement('button'),back=document.createElement('a');
  copy.textContent='Copy details';close.textContent='Close';back.textContent='Back to setup';back.href='/gta5loader/';
  for(const el of [copy,close,back])el.style.cssText='display:inline-block;background:#b4e9c3;color:#10241a;padding:12px;margin:0 8px 12px 0;border:0;border-radius:8px;font:700 14px system-ui;text-decoration:none';
  const currentReport=()=>['Startup repair v7 — minimal engine',navigator.userAgent,'Isolated: '+crossOriginIsolated,'Elapsed: '+Math.round((performance.now()-start)/1000)+' s','Step: '+phase,'Engine heap: '+Math.round(heap)+' MiB'+(requested?' (last growth request '+Math.round(requested)+' MiB)':''),'Graphics: '+(gpu?JSON.stringify(gpu):'Not reported'),'First error: '+(failure||'No exception reported'),...lines].join('\n');
  const report=()=>paused?('Previous session before restart:\n'+previousReport):currentReport();
  const render=()=>{status.textContent=failure||('Current step: '+phase);pre.textContent=report();};
  function save(force=false){if(!active)return;if(!force&&performance.now()-lastSave<1000)return;lastSave=performance.now();const state=JSON.stringify({active:true,updated:Date.now(),report:currentReport()});try{sessionStorage.setItem(key,state);}catch{}try{localStorage.setItem(key,state);localStorage.setItem('gta-last-startup',currentReport());}catch{}}
  function record(text){lines.push(Math.round((performance.now()-start)/1000)+'s '+String(text).slice(0,700));if(lines.length>40)lines.shift();if(!panel.hidden&&!paused)render();save();}
  const show=()=>{render();panel.hidden=false;};
  button.onclick=show;close.onclick=()=>{panel.hidden=true;};
  copy.onclick=async()=>{try{await navigator.clipboard.writeText(report());copy.textContent='Copied';}catch{copy.textContent='Select the details below to copy';}};
  panel.append(heading,status,copy,close,back,pre);document.body.append(button,panel);
  const progress=new BroadcastChannel('game-progress');
  progress.onmessage=({data:m})=>{
    if(!active)return;
    if(m.startup||m.label){phase=m.startup||m.label;record(phase);save(true);}
    if(m.compiling!==undefined){phase='Preparing shaders ('+m.compiling+' pending)';save();}
    if(m.memoryMiB!==undefined){heap=Math.max(heap,m.memoryMiB);if(m.memoryRequestedMiB!==undefined)requested=Math.max(requested,m.memoryRequestedMiB);save(true);}
    if(m.gpuMemory){gpu=m.gpuMemory;save();}
    if(m.gpuLost){failure ||= 'Graphics device lost: '+m.gpuLost;record(failure);save(true);show();}
    if(m.lines)for(const line of m.lines.slice(-3))record(line);
    if(m.error){failure ||= m.error;record(m.error);save(true);show();}
    if(m.world){world=true;phase='Playing';record('World shown');save(true);button.textContent='Game details';button.hidden=false;panel.hidden=true;}
  };
  const crashes=new BroadcastChannel('game-crash');
  crashes.onmessage=({data:m})=>{const detail=String(m.text||'Engine worker failed');failure ||= detail;record('Heap: '+m.heap+' MiB; '+detail);button.hidden=false;show();};
  addEventListener('error',e=>{failure ||= e.message;record(e.message);show();});
  addEventListener('unhandledrejection',e=>{const detail=String(e.reason?.message||e.reason);failure ||= detail;record(detail);show();});
  setInterval(()=>save(),2000);
  addEventListener('pagehide',()=>{if(!active)return;save(true);const state=JSON.stringify({active:false,updated:Date.now(),report:currentReport()});try{sessionStorage.setItem(key,state);}catch{}try{localStorage.setItem(key,state);}catch{}});
  addEventListener('pageshow',e=>{if(e.persisted&&active)save(true);});
  return {
    async beforeStart(){
      const navigationType=performance.getEntriesByType('navigation')[0]?.type;
      if(shouldPauseRestart(previous,navigationType)){
        paused=true;heading.textContent='Game restart paused';close.style.display='none';
        failure='The previous game session ended or reloaded. Automatic restart is paused. Memory pressure or a browser/graphics failure may be responsible; this report cannot confirm which.';
        const retry=document.createElement('button');retry.textContent='Retry with reduced graphics';retry.style.cssText=copy.style.cssText;
        panel.insertBefore(retry,pre);show();
        await new Promise(resolve=>{retry.onclick=()=>{const url=new URL(location.href);url.searchParams.set('compact','1');url.searchParams.set('low','1');url.searchParams.delete('res');url.searchParams.delete('slots');history.replaceState(null,'',url);retry.remove();resolve();};});
        paused=false;failure='';close.style.display='inline-block';heading.textContent='Game loading details';panel.hidden=true;
      }
      active=true;record('Starting one game session');save(true);
    },
    stall(seconds){if(world||failure)return;record('No engine activity for '+seconds+' seconds; cause not yet known.');show();},record
  };
}
