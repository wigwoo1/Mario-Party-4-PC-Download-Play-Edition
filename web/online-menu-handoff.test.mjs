import test from 'node:test';
import assert from 'node:assert/strict';
import {installOnlineMenuHandoff} from './online-menu-handoff.mjs';

test('menu transfers ownership only after asynchronous disposal, once',async()=>{
  const events=[];let release;
  const runtime={module:{},dispose:()=>{events.push('dispose');return new Promise(r=>release=r);}};
  const bridge=installOnlineMenuHandoff(runtime,{openLobby:mode=>events.push(mode),onError:assert.fail});
  assert.equal(runtime.module.partyBoardOnlineMenuRequest(2),false);
  assert.equal(runtime.module.partyBoardOnlineMenuRequest(0),true);
  assert.deepEqual(events,[]);
  assert.equal(runtime.module.partyBoardOnlineMenuRequest(1),false);
  await Promise.resolve();assert.deepEqual(events,['dispose']);
  release();await bridge.pending;assert.deepEqual(events,['dispose','host']);
  bridge.close();assert.equal(runtime.module.partyBoardOnlineMenuRequest,undefined);
});
test('closing a pending handoff suppresses stale lobby opening',async()=>{
  let release;const events=[];
  const runtime={module:{},dispose:()=>new Promise(r=>release=r)};
  const bridge=installOnlineMenuHandoff(runtime,{openLobby:mode=>events.push(mode),onError:assert.fail});
  assert.equal(runtime.module.partyBoardOnlineMenuRequest(1),true);
  await Promise.resolve();bridge.close();release();await bridge.pending;
  assert.deepEqual(events,[]);
});
test('failed cleanup never opens a lobby and reports the failure',async()=>{
  const error=Error('shutdown failed');let reported;
  const runtime={module:{},dispose:async()=>{throw error;}};
  const bridge=installOnlineMenuHandoff(runtime,{openLobby:assert.fail,onError:e=>reported=e});
  runtime.module.partyBoardOnlineMenuRequest(1);await bridge.pending;
  assert.equal(reported,error);bridge.close();
});
