import test from 'node:test';
import assert from 'node:assert/strict';
import {prepare,CHUNK} from './transfer.mjs';
import {verifiedAssetBlob} from './runtime-asset.mjs';

const bytes=Uint8Array.from({length:32*1024*1024},(_,i)=>i%251);
const source=new Blob([bytes]),asset=await prepare(source);
function fixture(){
 let written=0,disposed=0,finished=0;
 const writes=[];
 const store={manifest:async()=>asset.manifest,get:async(_,index)=>({bytes:await source.slice(index*CHUNK,(index+1)*CHUNK).arrayBuffer()})};
 const file={write:async chunk=>{assert.deepEqual(new Uint8Array(chunk),bytes.subarray(written,written+chunk.byteLength));writes.push(chunk.byteLength);written+=chunk.byteLength;},finish:async()=>{finished++;return source;},dispose:async()=>{disposed++;}};
 return {store,file,writes,createFile:async()=>file,stats:()=>({written,disposed,finished})};
}
test('large cached asset writes verified bytes in order and retains file until disposal',async()=>{
 const f=fixture(),result=await verifiedAssetBlob(f.store,asset.manifest.id,f);
 assert.equal(result.blob,source);assert.deepEqual(f.stats(),{written:bytes.length,disposed:0,finished:1});
 assert.equal(f.writes.length,Math.ceil(asset.manifest.count/4));
 assert.ok(f.writes.every(size=>size>0&&size<=4*CHUNK));
 assert.equal(f.writes.at(-1),bytes.length%(4*CHUNK));
 await result.dispose();assert.equal(f.stats().disposed,1);
});

test('cancellation during a pending file batch starts no further reads or progress',async()=>{
 const f=fixture(),controller=new AbortController();let release,entered,reads=0,progress=0;
 const writing=new Promise(resolve=>entered=resolve),hold=new Promise(resolve=>release=resolve),get=f.store.get;
 f.store.get=async(...args)=>{reads++;return get(...args);};
 f.file.write=async()=>{entered();await hold;};
 const result=verifiedAssetBlob(f.store,asset.manifest.id,{...f,signal:controller.signal,progress:()=>progress++});
 const rejected=assert.rejects(result,{name:'AbortError'});
 await writing;controller.abort();release();await rejected;
 assert.equal(reads,4);assert.equal(progress,0);assert.equal(f.stats().disposed,1);assert.equal(f.stats().finished,0);
});
test('file write failure and cancellation dispose unfinished storage',async()=>{
 const f=fixture();f.file.write=async()=>{throw Error('Disk full');};
 await assert.rejects(verifiedAssetBlob(f.store,asset.manifest.id,f),/Disk full/);
 assert.deepEqual(f.stats(),{written:0,disposed:1,finished:0});
 const g=fixture(),controller=new AbortController();
 await assert.rejects(verifiedAssetBlob(g.store,asset.manifest.id,{...g,signal:controller.signal,progress:()=>controller.abort()}),{name:'AbortError'});
 assert.equal(g.stats().disposed,1);assert.equal(g.stats().finished,0);
});
test('cancellation after file close still removes the completed temporary file',async()=>{
 const f=fixture(),controller=new AbortController();f.file.finish=async()=>{controller.abort();return source;};
 await assert.rejects(verifiedAssetBlob(f.store,asset.manifest.id,{...f,signal:controller.signal}),{name:'AbortError'});
 assert.equal(f.stats().disposed,1);
});

test('whole-asset mismatch removes written bytes without publishing a file',async()=>{
 const f=fixture(),wrongId='1'.repeat(64);
 f.store.manifest=async()=>({...asset.manifest,id:wrongId});
 await assert.rejects(verifiedAssetBlob(f.store,wrongId,f),/integrity/);
 assert.deepEqual(f.stats(),{written:bytes.length,disposed:1,finished:0});
});
