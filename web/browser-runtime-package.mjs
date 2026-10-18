// Download the browser executable pair and explicitly enumerated shared-package
// artifacts/resources. Native entries are measured, never executed in browser.
// Game assets remain in the verified local
// store and are mounted by prepareFingerprintedSessionRuntime. The returned
// immutable blobs must pass that fingerprinted preparation boundary before use.
export async function loadSharedRuntimeInventory({baseURL,signal,fetch:request=globalThis.fetch}={}){
 const response=await request(new URL('runtime-package.json',new URL('./',baseURL)),{signal,cache:'no-store',redirect:'error',credentials:'same-origin'});
 if(response.status===404){await response.body?.cancel().catch(()=>{});return null;}if(!response.ok||!response.body){await response.body?.cancel().catch(()=>{});throw Error('Shared runtime inventory is unavailable.');}
 const reader=response.body.getReader(),parts=[];let size=0,complete=false;
 try{while(true){signal?.throwIfAborted();const {done,value}=await reader.read();if(done){complete=true;break;}size+=value.byteLength;if(size>256*1024)throw Error('Shared runtime inventory exceeds its size limit.');parts.push(value);}}
 finally{if(!complete)await reader.cancel().catch(()=>{});reader.releaseLock();}
 const value=JSON.parse(await new Blob(parts).text());signal?.throwIfAborted();
 if(value.version!==1||value.profile!=='unlocked-no-save-v1'||! /^[a-f0-9]{64}$/.test(value.buildId)||!Array.isArray(value.runtimeArtifacts)||value.runtimeArtifacts.length>254)throw Error('Invalid shared runtime inventory.');
 const names=new Set(['contract.mjs','contract.wasm']);for(const name of value.runtimeArtifacts){if(typeof name!=='string'||name.length>512||! /^[a-zA-Z0-9_.-]+(?:\/[a-zA-Z0-9_.-]+)*$/.test(name)||name.split('/').some(part=>part==='.'||part==='..')||names.has(name))throw Error('Invalid shared runtime artifact name.');names.add(name);}
 return Object.freeze({buildId:value.buildId,runtimeArtifacts:Object.freeze([...value.runtimeArtifacts])});
}
export async function loadBrowserRuntimePackage({baseURL,signal,fetch:request=globalThis.fetch,supplementalArtifacts=[]}={}) {
  if(typeof request!=='function')throw Error('Runtime package fetch required.');
  if(!Array.isArray(supplementalArtifacts)||supplementalArtifacts.length>254)
    throw Error('Bounded supplemental runtime artifact list required.');
  const names=new Set(['contract.mjs','contract.wasm']);
  const extras=supplementalArtifacts.map(name=>{
    if(typeof name!=='string'||!/^[a-zA-Z0-9_.-]+(?:\/[a-zA-Z0-9_.-]+)*$/.test(name)||
      name.split('/').some(part=>part==='.'||part==='..')||names.has(name))throw Error('Invalid supplemental runtime artifact.');
    names.add(name);return name;
  });
  const base=new URL(baseURL??'./',globalThis.location?.href);
  if(!['http:','https:'].includes(base.protocol)||base.username||base.password||base.search||base.hash)
    throw Error('Runtime package HTTP directory required.');
  if(!base.pathname.endsWith('/'))throw Error('Runtime package directory required.');
  signal?.throwIfAborted();
  const abort=new AbortController();
  const cancel=()=>abort.abort(signal.reason);
  signal?.addEventListener('abort',cancel,{once:true});
  const read=async(name,limit,type)=>{
    const url=new URL(name,base);
    const response=await request(url.href,{signal:abort.signal,redirect:'error',credentials:'same-origin',cache:'no-store'});
    if(!response.ok||!response.body){await response.body?.cancel().catch(()=>{});throw Error('Runtime artifact unavailable: '+name);}
    const reader=response.body.getReader(),parts=[];let size=0,complete=false;
    try {
      abort.signal.throwIfAborted();
      const length=response.headers.get('content-length');
      if(length!==null&&(!/^\d+$/.test(length)||Number(length)>limit))throw Error('Runtime artifact exceeds package bound: '+name);
      while(true) {
        abort.signal.throwIfAborted();
        const {done,value}=await reader.read();
        abort.signal.throwIfAborted();
        if(done){complete=true;break;}
        size+=value.byteLength;
        if(size>limit)throw Error('Runtime artifact exceeds package bound: '+name);
        parts.push(value);
      }
      if(!size)throw Error('Empty runtime artifact: '+name);
      return {name,blob:new Blob(parts,{type})};
    } finally {
      if(!complete)await reader.cancel().catch(()=>{});
      reader.releaseLock();
    }
  };
  try {
    // Wait for both operations to settle before releasing their cancellation
    // owner; a failed sibling must not leave a large Wasm fetch running.
    const results=await Promise.allSettled([
      read('contract.mjs',16*1024**2,'text/javascript'),
      read('contract.wasm',256*1024**2,'application/wasm'),
      ...extras.map(name=>read(name,1024**3,'application/octet-stream')),
    ].map(operation=>operation.catch(error=>{abort.abort(error);throw error;})));
    const failed=results.find(result=>result.status==='rejected');
    if(failed)throw failed.reason;
    signal?.throwIfAborted();
    return results.map(result=>result.value);
  } finally {signal?.removeEventListener('abort',cancel);}
}
