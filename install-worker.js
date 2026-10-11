import {sha256,folderPath} from './offline-common.js';
import {records,saveRecord,validBytes,summarize,dataDirectory,fileName} from './offline-store.js';
let running=false,aborter;
const paused=()=>{if(aborter?.signal.aborted)throw new DOMException('Installation paused.','AbortError');};
async function fetchChunk(manifest,file,start,end) {
  for(let attempt=0;attempt<3;attempt++){
    paused();
    try{
      const url=new URL(file.path,self.location.origin);url.searchParams.set('__install',manifest.id);
      const response=await fetch(url,{cache:'no-store',headers:{Range:`bytes=${start}-${end}`},signal:aborter.signal});
      if(response.status!==206 && !(response.status===200&&start===0&&end+1===file.size)) {await response.body?.cancel();throw Error('The setup server must support byte ranges: '+file.path);}
      if(response.status===206 && response.headers.get('Content-Range')!==`bytes ${start}-${end}/${file.size}`){await response.body?.cancel();throw Error('Incorrect byte range: '+file.path);}
      // Stream into a bounded buffer; a wrong server cannot make us buffer an archive.
      const bytes=new Uint8Array(end-start+1),reader=response.body.getReader();let offset=0;
      try{for(;;){const {value,done}=await reader.read();if(done)break;if(offset+value.length>bytes.length)throw Error('Oversized download chunk');bytes.set(value,offset);offset+=value.length;}}
      finally{await reader.cancel().catch(()=>{});reader.releaseLock();}
      if(offset!==bytes.length)throw Error('Incomplete download chunk');return bytes;
    }catch(e){if(aborter.signal.aborted||attempt===2)throw e;await new Promise(r=>setTimeout(r,500*(attempt+1)));}
  }
}
async function install(manifest,localFiles) {
  const dir=await dataDirectory(manifest,true),rows=await records(manifest);
  const local=localFiles?new Map(localFiles.map(f=>[folderPath(f),f])):null;
  let last=0,current='';
  function progress(force=false){if(force||performance.now()-last>150){postMessage({type:'progress',...summarize(manifest,rows),current});last=performance.now();}}
  const checkpoint=async(file,bytes,complete)=>{
    await saveRecord(manifest,file,bytes,complete);
    rows.set(file.index,{sha256:file.sha256,size:file.size,bytes,complete});progress();
  };
  progress(true);
  let next=0;
  async function run(){
    while(next<manifest.files.length){
      paused();const file=manifest.files[next++];current=file.path;
      const handle=await dir.getFileHandle(fileName(file),{create:true});
      const stored=await handle.getFile(),row=rows.get(file.index);
      let offset=validBytes(file,row);
      if(row?.complete&&row.sha256===file.sha256&&row.size===file.size&&row.bytes===file.size&&offset===file.size&&stored.size===file.size){progress();continue;}
      if(stored.size<offset || (offset!==file.size && offset%manifest.chunkBytes!==0))offset=0;
      const source=local?.get(file.path);
      if(local&&(!source||source.size!==file.size))throw Error('Choose the complete web folder. Missing or wrong-sized file: '+file.path);
      const access=await handle.createSyncAccessHandle();
      try{
        access.truncate(offset);access.flush();await checkpoint(file,offset,false);
        while(offset<file.size){
          paused();const end=Math.min(file.size,offset+manifest.chunkBytes),chunkIndex=offset/manifest.chunkBytes;
          const bytes=source?new Uint8Array(await source.slice(offset,end).arrayBuffer()):await fetchChunk(manifest,file,offset,end-1);
          if(await sha256(bytes)!==file.chunks[chunkIndex])throw Error('File verification failed: '+file.path+' (chunk '+(chunkIndex+1)+'). The saved verified portion is safe to resume.');
          paused();let written=0;
          while(written<bytes.length){const n=access.write(bytes.subarray(written),{at:offset+written});if(!n)throw Error('Storage write failed');written+=n;}
          offset=end;access.flush();await checkpoint(file,offset,false);
        }
        if(file.size===0&&await sha256(new Uint8Array())!==file.sha256)throw Error('Invalid empty-file checksum');
        access.truncate(file.size);access.flush();
      }finally{access.close();}
      await checkpoint(file,file.size,true);
    }
  }
  // Four bounded 4 MiB transfers, with a durable checkpoint after every chunk.
  let error;
  await Promise.all(Array.from({length:4},()=>run().catch(e=>{error ||= e;aborter.abort();})));
  progress(true);if(error)throw error;
  const summary=summarize(manifest,rows);if(!summary.ready)throw Error('Installation is incomplete. Resume to finish.');
  postMessage({type:'complete',...summary});
}
self.onmessage=async({data})=>{
  if(data.command==='pause'){aborter?.abort();return;}
  if(running)return;
  running=true;aborter=new AbortController();
  try{
    const manifest=await (await fetch('/gta5loader/offline-manifest.json')).json();
    if(data.command==='status'){postMessage({type:'status',...summarize(manifest,await records(manifest))});return;}
    const work=()=>install(manifest,data.files || null);
    if(navigator.locks){await navigator.locks.request('gta-mobile-install-'+(manifest.dataId||manifest.id),{ifAvailable:true},lock=>{if(!lock)throw Error('Another installation is open. Close it before continuing here.');return work();});}
    else await work();
  }catch(e){postMessage({type:e.name==='AbortError'?'paused':'error',message:e.name==='QuotaExceededError'?'The iPhone/browser ran out of storage. Free space, then resume; verified files are retained.':e.message});}
  finally{running=false;}
};
