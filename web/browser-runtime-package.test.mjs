import {test} from 'node:test';
import assert from 'node:assert/strict';
import {loadBrowserRuntimePackage} from './browser-runtime-package.mjs';
import {fingerprintRuntimeFiles} from './runtime-build.mjs';

test('shared package retains native and resource bytes in the measured identity',async()=>{
 const supplementalArtifacts=['native/partyboard.exe','native/res/666f6e742e747466'];
 const files=await loadBrowserRuntimePackage({baseURL:'https://example.test/runtime/',supplementalArtifacts,
  fetch:async url=>new Response(url.endsWith('partyboard.exe')?'native-executable':url.endsWith('ttf')?'font':'browser')});
 assert.deepEqual(files.map(file=>file.name),['contract.mjs','contract.wasm',...supplementalArtifacts]);
 const measured=await fingerprintRuntimeFiles(files);
 assert.notEqual(measured.buildId,(await fingerprintRuntimeFiles(files.slice(0,2))).buildId);
 assert.equal(measured.files.length,4);
});
test('invalid supplemental paths and duplicate executable names reject before requests',async()=>{
 for(const supplementalArtifacts of [null,['../native'],['native/./file'],['contract.wasm'],['native/a','native/a'],['native/a?x'],Array(255).fill('native/a')]){
  let calls=0;
  await assert.rejects(loadBrowserRuntimePackage({baseURL:'https://example.test/runtime/',supplementalArtifacts,fetch:async()=>{calls++;return new Response('x');}}),/artifact/);
  assert.equal(calls,0);
 }
});
test('failed native artifact cancels and settles pending browser and resource downloads',async()=>{
 let cancelled=0;
 await assert.rejects(loadBrowserRuntimePackage({baseURL:'https://example.test/runtime/',supplementalArtifacts:['native/missing.dll','native/res/font'],
  fetch:async(url,{signal})=>url.endsWith('missing.dll')?new Response('missing',{status:404}):
   new Promise((resolve,reject)=>signal.addEventListener('abort',()=>{cancelled++;reject(signal.reason);},{once:true}))}),/unavailable/);
 assert.equal(cancelled,3);
});

test('package downloads only executable pair with immutable bytes and no redirects',async()=>{
  const calls=[],bytes=new Uint8Array([0,97,115,109]);
  const files=await loadBrowserRuntimePackage({baseURL:'https://example.test/runtime/',fetch:async(url,options)=>{
    calls.push({url,options});return new Response(url.endsWith('.wasm')?bytes:'export default ()=>{}');
  }});
  bytes.fill(9);
  assert.deepEqual(calls.map(call=>call.url),['https://example.test/runtime/contract.mjs','https://example.test/runtime/contract.wasm']);
  for(const {options} of calls){assert.equal(options.redirect,'error');assert.equal(options.cache,'no-store');}
  assert.deepEqual([...new Uint8Array(await files[1].blob.arrayBuffer())],[0,97,115,109]);
  assert.equal(files[0].blob.type,'text/javascript');
});

test('failed wrapper cancels pending Wasm fetch before returning',async()=>{
  let cancelled=false;
  await assert.rejects(loadBrowserRuntimePackage({baseURL:'https://example.test/',fetch:async(url,{signal})=>{
    if(url.endsWith('.mjs'))return new Response('missing',{status:404});
    return new Promise((resolve,reject)=>signal.addEventListener('abort',()=>{cancelled=true;reject(signal.reason);},{once:true}));
  }}),/unavailable/);
  assert.equal(cancelled,true);
});

test('stream bound rejects before retaining oversized executable and cancels reader',async()=>{
  let cancelled=false;
  await assert.rejects(loadBrowserRuntimePackage({baseURL:'https://example.test/',fetch:async url=>{
    if(url.endsWith('.wasm'))return new Response('wasm');
    return new Response(new ReadableStream({start(controller){controller.enqueue(new Uint8Array(16*1024**2+1));},cancel(){cancelled=true;}}));
  }}),/exceeds/);
  assert.equal(cancelled,true);
});

test('caller cancellation during streaming rejects and releases both readers',async()=>{
  const caller=new AbortController();let cancelled=0,reads=0;
  await assert.rejects(loadBrowserRuntimePackage({baseURL:'https://example.test/',signal:caller.signal,fetch:async()=>
    new Response(new ReadableStream({pull(controller){controller.enqueue(new Uint8Array([1]));if(++reads===2)caller.abort();},cancel(){cancelled++;}}))
  }),{name:'AbortError'});
  assert.equal(cancelled,2);
});

test('rejects empty files, misleading length and invalid package directories',async()=>{
  for(const response of [()=>new Response(''),()=>new Response('x',{headers:{'content-length':'999999999'}})])
    await assert.rejects(loadBrowserRuntimePackage({baseURL:'https://example.test/',fetch:async()=>response()}),/Empty|exceeds/);
  for(const baseURL of ['file:///local/','https://user:secret@example.test/','https://example.test/file','https://example.test/?asset=x'])
    await assert.rejects(loadBrowserRuntimePackage({baseURL,fetch:()=>{throw Error('Must not fetch');}}),/directory/);
});
