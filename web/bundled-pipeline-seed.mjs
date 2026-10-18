export async function loadBundledPipelineSeed({baseURL,signal,fetch=globalThis.fetch}){
 const root=new URL('./',baseURL),read=async name=>{const response=await fetch(new URL(name,root),{signal,cache:'no-store'});if(!response.ok)throw Error('The lobby website is missing its bundled shader definitions.');return response;};
 const metadata=await(await read('initial_pipeline_cache.json')).json();
 if(metadata.version!==1||metadata.scope!=='discovered-pipelines-only'||!Number.isSafeInteger(metadata.pipelineCount)||metadata.pipelineCount<1||!Number.isSafeInteger(metadata.bytes)||metadata.bytes<100||metadata.bytes>32*1024**2||!/^[a-f0-9]{64}$/.test(metadata.sha256))throw Error('Invalid bundled shader description.');
 const response=await read('initial_pipeline_cache.db'),declared=Number(response.headers.get('content-length'));
 if(declared&&declared!==metadata.bytes)throw Error('Bundled shader definition size differs.');
 const blob=await response.blob();signal?.throwIfAborted();if(blob.size!==metadata.bytes)throw Error('Bundled shader definition size differs.');
 const bytes=new Uint8Array(await blob.arrayBuffer());
 if(!new TextEncoder().encode('SQLite format 3\0').every((b,i)=>bytes[i]===b))throw Error('Invalid bundled shader database.');
 const digest=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),b=>b.toString(16).padStart(2,'0')).join('');signal?.throwIfAborted();
 if(digest!==metadata.sha256)throw Error('Bundled shader definitions failed integrity verification.');
 return Object.freeze({blob,sha256:digest});
}
