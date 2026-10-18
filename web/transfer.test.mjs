import {test} from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {prepare,host,guest,CHUNK,decodeChunk,encodeChunk,validateManifest} from './transfer.mjs';

class Channel extends EventTarget {
  readyState = 'connecting';
  sent = 0;
  send(data) {
    if(this.readyState !== 'open') throw Error('Closed');
    this.sent++;
    queueMicrotask(()=>{if(this.peer.readyState==='open')this.peer.dispatchEvent(new MessageEvent('message',{data}));});
  }
  close() { if(this.readyState==='closed')return;this.readyState='closed';this.dispatchEvent(new Event('close'));this.peer.close(); }
}
function pair() {
  const a=new Channel(),b=new Channel(); a.peer=b;b.peer=a;
  return [a,b,()=>{a.readyState=b.readyState='open';a.dispatchEvent(new Event('open'));b.dispatchEvent(new Event('open'));}];
}
class Store {
  values = new Map(); done = null;
  async get(id,i){return this.values.get(`${id}:${i}`);}
  async put(id,i,value){this.values.set(`${id}:${i}`,value);}
  async complete(m){this.done=m;}
}
const file = new Blob([Uint8Array.from({length:CHUNK*35+17},(_,i)=>i%251)]);
test('manifest identity remains ordered with bounded out-of-order file reads',async()=>{
  let active=0,maximum=0;
  const source={size:file.size,name:'asset',slice(start,end){return {async arrayBuffer(){
    active++;maximum=Math.max(maximum,active);
    try{await new Promise(resolve=>setTimeout(resolve,(7-(start/CHUNK)%8)*2));return file.slice(start,end).arrayBuffer();}
    finally{active--;}
  }};}};
  const result=await prepare(source);
  const expected=[];
  for(let start=0;start<file.size;start+=CHUNK)expected.push(createHash('sha256').update(Buffer.from(await file.slice(start,start+CHUNK).arrayBuffer())).digest());
  const hashes=Buffer.concat(expected);
  assert.deepEqual(Buffer.from(result.hashes),hashes);
  assert.equal(result.manifest.id,createHash('sha256').update(hashes).digest('hex'));
  assert.ok(maximum>1&&maximum<=8);assert.equal(active,0);
});
test('abort during manifest reads rejects without publishing completion',async()=>{
  const controller=new AbortController(),progress=[];
  const source={size:file.size,slice(start,end){return {async arrayBuffer(){
    controller.abort();return file.slice(start,end).arrayBuffer();
  }};}};
  await assert.rejects(prepare(source,value=>progress.push(value),controller.signal),{name:'AbortError'});
  assert.deepEqual(progress,[]);
});
test('closing from final manifest progress does not publish an asset',async()=>{
  const controller=new AbortController();
  await assert.rejects(prepare(new Blob(['asset']),value=>{
    if(value===1)controller.abort();
  },controller.signal),{name:'AbortError'});
});
async function transfer(asset,store) {
  const [a,b,open]=pair();
  const result = new Promise((resolve,reject)=>{
    const g=guest(b,store,{offer:()=>g.accept().catch(reject),complete:resolve,error:reject});
    host(a,asset,reject); open();
  });
  await result;
  return a;
}
test('multi-window transfer validates and persists exact bytes',async()=>{
  const asset=await prepare(file),store=new Store();
  await transfer(asset,store);
  assert.equal(store.done.id,asset.manifest.id);
  const parts=[];
  for(let i=0;i<asset.manifest.count;i++)parts.push((await store.get(asset.manifest.id,i)).bytes);
  assert.deepEqual(new Uint8Array(await new Blob(parts).arrayBuffer()),new Uint8Array(await file.arrayBuffer()));
});
test('resumes a verified prefix and repairs a damaged cached chunk',async()=>{
  const asset=await prepare(file),store=new Store();
  await transfer(asset,store);
  store.values.get(`${asset.manifest.id}:5`).bytes=new ArrayBuffer(4);
  const channel=await transfer(asset,store);
  assert.equal(channel.sent,1+asset.manifest.count-5);
  assert.equal((await store.get(asset.manifest.id,5)).bytes.byteLength,CHUNK);
});
test('rejects altered data, wrong chunk index and oversized manifests',async()=>{
  const asset=await prepare(file),bytes=await file.slice(0,CHUNK).arrayBuffer();
  const packet=encodeChunk(0,asset.hashes.subarray(0,32),bytes);
  new Uint8Array(packet)[45]^=1;
  await assert.rejects(decodeChunk(packet,asset.manifest,0),/integrity/);
  await assert.rejects(decodeChunk(packet,asset.manifest,1),/range/);
  assert.throws(()=>validateManifest({...asset.manifest,size:3*1024**3}),/manifest/);
});
test('rejects whole-asset hash mismatch despite valid individual chunks',async()=>{
  const asset=await prepare(file); asset.manifest.id='0'.repeat(64);
  await assert.rejects(transfer(asset,new Store()),/Whole-asset/);
});
test('cancels mid-transfer without marking complete, then resumes',async()=>{
  const asset=await prepare(file),store=new Store();
  const [a,b,open]=pair();
  await new Promise((resolve,reject)=>{
    const g=guest(b,store,{offer:()=>g.accept().catch(reject),error:reject,
      progress:bytes=>{if(bytes>=CHUNK*3){g.cancel();resolve();}}});
    host(a,asset,reject);open();
  });
  assert.equal(store.done,null);
  assert.equal(store.values.size,3);
  const resumed=await transfer(asset,store);
  assert.equal(resumed.sent,1+asset.manifest.count-3);
  assert.equal(store.done.id,asset.manifest.id);
});

test('host observes verified progress and completion, including fully cached resume',async()=>{
  const asset=await prepare(file),store=new Store();
  for(let run=0;run<2;run++) {
    const [a,b,open]=pair(),updates=[];
    await new Promise((resolve,reject)=>{
      const g=guest(b,store,{offer:()=>g.accept().catch(reject),error:reject});
      host(a,asset,reject,resolve,(bytes,total)=>updates.push([bytes,total])); open();
    });
    assert.deepEqual(updates.at(-1),[file.size,file.size]);
    assert.ok(updates.every(([bytes,total],i)=>total===file.size && bytes>=0 && bytes<=total && (!i || bytes>=updates[i-1][0])));
    if(run===0) assert.ok(updates.some(([bytes])=>bytes>0 && bytes<file.size));
    else assert.equal(updates.length,1);
  }
});

test('cancel during pending cached read suppresses late progress and completion',async()=>{
  const asset=await prepare(new Blob([new Uint8Array([1,2,3])])),[a,b,open]=pair();
  let release,started,accepting;const reading=new Promise(resolve=>{started=resolve;}),updates=[];
  const store={get:()=>{started();return new Promise(resolve=>{release=resolve;});},
    complete:()=>{throw Error('Cancelled transfer must not complete');}};
  const errors=[],g=guest(b,store,{offer:()=>{accepting=g.accept();},progress:bytes=>updates.push(bytes),error:e=>errors.push(e)});
  host(a,asset,e=>errors.push(e));open();await reading;g.cancel();release(undefined);await accepting;
  assert.deepEqual(updates,[]);assert.deepEqual(errors,[]);assert.equal(b.sent,0);
  await g.accept();assert.deepEqual(updates,[]);
});

test('host ignores queued completion acknowledgement after connection closes',async()=>{
  const asset=await prepare(new Blob([new Uint8Array([1,2,3])])),[a,b,open]=pair();
  const errors=[],updates=[];let completed=0;
  host(a,asset,e=>errors.push(e),()=>completed++,(bytes)=>updates.push(bytes));open();
  // Deliver before close but let the serialized async handler run afterward.
  a.dispatchEvent(new MessageEvent('message',{data:JSON.stringify({type:'complete',id:asset.manifest.id})}));
  b.close();await new Promise(resolve=>setImmediate(resolve));
  assert.equal(completed,0);assert.deepEqual(updates,[]);assert.deepEqual(errors,[]);
});

test('closing from host progress callback suppresses completion callback',async()=>{
  const asset=await prepare(new Blob([new Uint8Array([1,2,3])])),[a,b,open]=pair();
  let completed=0;const errors=[];
  host(a,asset,e=>errors.push(e),()=>completed++,()=>b.close());open();
  a.dispatchEvent(new MessageEvent('message',{data:JSON.stringify({type:'complete',id:asset.manifest.id})}));
  await new Promise(resolve=>setImmediate(resolve));assert.equal(completed,0);assert.deepEqual(errors,[]);
  a.dispatchEvent(new Event('open'));assert.equal(a.sent,1);
});
