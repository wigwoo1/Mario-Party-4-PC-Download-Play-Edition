const {test}=require('node:test');
const assert=require('node:assert/strict');
const vm=require('node:vm');
const fs=require('node:fs');
const path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'async_blob_read.js'),'utf8');
function fixture(contents=new Blob([Uint8Array.from({length:16},(_,i)=>i)])) {
  const memory=new ArrayBuffer(512), stats={reads:0,bytes:0,largestRead:0};
  const stream={position:0,node:{contents,asyncBlob:stats}};
  let library;
  const context=vm.createContext({HEAPU8:new Uint8Array(memory),HEAPU32:new Uint32Array(memory),
    SYSCALLS:{getStreamFromFD:()=>stream}, Asyncify:{handleAsync:fn=>fn()},
    doReadv:()=>7, addToLibrary:value=>{library=value;}});
  vm.runInContext(source,context);
  return {context,stream,stats,async read(vectors) {
    vectors.forEach(([pointer,length],i)=>context.HEAPU32.set([pointer,length],32+2*i));
    const error=await library.fd_read(3,128,vectors.length,192);
    return {error,bytes:context.HEAPU32[48]};
  }};
}
test('vectored reads preserve ordering and advance position',async()=>{
  const f=fixture(); assert.deepEqual(await f.read([[0,3],[16,5]]),{error:0,bytes:8});
  assert.deepEqual([...f.context.HEAPU8.slice(0,3)],[0,1,2]);
  assert.deepEqual([...f.context.HEAPU8.slice(16,21)],[3,4,5,6,7]);
  assert.equal(f.stream.position,8);
});
test('short EOF stops before later vectors and repeated EOF returns zero',async()=>{
  const f=fixture(); f.stream.position=14; f.context.HEAPU8.fill(99,0,32);
  assert.deepEqual(await f.read([[0,4],[16,4]]),{error:0,bytes:2});
  assert.deepEqual([...f.context.HEAPU8.slice(0,4)],[14,15,99,99]);
  assert.equal(f.context.HEAPU8[16],99);
  assert.deepEqual(await f.read([[0,4]]),{error:0,bytes:0});
  assert.equal(f.stream.position,16);
});
test('zero vectors and zero-length reads do not move position',async()=>{
  const f=fixture(); assert.deepEqual(await f.read([]),{error:0,bytes:0});
  assert.deepEqual(await f.read([[0,0]]),{error:0,bytes:0}); assert.equal(f.stream.position,0);
});
test('Blob rejection reports EIO without advancing or writing data',async()=>{
  const f=fixture({slice:()=>({arrayBuffer:async()=>{throw Error('read failed');}})});
  f.context.HEAPU8[0]=99;
  assert.deepEqual(await f.read([[0,8]]),{error:29,bytes:0});
  assert.equal(f.context.HEAPU8[0],99); assert.equal(f.stream.position,0);
});
test('pending Blob reads are observable until Asyncify resumes or returns EIO',async()=>{
  let release;
  const f=fixture({slice:(start,end)=>({arrayBuffer:()=>new Promise(resolve=>{release=()=>resolve(Uint8Array.from({length:end-start},(_,i)=>start+i).buffer);})})});
  const pending=f.read([[0,6]]);
  assert.equal(f.stats.pending,1);
  assert.equal(f.stats.lastPending.position,0);
  assert.equal(f.stats.lastPending.vectorCount,1);
  assert.equal(f.stats.lastPending.requestedBytes,6);
  release();
  assert.deepEqual(await pending,{error:0,bytes:6});
  assert.equal(f.stats.pending,0);
  assert.equal(f.stats.lastError,undefined);
});
test('heap views are reacquired after suspension; vector descriptors are snapshotted',async()=>{
  let f;
  f=fixture({slice:(start,end)=>({arrayBuffer:async()=>{
    const memory=new ArrayBuffer(1024), next=new Uint8Array(memory);
    next.set(f.context.HEAPU8); f.context.HEAPU8=next; f.context.HEAPU32=new Uint32Array(memory);
    f.context.HEAPU32[34]=400; // Mutate second vector after the read has started.
    return Uint8Array.from({length:end-start},(_,i)=>start+i).buffer;
  }})});
  assert.deepEqual(await f.read([[0,2],[16,2]]),{error:0,bytes:4});
  assert.deepEqual([...f.context.HEAPU8.slice(16,18)],[2,3]);
  assert.equal(f.context.HEAPU8[400],0);
});
test('ordinary files keep the original synchronous read path',async()=>{
  const f=fixture(); delete f.stream.node.asyncBlob;
  assert.deepEqual(await f.read([[0,8]]),{error:0,bytes:7}); assert.equal(f.stats.reads,0);
});
