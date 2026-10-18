import {test} from 'node:test';
import assert from 'node:assert/strict';
import {prepare, CHUNK} from './transfer.mjs';
import {verifiedAssetBlob} from './runtime-asset.mjs';

async function fixture() {
  const bytes = Uint8Array.from({length:CHUNK*5+7}, (_,i)=>i%251);
  const asset = await prepare(new Blob([bytes]));
  const chunks = [];
  for(let i=0;i<asset.manifest.count;i++) chunks.push({bytes:bytes.slice(i*CHUNK,(i+1)*CHUNK).buffer});
  return {bytes, asset, chunks, store:{manifest:async()=>asset.manifest,get:async(_,i)=>chunks[i]}};
}
test('verified handoff produces exact immutable bytes and reports progress',async()=>{
  const f=await fixture(), progress=[];
  const result=await verifiedAssetBlob(f.store,f.asset.manifest.id,{progress:(a,b)=>progress.push([a,b])});
  f.chunks[0].bytes=new ArrayBuffer(CHUNK);
  assert.deepEqual(new Uint8Array(await result.blob.arrayBuffer()),f.bytes);
  assert.deepEqual(progress.at(-1),[f.bytes.length,f.bytes.length]);
});
test('partial downloads and missing chunks cannot be mounted',async()=>{
  const f=await fixture();
  await assert.rejects(verifiedAssetBlob({...f.store,manifest:async()=>null},f.asset.manifest.id),/not complete/);
  f.chunks.pop();
  await assert.rejects(verifiedAssetBlob(f.store,f.asset.manifest.id),/incomplete/);
});
test('changed bytes and mismatched manifest are rejected',async()=>{
  const f=await fixture();
  new Uint8Array(f.chunks[0].bytes)[0]^=1;
  await assert.rejects(verifiedAssetBlob(f.store,f.asset.manifest.id),/integrity/);
  await assert.rejects(verifiedAssetBlob(f.store,'0'.repeat(64)),/identity mismatch/);
});

test('corruption after the first hash batch cannot be mounted',async()=>{
  const f=await fixture();
  new Uint8Array(f.chunks[4].bytes)[0]^=1;
  await assert.rejects(verifiedAssetBlob(f.store,f.asset.manifest.id),/integrity/);
});
test('cancellation never returns a mountable blob',async()=>{
  const f=await fixture(), controller=new AbortController();
  await assert.rejects(verifiedAssetBlob(f.store,f.asset.manifest.id,{signal:controller.signal,progress:()=>controller.abort()}),{name:'AbortError'});
});


test('leaving while storage is pending does not read chunks or publish stale progress',async()=>{
  for(const boundary of ['manifest','get']) {
    const f=await fixture(), controller=new AbortController();
    let release,entered;
    const waiting=new Promise(resolve=>entered=resolve);
    const hold=new Promise(resolve=>release=resolve);
    let reads=0,progress=0;
    const store={
      async manifest(){if(boundary==='manifest'){entered();await hold;}return f.asset.manifest;},
      async get(_,i){reads++;if(boundary==='get'){entered();await hold;}return f.chunks[i];}
    };
    const result=verifiedAssetBlob(store,f.asset.manifest.id,{signal:controller.signal,progress:()=>progress++});
    const rejected=assert.rejects(result,{name:'AbortError'});
    await waiting;controller.abort();release();await rejected;
    assert.equal(reads,boundary==='manifest'?0:1);
    assert.equal(progress,0);
  }
});
