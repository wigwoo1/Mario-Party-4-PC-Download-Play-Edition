import {test} from 'node:test';
import assert from 'node:assert/strict';
import {fingerprintRuntimeFiles,prepareFingerprintedRuntimeFactory} from './runtime-build.mjs';
const files=()=>[{name:'contract.wasm',blob:new Blob(['wasm'])},{name:'contract.mjs',blob:new Blob(['wrapper'])}];

test('agreed shared package identity rejects changed native artifacts before executable import',async()=>{
  const supplied=[...files(),{name:'native/partyboard.exe',blob:new Blob(['native'])}];
  const {buildId}=await fingerprintRuntimeFiles(supplied);let imports=0;
  const options={expectedBuildId:buildId,importWrapper:async()=>{imports++;return {default:options=>options};}};
  const factory=await prepareFingerprintedRuntimeFactory(supplied,options);
  assert.equal(factory.buildId,buildId);assert.equal(imports,1);
  for(const index of [0,1,2]){
    const changed=supplied.slice();changed[index]={...changed[index],blob:new Blob(['different'])};
    await assert.rejects(prepareFingerprintedRuntimeFactory(changed,options),/differs from agreed identity/);
  }
  for(const expectedBuildId of ['',null,'X'.repeat(64)])
    await assert.rejects(prepareFingerprintedRuntimeFactory(supplied,{...options,expectedBuildId}),/Valid expected/);
  assert.equal(imports,1);
});
test('runtime identity covers wrapper and Wasm, independent of enumeration order',async()=>{
  const a=await fingerprintRuntimeFiles(files()),b=await fingerprintRuntimeFiles(files().reverse());
  assert.deepEqual(a,b);assert.match(a.buildId,/^[a-f0-9]{64}$/);
  for(const index of [0,1]) {
    const changed=files();changed[index].blob=new Blob(['changed']);
    assert.notEqual((await fingerprintRuntimeFiles(changed)).buildId,a.buildId);
  }
});
test('artifact names and side-module coverage affect identity',async()=>{
  const original=await fingerprintRuntimeFiles(files()),renamed=files();renamed[0].name='other.wasm';
  assert.notEqual((await fingerprintRuntimeFiles(renamed)).buildId,original.buildId);
  assert.notEqual((await fingerprintRuntimeFiles([...files(),{name:'overlays/boot.wasm',blob:new Blob(['overlay'])}])).buildId,original.buildId);
});
test('invalid or duplicate runtime artifact entries and cancellation reject',async()=>{
  for(const name of ['../file','/absolute','C:/file','a/../b',''])
    await assert.rejects(fingerprintRuntimeFiles([{name,blob:new Blob(['x'])}]),/Invalid/);
  await assert.rejects(fingerprintRuntimeFiles([files()[0],files()[0]]),/Invalid/);
  const abort=new AbortController();abort.abort();await assert.rejects(fingerprintRuntimeFiles(files(),{signal:abort.signal}),{name:'AbortError'});
});

test('factory loads fingerprinted wrapper and isolated binary snapshots',async()=>{
  const supplied=files();let imported;
  const factory=await prepareFingerprintedRuntimeFactory(supplied,{importWrapper:async blob=>{
    imported=await blob.text();return {default:options=>options};
  }});
  assert.equal(imported,'wrapper');assert.equal(factory.buildId,(await fingerprintRuntimeFiles(supplied)).buildId);
  const first=factory.createRuntime({noInitialRun:true,wasmBinary:new Uint8Array([9])});
  assert.equal(new TextDecoder().decode(first.wasmBinary),'wasm');assert.equal(first.noInitialRun,true);
  assert.equal(first.locateFile('contract.wasm'),'data:application/wasm,');
  assert.throws(()=>first.locateFile('unlisted.data'),/Unmapped/);
  first.wasmBinary.fill(0);
  assert.equal(new TextDecoder().decode(factory.createRuntime({}).wasmBinary),'wasm');
});

test('runtime captures its generated Module before startup Asyncify loads embedded overlays',async()=>{
  const supplied=files();let captured,revoked=[],initializedThis,initializedArgs;
  const oldCreate=URL.createObjectURL,oldRevoke=URL.revokeObjectURL;
  URL.createObjectURL=blob=>{const url='blob:verified/'+blob.size;return url;};
  URL.revokeObjectURL=url=>revoked.push(url);
  try {
    const factory=await prepareFingerprintedRuntimeFactory(supplied,{importWrapper:async()=>({
      default:options=>{captured=options;const generatedModule={FS:{
        readdir:()=>['.','..','m444dll.wasm'],
        analyzePath:path=>({exists:path==='/m444dll.wasm'}),
        readFile:path=>{assert.equal(path,'/m444dll.wasm');return new Uint8Array([0,97,115,109]);},
      }};options.onRuntimeInitialized.call(generatedModule,'ready');return Promise.resolve(generatedModule);},
    })});
    const runtime=await factory.createRuntime({noInitialRun:true,onRuntimeInitialized(...args){
      initializedThis=this;initializedArgs=args;
      assert.equal(captured.locateFile('/m444Dll.wasm'),'blob:verified/4');
    }});
    assert.deepEqual(initializedArgs,['ready']);assert.ok(initializedThis.FS);
    assert.equal(captured.locateFile('contract.wasm'),'data:application/wasm,');
    assert.equal(captured.locateFile('/m444Dll.wasm'),'blob:verified/4');
    assert.equal(captured.locateFile('m444Dll.wasm'),'blob:verified/4');
    assert.throws(()=>captured.locateFile('/unlisted.wasm'),/Unmapped/);
    assert.throws(()=>captured.locateFile('/assets/foreign.wasm'),/Unmapped/);
    assert.throws(()=>captured.locateFile('/not-a-module.data'),/Unmapped/);
    runtime.partyboardReleaseRuntimeArtifacts();runtime.partyboardReleaseRuntimeArtifacts();
    assert.deepEqual(revoked,['blob:verified/4']);
  } finally {URL.createObjectURL=oldCreate;URL.revokeObjectURL=oldRevoke;}
});

test('factory rejects invalid artifacts, missing pairs and abort during import',async()=>{
  await assert.rejects(prepareFingerprintedRuntimeFactory([{name:'contract.mjs',blob:'text'}]),/Invalid/);
  await assert.rejects(prepareFingerprintedRuntimeFactory([files()[0]],{importWrapper:()=>{throw Error('Must not import');}}),/wrapper and Wasm/);
  const abort=new AbortController();
  await assert.rejects(prepareFingerprintedRuntimeFactory(files(),{signal:abort.signal,importWrapper:async()=>{
    abort.abort();return {default:()=>{throw Error('Must not instantiate');}};
  }}),{name:'AbortError'});
});

test('cancelled preparation never reads artifact entries or imports a wrapper',async()=>{
  const abort=new AbortController();abort.abort();let reads=0;
  const entries=[{get blob(){reads++;throw Error('Must not read');}}];
  await assert.rejects(prepareFingerprintedRuntimeFactory(entries,{signal:abort.signal,
    importWrapper:()=>{throw Error('Must not import');}}),{name:'AbortError'});
  assert.equal(reads,0);
});

test('instance cancellation prevents instantiation without cancelling other instances',async()=>{
  let instances=0;
  const factory=await prepareFingerprintedRuntimeFactory(files(),{importWrapper:async()=>({
    default:options=>{instances++;return options;},
  })});
  const abort=new AbortController();abort.abort();
  assert.throws(()=>factory.createRuntime({signal:abort.signal}),{name:'AbortError'});
  assert.equal(instances,0);
  assert.equal(factory.createRuntime({noInitialRun:true}).noInitialRun,true);
  assert.equal(instances,1);
});

test('execution overrides cannot bypass the fingerprinted binary boundary',async()=>{
  let instances=0,located=0;
  const factory=await prepareFingerprintedRuntimeFactory(files(),{importWrapper:async()=>({
    default:options=>{instances++;return options;},
  })});
  for(const options of [{instantiateWasm:()=>{}},{wasm:{}},
      {dynamicLibraries:['foreign.wasm']},{dynamicLibraries:'foreign.wasm'}])
    assert.throws(()=>factory.createRuntime(options),/Unfingerprinted/);
  assert.equal(instances,0);
  const runtime=factory.createRuntime({dynamicLibraries:[],locateFile:()=>{located++;return '/foreign.wasm';}});
  assert.equal(runtime.locateFile('contract.wasm'),'data:application/wasm,');
  assert.throws(()=>runtime.locateFile('foreign.wasm'),/Unmapped/);
  assert.equal(located,0);assert.equal(instances,1);
});
