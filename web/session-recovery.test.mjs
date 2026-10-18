import {test} from 'node:test';
import assert from 'node:assert/strict';
import {createSessionRecovery} from './session-recovery.mjs';
test('disconnect pauses; deadline uses wall clock and cleanup waits for execution',async()=>{
  class Channel extends EventTarget {readyState='open';close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}}
  const channel=new Channel();let time=100,stop=0,disposed=0,menu=0,finish;
  const execution=new Promise(resolve=>finish=resolve);
  const recovery=createSessionRecovery({channel,execution,runtime:{engine:{requestStop(){stop++;}},dispose:async()=>disposed++}},
    {owners:[1,2,0,0],hostPeer:1,now:()=>time,returnToMenu:async()=>menu++});
  channel.close();assert.equal(stop,1);assert.equal(recovery.state,'Paused');
  time=60099;await recovery.tick();assert.equal(disposed,0);assert.equal(recovery.remainingMs,1);
  time=60100;const cleanup=recovery.tick();await Promise.resolve();assert.equal(disposed,0);
  finish();await cleanup;assert.equal(disposed,1);assert.equal(menu,1);await recovery.tick();assert.equal(menu,1);
});

test('failed stopped execution is terminal and cleanup can retry without early menu return',async()=>{
  class Channel extends EventTarget {readyState='open';close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}}
  const channel=new Channel();let reject,attempts=0,menus=0;
  const execution=new Promise((resolve,fail)=>reject=fail);
  const recovery=createSessionRecovery({channel,execution,runtime:{engine:{requestStop(){}},
    dispose:async()=>{if(++attempts===1)throw Error('GPU busy');}}},
    {owners:[1,2,0,0],hostPeer:1,now:()=>100,returnToMenu:async()=>menus++});
  channel.close();reject(Error('engine failed'));await Promise.resolve();
  assert.equal(recovery.state,'ReturnToMenu');await assert.rejects(recovery.tick(),/GPU busy/);
  assert.equal(menus,0);await recovery.tick();assert.equal(attempts,2);assert.equal(menus,1);
  await recovery.tick();assert.equal(attempts,2);
});
test('repeated channel loss does not extend deadline; backward clock exits safely',async()=>{
  class Channel extends EventTarget {readyState='open';close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}}
  const channel=new Channel();let time=100,stops=0,menus=0;
  const recovery=createSessionRecovery({channel,execution:Promise.resolve(),runtime:{engine:{requestStop(){stops++;}},dispose:async()=>{}}},
    {owners:[1,2,0,0],hostPeer:1,now:()=>time,returnToMenu:async()=>menus++});
  channel.close();time=500;channel.dispatchEvent(new Event('error'));
  assert.equal(stops,1);assert.equal(recovery.remainingMs,59600);
  time=99;await recovery.tick();assert.equal(menus,1);
});

test('recovery verification waits for pause and rejects stale or expired results',async()=>{
  class Channel extends EventTarget {readyState='open';close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}}
  const channel=new Channel();let time=0,finish,verified=0;
  const recovery=createSessionRecovery({channel,execution:new Promise(resolve=>finish=resolve),runtime:{engine:{requestStop(){}},dispose:async()=>{}}},
    {owners:[1,2,0,0],hostPeer:1,now:()=>time,returnToMenu:async()=>{}});
  channel.close();const attempt=recovery.attempt;
  assert.equal(await recovery.verifyRecoveredHost({...attempt,epoch:0n},()=>{verified++;return true;}),false);
  const pending=recovery.verifyRecoveredHost(attempt,()=>{verified++;return true;});await Promise.resolve();assert.equal(verified,0);
  finish();assert.equal(await pending,true);assert.equal(recovery.state,'Paused');
  let complete;const delayed=recovery.verifyRecoveredHost(attempt,()=>new Promise(resolve=>complete=resolve));
  await Promise.resolve();time=60000;complete(true);assert.equal(await delayed,false);
  assert.equal(recovery.state,'ReturnToMenu');await recovery.tick();
});

test('resume requires verified attempt, live reliable channel and explicit synchronous authorization',async()=>{
  class Channel extends EventTarget {readyState='open';ordered=true;close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}}
  const initial=new Channel(),next=new Channel();let runs=0,time=0;
  const engine={phase:'Running',requestStop(){this.phase='Idle';},resume(){this.phase='Running';runs++;return Promise.resolve();}};
  const recovery=createSessionRecovery({channel:initial,execution:Promise.resolve(),runtime:{engine,dispose:async()=>{}}},
    {owners:[1,2,0,0],hostPeer:1,now:()=>time,returnToMenu:async()=>{}});
  initial.close();const attempt=recovery.attempt;
  assert.equal(recovery.resumeRecoveredHost(attempt,next,()=>true),false);
  assert.equal(await recovery.verifyRecoveredHost(attempt,()=>true),true);
  assert.equal(recovery.resumeRecoveredHost(attempt,next,()=>false),false);
  next.ordered=false;assert.equal(recovery.resumeRecoveredHost(attempt,next,()=>true),false);next.ordered=true;
  assert.equal(recovery.resumeRecoveredHost(attempt,next,()=>true),true);assert.equal(runs,1);
  assert.equal(recovery.resumeRecoveredHost(attempt,next,()=>true),false);
  time=10;next.close();assert.equal(recovery.state,'Paused');assert.notEqual(recovery.attempt.epoch,attempt.epoch);
  assert.equal(await recovery.verifyRecoveredHost(attempt,()=>true),false);await recovery.close();
});
