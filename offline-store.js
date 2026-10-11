let dbPromise;
function db() {
  return dbPromise ||= new Promise((resolve,reject)=>{
    const request=indexedDB.open('gta-mobile-install-v1',1);
    request.onupgradeneeded=()=>{const store=request.result.createObjectStore('files',{keyPath:'key'});store.createIndex('manifest','manifest');};
    request.onsuccess=()=>resolve(request.result);request.onerror=()=>reject(request.error);
  });
}
export async function records(manifest) {
  const database=await db();
  return new Promise((resolve,reject)=>{
    const req=database.transaction('files').objectStore('files').index('manifest').getAll(manifest.dataId||manifest.id);
    req.onsuccess=()=>resolve(new Map(req.result.map(row=>[row.index,row])));req.onerror=()=>reject(req.error);
  });
}
export async function saveRecord(manifest,file,bytes,complete=false) {
  const database=await db();
  return new Promise((resolve,reject)=>{
    const tx=database.transaction('files','readwrite');
    const dataId=manifest.dataId||manifest.id;
    tx.objectStore('files').put({key:dataId+':'+file.index,manifest:dataId,index:file.index,sha256:file.sha256,size:file.size,bytes,complete});
    tx.oncomplete=()=>resolve();tx.onerror=()=>reject(tx.error);tx.onabort=()=>reject(tx.error||Error('Storage transaction was interrupted.'));
  });
}
export function validBytes(file,row) {
  if(!row||row.sha256!==file.sha256||row.size!==file.size||!Number.isSafeInteger(row.bytes)||row.bytes<0||row.bytes>file.size)return 0;
  return row.bytes;
}
export function summarize(manifest,rows) {
  let bytes=0,complete=0,requiredMissing=0,optionalMissing=0;
  const missingFiles=[];
  for(const file of manifest.files){
    const row=rows.get(file.index),n=validBytes(file,row);bytes+=n;
    if(row?.complete&&n===file.size&&row.size===file.size&&row.bytes===file.size&&row.sha256===file.sha256)complete++;
    else{
      if(file.optional===true)optionalMissing++;else requiredMissing++;
      if(missingFiles.length<30)missingFiles.push({path:file.path,optional:file.optional===true});
    }
  }
  return {bytes,complete,total:manifest.files.length,totalBytes:manifest.totalBytes,ready:requiredMissing===0,fullReady:complete===manifest.files.length,requiredMissing,optionalMissing,missingFiles};
}
export async function dataDirectory(manifest,create=false) {
  const root=await navigator.storage.getDirectory();
  const app=await root.getDirectoryHandle('gta-mobile-offline',{create});
  return app.getDirectoryHandle(manifest.dataId||manifest.id,{create});
}
export function fileName(file){return file.index+'.bin';}
