// A precompiled pipeline-config seed is not evidence of complete scene coverage.
// Validate immutable bytes before Aurora opens its bundled cache during setup.
export async function installPipelineSeed(module,seed,{signal}={}){
 if(!(seed?.blob instanceof Blob)||seed.blob.size<100||seed.blob.size>32*1024**2||
   typeof seed.sha256!=='string'||!/^[a-f0-9]{64}$/.test(seed.sha256)||
   typeof module?.FS?.writeFile!=='function'||typeof module?.FS?.unlink!=='function')throw Error('Valid pipeline seed and writable runtime FS required.');
 signal?.throwIfAborted();const bytes=new Uint8Array(await seed.blob.arrayBuffer());signal?.throwIfAborted();
 const header=new TextEncoder().encode('SQLite format 3\0');
 if(!header.every((byte,index)=>bytes[index]===byte))throw Error('Pipeline seed is not a SQLite database.');
 const digest=new Uint8Array(await crypto.subtle.digest('SHA-256',bytes));signal?.throwIfAborted();
 const hash=Array.from(digest,b=>b.toString(16).padStart(2,'0')).join('');
 if(hash!==seed.sha256)throw Error('Pipeline seed integrity mismatch.');
 const path='/initial_pipeline_cache.db';module.FS.writeFile(path,bytes);let removed=false;
 return {sha256:hash,size:bytes.length,scope:'seeded-pipelines-only',dispose(){if(removed)return;module.FS.unlink(path);removed=true;}};
}
// Call only after Aurora shutdown closes/checkpoints SQLite's WAL database.
export async function captureClosedPipelineSeed(module){
 const bytes=module.FS.readFile('/pipeline_cache.db').slice();
 if(bytes.length<100||bytes.length>32*1024**2||!new TextEncoder().encode('SQLite format 3\0').every((b,i)=>bytes[i]===b))throw Error('Invalid captured pipeline database.');
 const digest=new Uint8Array(await crypto.subtle.digest('SHA-256',bytes));
 return Object.freeze({blob:new Blob([bytes]),sha256:Array.from(digest,b=>b.toString(16).padStart(2,'0')).join(''),scope:'observed-scenes-only'});
}
