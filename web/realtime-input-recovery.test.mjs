import test from 'node:test';
import assert from 'node:assert/strict';
import {SessionInput} from './session-input.mjs';
import {createSessionInputExchange} from './session-input-exchange.mjs';
import {createRealtimeInputRecovery} from './realtime-input-recovery.mjs';

function fixture({failMenu=false,closeChannels,execution}={}) {
  let time=0,id=0;const tasks=new Map(),events=[],errors=[];
  const session=new Uint8Array(16).fill(1),owners=[11,22,0,0];
  const queue=new SessionInput(session,owners);
  const bridge=createSessionInputExchange(queue,{session,owners,peer:11,checkpoint:()=>42,
    send:()=>{},suspendOnPause:true,now:()=>time});
  const wait=bridge.exchange(new Uint8Array(44));queue.disconnected(22,time);
  const recovery=createRealtimeInputRecovery(queue,bridge,{
    channels:{close(){events.push('channels');queue.cancel();return closeChannels?.();}},
    dispose:async()=>{assert.equal(await wait,null);events.push('dispose');},
    returnToMenu:()=>{events.push('menu');if(failMenu){failMenu=false;throw Error('Menu temporarily unavailable');}},
    execution,transportReady:()=>true,now:()=>time,error:cause=>errors.push(cause.message),
    schedule:callback=>{tasks.set(++id,callback);return id;},cancel:key=>tasks.delete(key),
  });
  return {queue,bridge,recovery,wait,tasks,events,errors,time:value=>time=value,
    async check(){assert.equal(tasks.size,1);const [key,callback]=tasks.entries().next().value;tasks.delete(key);await callback();}};
}

test('automatic monotonic deadline releases suspended input without incoming traffic',async()=>{
  const f=fixture();f.time(59999);await f.check();assert.deepEqual(f.events,[]);
  assert.equal(f.bridge.pendingState().frame,0n);assert.equal(f.tasks.size,1);
  f.time(60000);await f.check();assert.deepEqual(f.events,['channels','dispose','menu']);
  assert.equal(f.tasks.size,0);assert.equal(f.queue.status(60000).state,'ReturnToMenu');
  await f.recovery.close();assert.deepEqual(f.events,['channels','dispose','menu']);
});

test('explicit close cancels scheduled checks and stale callbacks cannot run cleanup again',async()=>{
  const f=fixture(),stale=f.tasks.values().next().value;
  await f.recovery.close();assert.equal(f.tasks.size,0);await stale();
  assert.deepEqual(f.events,['channels','dispose','menu']);assert.equal(f.tasks.size,0);
});

test('failed menu handoff retries on timer without disposing runtime twice',async()=>{
  const f=fixture({failMenu:true});f.time(60000);await f.check();
  assert.deepEqual(f.errors,['Menu temporarily unavailable']);assert.equal(f.tasks.size,1);
  await f.check();assert.deepEqual(f.events,['channels','dispose','menu','menu']);assert.equal(f.tasks.size,0);
});

test('automatic expiry cancels a stalled replacement verifier',async()=>{
  const f=fixture(),token=f.recovery.attempt(22);let signal;
  const restoring=f.recovery.restore(token,(_token,options)=>{signal=options.signal;return new Promise(()=>{});});
  await Promise.resolve();f.time(60000);await f.check();assert.equal(await restoring,false);
  assert.equal(signal.aborted,true);assert.equal(f.tasks.size,0);assert.deepEqual(f.events,['channels','dispose','menu']);
});

test('asynchronous transport release finishes before engine disposal or another scheduled check',async()=>{
  let release;const f=fixture({closeChannels:()=>new Promise(resolve=>{release=resolve;})});
  f.time(60000);const checking=f.check();await Promise.resolve();await Promise.resolve();
  assert.deepEqual(f.events,['channels']);assert.equal(f.tasks.size,0);
  release();await checking;assert.deepEqual(f.events,['channels','dispose','menu']);assert.equal(f.tasks.size,0);
});

test('failed transport release retries before disposal and menu handoff',async()=>{
  let tries=0;const f=fixture({closeChannels:async()=>{if(++tries===1)throw Error('Transport release failed');}});
  f.time(60000);await f.check();assert.deepEqual(f.events,['channels']);
  assert.deepEqual(f.errors,['Transport release failed']);assert.equal(f.tasks.size,1);
  await f.check();assert.deepEqual(f.events,['channels','channels','dispose','menu']);assert.equal(f.tasks.size,0);
});

test('transport callback requesting close joins the existing timeout cleanup',async()=>{
  let closing;const f=fixture({closeChannels:()=>{closing=f.recovery.close();}});
  f.time(60000);await f.check();await closing;
  assert.deepEqual(f.events,['channels','dispose','menu']);assert.equal(f.tasks.size,0);
});

test('explicit close during pending timeout cleanup joins it and stops scheduling',async()=>{
  let release;const f=fixture({closeChannels:()=>new Promise(resolve=>{release=resolve;})});
  f.time(60000);const checking=f.check();
  while(!release)await Promise.resolve();const closing=f.recovery.close();
  assert.deepEqual(f.events,['channels']);release();await Promise.all([checking,closing]);
  assert.deepEqual(f.events,['channels','dispose','menu']);assert.equal(f.tasks.size,0);
});

test('failed engine execution cancels input and cleans up before reconnect deadline',async()=>{
  let fail;const execution=new Promise((_resolve,reject)=>{fail=reject;}),f=fixture({execution});
  fail(Error('Engine failed'));await execution.catch(()=>{});await f.recovery.tick();
  assert.equal(f.queue.status(0).state,'ReturnToMenu');assert.deepEqual(f.errors,['Engine failed']);
  assert.deepEqual(f.events,['channels','dispose','menu']);assert.equal(f.tasks.size,0);
});

test('normal engine termination releases session ownership exactly once',async()=>{
  let finish;const execution=new Promise(resolve=>{finish=resolve;}),f=fixture({execution});
  finish();await execution;await f.recovery.tick();await f.recovery.close();
  assert.deepEqual(f.errors,[]);assert.deepEqual(f.events,['channels','dispose','menu']);assert.equal(f.tasks.size,0);
});
