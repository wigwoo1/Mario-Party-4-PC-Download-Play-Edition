import test from 'node:test';
import assert from 'node:assert/strict';
import {createSessionConfiguration,receiveSessionConfiguration} from './session-config.mjs';
const assetId='a'.repeat(64);
test('host session configuration preserves seed zero, identity and immutable duplicate delivery',()=>{
  const configuration=createSessionConfiguration(assetId,{getRandomValues:a=>(a instanceof Uint8Array?a.fill(1):a.fill(0))});
  assert.equal(configuration.seed,0);
  assert.equal(receiveSessionConfiguration(JSON.stringify(configuration),{assetId,current:configuration}),configuration);
  assert.throws(()=>configuration.seed=2,TypeError);
  assert.throws(()=>receiveSessionConfiguration(JSON.stringify({...configuration,seed:2}),{assetId,current:configuration}),/changed/);
  assert.throws(()=>receiveSessionConfiguration(JSON.stringify(configuration),{assetId:'b'.repeat(64)}),/Invalid/);
  assert.throws(()=>receiveSessionConfiguration(JSON.stringify({...configuration,session:'0'.repeat(32)})),/Invalid/);
});
test('configured package pin cannot change or disappear on duplicate delivery',()=>{
 const configuration=createSessionConfiguration(assetId,{getRandomValues:a=>a.fill(1)}),buildId='b'.repeat(64);
 const pinned=receiveSessionConfiguration(JSON.stringify({...configuration,buildId}),{assetId});
 assert.equal(pinned.buildId,buildId);assert.throws(()=>pinned.buildId='c'.repeat(64),TypeError);
 for(const message of [configuration,{...pinned,buildId:'c'.repeat(64)}])
  assert.throws(()=>receiveSessionConfiguration(JSON.stringify(message),{assetId,current:pinned}),/changed/);
 for(const buildId of ['',null,'X'.repeat(64)])
  assert.throws(()=>receiveSessionConfiguration(JSON.stringify({...configuration,buildId}),{assetId}),/Invalid/);
});
test('host creates an immutable configuration with its measured package pin',()=>{
 const random={getRandomValues:a=>a.fill(1)},buildId='c'.repeat(64);
 const configuration=createSessionConfiguration(assetId,random,buildId);
 assert.equal(configuration.buildId,buildId);
 assert.equal(receiveSessionConfiguration(JSON.stringify(configuration),{assetId,current:configuration}),configuration);
 assert.throws(()=>createSessionConfiguration(assetId,random,'invalid'),/Invalid/);
});
test('shared artifact enumeration is pinned, immutable and cannot change on redelivery',()=>{
 const configuration=createSessionConfiguration(assetId,{getRandomValues:a=>a.fill(1)},'b'.repeat(64));
 const runtimeArtifacts=['native/partyboard.exe','native/res/666f6e74'];
 const pinned=receiveSessionConfiguration(JSON.stringify({...configuration,runtimeArtifacts}),{assetId});
 assert.deepEqual(pinned.runtimeArtifacts,runtimeArtifacts);
 assert.throws(()=>pinned.runtimeArtifacts.push('native/new'),TypeError);
 assert.throws(()=>receiveSessionConfiguration(JSON.stringify({...pinned,runtimeArtifacts:runtimeArtifacts.slice(0,1)}),{assetId,current:pinned}),/changed/);
 for(const names of [['../bad'],['contract.wasm'],['native/a','native/a'],Array(255).fill('native/a')])
  assert.throws(()=>receiveSessionConfiguration(JSON.stringify({...configuration,runtimeArtifacts:names})),/artifact/);
 assert.throws(()=>receiveSessionConfiguration(JSON.stringify({...configuration,buildId:undefined,runtimeArtifacts})),/artifact/);
});
