// CI-only transport resilience. Integrity checks stay at the pinned caller.
async function request(url,read,{fetchImpl=fetch,delay=ms=>new Promise(r=>setTimeout(r,ms)),attempts=3,headers={}}={}){
 if(!Number.isInteger(attempts)||attempts<1||attempts>3)throw Error('Reference retry bound must be 1..3');
 for(let attempt=0;attempt<attempts;attempt++){
  try {const response=await fetchImpl(url,{headers,signal:AbortSignal.timeout(45000)});if(!response.ok){const error=Error('Reference HTTP '+response.status);error.retryable=response.status===408||response.status===429||response.status>=500;if(response.body)await response.body.cancel();throw error;}return await read(response);}
  catch(error){const retry=error.retryable===true||error instanceof TypeError||error.name==='AbortError'||error.name==='TimeoutError';if(!retry||attempt+1===attempts)throw error;await delay(500*2**attempt);}
 }
 throw Error('Unreachable reference retry state');
}
export async function readReference(url,options={}){return request(url,async r=>Buffer.from(await r.arrayBuffer()),options);}
export async function readReferenceRanges(url,{expectedBytes=0,maxBytes=16*1024*1024,chunkBytes=256*1024,...options}={}){
 if(!Number.isSafeInteger(maxBytes)||maxBytes<1||maxBytes>16*1024*1024||!Number.isSafeInteger(expectedBytes)||expectedBytes<0||expectedBytes>maxBytes||!Number.isSafeInteger(chunkBytes)||chunkBytes<1||chunkBytes>1024*1024)throw Error('Reference range bound invalid');
 const chunks=[];let offset=0,total=expectedBytes;
 do {const end=total?Math.min(offset+chunkBytes,total)-1:offset+chunkBytes-1;
  const part=await request(url,async response=>{
   const match=/^bytes (\d+)-(\d+)\/(\d+)$/.exec(response.headers.get('content-range')||'');
   if(response.status!==206||!match){if(response.body)await response.body.cancel();throw Error('Reference server did not honor required byte range');}
   const [start,last,size]=match.slice(1).map(Number);
   if(![start,last,size].every(Number.isSafeInteger)||start!==offset||size<1||size>maxBytes||(total&&size!==total)||last!==Math.min(end,size-1)||last<start){if(response.body)await response.body.cancel();throw Error('Reference Content-Range mismatch');}
   const bytes=Buffer.from(await response.arrayBuffer());if(bytes.length!==last-start+1)throw Error('Reference range body length mismatch');return {bytes,total:size};
  },{...options,headers:{Range:'bytes='+offset+'-'+end}});
  total=part.total;chunks.push(part.bytes);offset+=part.bytes.length;
 }while(offset<total);
 return Buffer.concat(chunks,total);
}
