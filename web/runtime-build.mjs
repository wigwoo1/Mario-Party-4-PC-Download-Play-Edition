const hex=bytes=>Array.from(new Uint8Array(bytes),byte=>byte.toString(16).padStart(2,'0')).join('');
// Caller must enumerate every executable runtime/side-module artifact. This
// identifies supplied files; it does not authenticate their publisher.
export async function fingerprintRuntimeFiles(files,{signal}={}) {
  if(!Array.isArray(files)||files.length<1||files.length>256)throw Error('Runtime artifact list required.');
  const names=new Set();
  const entries=files.map(({name,blob})=>{
    if(typeof name!=='string'||!/^[a-zA-Z0-9_.-]+(?:\/[a-zA-Z0-9_.-]+)*$/.test(name)||
        name.split('/').some(part=>part==='.'||part==='..')||names.has(name)||
        !(blob instanceof Blob)||blob.size<1||blob.size>1024**3)throw Error('Invalid runtime artifact.');
    names.add(name);return {name,blob};
  }).sort((a,b)=>a.name<b.name?-1:a.name>b.name?1:0);
  const manifest=[];
  for(const {name,blob} of entries) {
    signal?.throwIfAborted();
    const bytes=await blob.arrayBuffer();signal?.throwIfAborted();
    const hash=hex(await crypto.subtle.digest('SHA-256',bytes));signal?.throwIfAborted();
    manifest.push({name,size:blob.size,sha256:hash});
  }
  const bytes=new TextEncoder().encode(JSON.stringify({version:1,profile:'unlocked-no-save-v1',files:manifest}));
  const buildId=hex(await crypto.subtle.digest('SHA-256',bytes));signal?.throwIfAborted();
  return {buildId,files:manifest};
}

// Browser loader for a supplied Emscripten wrapper/Wasm pair. Embedded or
// external side modules must still be included in the package manifest.
export async function prepareFingerprintedRuntimeFactory(files,{
  wrapperName='contract.mjs',wasmName='contract.wasm',signal,expectedBuildId,
  importWrapper=async blob=>{
    const url=URL.createObjectURL(blob);
    try{return await import(url);}finally{URL.revokeObjectURL(url);}
  },
}={}) {
  signal?.throwIfAborted();
  if(expectedBuildId!==undefined&&(typeof expectedBuildId!=='string'||!/^[a-f0-9]{64}$/.test(expectedBuildId)))
    throw Error('Valid expected runtime package identity required.');
  if(typeof importWrapper!=='function'||!Array.isArray(files))throw Error('Runtime wrapper importer required.');
  if(wrapperName===wasmName||files.some(file=>!(file?.blob instanceof Blob)))throw Error('Invalid runtime artifact.');
  // Native Blob snapshots keep the bytes immutable throughout hashing/loading.
  const snapshots=files.map(({name,blob})=>({name,blob:new Blob([blob],{type:name===wrapperName?'text/javascript':'application/octet-stream'})}));
  const identity=await fingerprintRuntimeFiles(snapshots,{signal});
  // An authenticated coordinator may pin the complete shared package before
  // executable import. Equality measures bytes; it does not prove compatibility.
  if(expectedBuildId!==undefined&&identity.buildId!==expectedBuildId)
    throw Error('Runtime package differs from agreed identity.');
  const wrapper=snapshots.find(file=>file.name===wrapperName),wasm=snapshots.find(file=>file.name===wasmName);
  if(!wrapper||!wasm)throw Error('Runtime wrapper and Wasm artifacts required.');
  signal?.throwIfAborted();
  const wasmBinary=new Uint8Array(await wasm.blob.arrayBuffer());signal?.throwIfAborted();
  const imported=await importWrapper(wrapper.blob);signal?.throwIfAborted();
  if(typeof imported?.default!=='function')throw Error('Runtime wrapper factory required.');
  return {...identity,createRuntime(options){
    signal?.throwIfAborted();
    options?.signal?.throwIfAborted();
    // Custom instantiation and added libraries can execute bytes outside this
    // package identity. Do not allow these overrides at the verified boundary.
    if(options?.instantiateWasm!==undefined||options?.wasm!==undefined||
        (options?.dynamicLibraries!==undefined&&
          (!Array.isArray(options.dynamicLibraries)||options.dynamicLibraries.length)))
      throw Error('Unfingerprinted runtime execution override.');
    // Each instance gets owned binary bytes; factories cannot mutate the stored
    // fingerprinted copy or substitute a separately fetched Wasm binary.
    let runtimeModule;
    const artifactUrls=new Map();
    const releaseArtifacts=()=>{
      for(const url of artifactUrls.values())URL.revokeObjectURL(url);
      artifactUrls.clear();
    };
    const moduleOptions={...options,wasmBinary:wasmBinary.slice(),locateFile(name,prefix){
      // Emscripten resolves a binary filename even when bytes are supplied.
      // Blob module URLs cannot resolve relative paths; this sentinel is never
      // fetched because wasmBinary supplies the corresponding binary directly.
      if(name===wasmName)return 'data:application/wasm,';
      // Side modules are embedded in the verified main Wasm's virtual FS.
      // If Emscripten's loader cannot resolve its embedded FS lookup, serve
      // that exact embedded file via an owned Blob URL as the fallback.
      // No path outside the virtual-FS root or non-Wasm artifact may escape.
      const match=/^\/?([A-Za-z0-9_.-]+\.wasm)$/.exec(name);
      const runtimeFS=runtimeModule?.FS??moduleOptions.FS;
      if(match&&runtimeFS){
        // Some game call sites use different filename case from the package
        // produced by the toolchain. Emscripten's virtual FS is case-sensitive,
        // so map only to a unique existing root entry with the same folded
        // name. The verified runtime FS remains the sole byte source.
        const requested=match[1].toLowerCase();
        const candidates=(runtimeFS.readdir?.('/')??[]).filter(entry=>
          /^[A-Za-z0-9_.-]+\.wasm$/i.test(entry)&&entry.toLowerCase()===requested);
        if(candidates.length>1)throw Error('Ambiguous embedded runtime artifact: '+name);
        const path='/'+(candidates[0]??match[1]);
        if(runtimeFS.analyzePath(path).exists){
          let url=artifactUrls.get(path);
          if(!url){
            if(typeof URL.createObjectURL!=='function')throw Error('Embedded runtime artifact URL support unavailable.');
            const bytes=runtimeFS.readFile(path,{encoding:'binary'});
            url=URL.createObjectURL(new Blob([bytes],{type:'application/wasm'}));
            artifactUrls.set(path,url);
          }
          return url;
        }
      }
      throw Error('Unmapped external runtime artifact: '+name);
    }};
    // Emscripten runs preRun and onRuntimeInitialized before callMain. The game
    // can request a side module from callMain's Asyncify work, before the
    // modularized factory promise resolves and exposes its returned Module.
    // Capture the generated Module at this earlier callback boundary so the
    // resolver can read only its verified embedded FS files during startup.
    const onRuntimeInitialized=moduleOptions.onRuntimeInitialized;
    moduleOptions.onRuntimeInitialized=function(...args){
      runtimeModule=this;
      return onRuntimeInitialized?.apply(this,args);
    };
    const result=imported.default(moduleOptions);
    const attach=module=>{
      runtimeModule=module;
      if(module&&typeof module==='object')Object.defineProperty(module,'partyboardReleaseRuntimeArtifacts',{
        configurable:true,value:releaseArtifacts,
      });
      return module;
    };
    return result&&typeof result.then==='function'?result.then(attach):attach(result);
  }};
}
