const test=require('node:test'),assert=require('node:assert/strict'),{createHash}=require('node:crypto');
test('bundled shader loader validates metadata, SQLite header, size and digest before automatic preparation',async()=>{
 const {loadBundledPipelineSeed}=await import('./bundled-pipeline-seed.mjs');
 const bytes=Buffer.alloc(128);bytes.write('SQLite format 3\0');
 const metadata={version:1,scope:'discovered-pipelines-only',pipelineCount:1,bytes:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex')};
 const load=(meta=metadata,data=bytes)=>loadBundledPipelineSeed({baseURL:'https://lobby.example/',fetch:async url=>new Response(url.pathname.endsWith('.json')?JSON.stringify(meta):data)});
 assert.equal((await load()).sha256,metadata.sha256);
 await assert.rejects(load({...metadata,scope:'all-scenes'}),/Invalid bundled/);
 await assert.rejects(load({...metadata,bytes:129}),/size differs/);
 const corrupted=Buffer.from(bytes);corrupted[90]=1;await assert.rejects(load(metadata,corrupted),/integrity/);
 const badHeader=Buffer.from(bytes);badHeader[0]=0;await assert.rejects(load(metadata,badHeader),/database/);
});
