import test from 'node:test';
import assert from 'node:assert/strict';
import {loadSharedRuntimeInventory} from './browser-runtime-package.mjs';
import {createSessionConfiguration} from './session-config.mjs';
const inventory={version:1,profile:'unlocked-no-save-v1',buildId:'a'.repeat(64),runtimeArtifacts:['native/dol.dll','native/partyboard.exe']};
const load=value=>loadSharedRuntimeInventory({baseURL:'https://lobby.example/',fetch:async()=>new Response(JSON.stringify(value))});
test('shared inventory supplies the exact runtime identity and artifact list to browser-host session configuration',async()=>{
 const value=await load(inventory);const config=createSessionConfiguration('b'.repeat(64),crypto,value.buildId,value.runtimeArtifacts);
 assert.equal(config.buildId,inventory.buildId);assert.deepEqual(config.runtimeArtifacts,inventory.runtimeArtifacts);assert.ok(Object.isFrozen(value.runtimeArtifacts));
});
test('standalone diagnostic runtimes keep the pair-only path only on an explicit missing inventory',async()=>{
 assert.equal(await loadSharedRuntimeInventory({baseURL:'https://lobby.example/',fetch:async()=>new Response('missing',{status:404})}),null);
 await assert.rejects(loadSharedRuntimeInventory({baseURL:'https://lobby.example/',fetch:async()=>new Response('unavailable',{status:500})}),/unavailable/);
});
test('inventory rejects traversal, private paths, duplicate executable names and oversized response bodies',async()=>{
 for(const name of ['../game.ciso','/private/game.ciso','C:\\private\\runtime.dll','contract.wasm','native/dol.dll']){
  await assert.rejects(load({...inventory,runtimeArtifacts:['native/dol.dll',name]}),/artifact name/);
 }
 await assert.rejects(load({...inventory,buildId:'wrong'}),/Invalid shared/);
 await assert.rejects(loadSharedRuntimeInventory({baseURL:'https://lobby.example/',fetch:async()=>new Response('x'.repeat(256*1024+1))}),/size limit/);
});
