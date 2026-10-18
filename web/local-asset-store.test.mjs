import test from 'node:test';
import assert from 'node:assert/strict';
import {prepare,CHUNK} from './transfer.mjs';
import {localAssetStore} from './local-asset-store.mjs';
import {verifiedAssetBlob} from './runtime-asset.mjs';
test('local selected file passes the same whole-asset verification boundary',async()=>{
  const bytes=Uint8Array.from({length:CHUNK+3},(_,i)=>i%251),asset=await prepare(new Blob([bytes]));
  const store=localAssetStore(asset),id=asset.manifest.id;
  bytes.fill(0);const manifest=await store.manifest(id);manifest.size=1;
  const verified=await verifiedAssetBlob(store,id);
  assert.equal(verified.blob,store.immutableBlob,'Mount the exact snapshot whose bytes were verified.');
  assert.equal(verified.blob.size,CHUNK+3);
  assert.equal(new Uint8Array(await verified.blob.arrayBuffer())[252],1);
  await assert.rejects(store.get(id,2),/Invalid/);
  await assert.rejects(store.manifest('wrong'),/Unknown/);
});

test('immutable snapshot is verified directly instead of trusting store chunk reads',async()=>{
  const bytes=Uint8Array.from({length:CHUNK+3},(_,i)=>i%251);
  const asset=await prepare(new Blob([bytes])),id=asset.manifest.id;
  const store=localAssetStore(asset);
  store.get=()=>{throw Error('Snapshot verification must read its own bytes.');};
  assert.equal((await verifiedAssetBlob(store,id)).blob,store.immutableBlob);
  const corrupt=bytes.slice();corrupt[CHUNK]^=1;
  await assert.rejects(verifiedAssetBlob({...store,immutableBlob:new Blob([corrupt])},id),/integrity/);
  await assert.rejects(verifiedAssetBlob({...store,immutableBlob:new Blob([bytes.slice(1)])},id),/snapshot/);
});

test('cancelling immutable snapshot verification never returns a mountable asset',async()=>{
  const asset=await prepare(new Blob([new Uint8Array(CHUNK*5)]));
  const controller=new AbortController();
  await assert.rejects(verifiedAssetBlob(localAssetStore(asset),asset.manifest.id,{
    signal:controller.signal,progress:()=>controller.abort(),
  }),{name:'AbortError'});
});
