export function installStartupDiagnostics() {
  const lines=[];let lastSave=0,phase='Opening launcher',failure='',world=false;
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
  const report=()=>['Startup repair v4',navigator.userAgent,'Isolated: '+crossOriginIsolated,'Elapsed: '+Math.round((performance.now()-start)/1000)+' s','Step: '+phase,'First error: '+(failure||'No exception reported'),...lines].join('\n');
  const render=()=>{status.textContent=failure||('Current step: '+phase);pre.textContent=report();};
  function record(text){lines.push(Math.round((performance.now()-start)/1000)+'s '+String(text).slice(0,700));if(lines.length>40)lines.shift();if(!panel.hidden)render();if(performance.now()-lastSave>1000){lastSave=performance.now();try{localStorage.setItem('gta-last-startup',report());}catch{}}}
  const show=()=>{render();panel.hidden=false;};
  button.onclick=show;close.onclick=()=>{panel.hidden=true;};
  copy.onclick=async()=>{try{await navigator.clipboard.writeText(report());copy.textContent='Copied';}catch{copy.textContent='Select the details below to copy';}};
  panel.append(heading,status,copy,close,back,pre);document.body.append(button,panel);
  const progress=new BroadcastChannel('game-progress');
  progress.onmessage=({data:m})=>{
    if(m.startup||m.label){phase=m.startup||m.label;record(phase);}
    if(m.lines)for(const line of m.lines.slice(-3))record(line);
    if(m.error){failure ||= m.error;record(m.error);show();}
    if(m.world){world=true;button.hidden=true;panel.hidden=true;}
  };
  const crashes=new BroadcastChannel('game-crash');
  crashes.onmessage=({data:m})=>{const detail=String(m.text||'Engine worker failed');failure ||= detail;record('Heap: '+m.heap+' MiB; '+detail);button.hidden=false;show();};
  addEventListener('error',e=>{failure ||= e.message;record(e.message);show();});
  addEventListener('unhandledrejection',e=>{const detail=String(e.reason?.message||e.reason);failure ||= detail;record(detail);show();});
  return {stall(seconds){if(world||failure)return;record('No engine activity for '+seconds+' seconds; cause not yet known.');show();},record};
}
