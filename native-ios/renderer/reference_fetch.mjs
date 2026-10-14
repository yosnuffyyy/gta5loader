// CI-only transport resilience. Integrity checks stay at the pinned caller.
export async function readReference(url,{fetchImpl=fetch,delay=ms=>new Promise(r=>setTimeout(r,ms)),attempts=3}={}){
 if(!Number.isInteger(attempts)||attempts<1||attempts>3)throw Error('Reference retry bound must be 1..3');
 for(let attempt=0;attempt<attempts;attempt++){
  try {const response=await fetchImpl(url,{signal:AbortSignal.timeout(45000)});if(!response.ok){const error=Error('Reference HTTP '+response.status);error.retryable=response.status===408||response.status===429||response.status>=500;if(response.body)await response.body.cancel();throw error;}return Buffer.from(await response.arrayBuffer());}
  catch(error){const retry=error.retryable===true||error instanceof TypeError||error.name==='AbortError'||error.name==='TimeoutError';if(!retry||attempt+1===attempts)throw error;await delay(500*2**attempt);}
 }
 throw Error('Unreachable reference retry state');
}
