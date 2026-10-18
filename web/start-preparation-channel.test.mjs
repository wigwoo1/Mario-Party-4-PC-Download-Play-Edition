import {test} from 'node:test';
import assert from 'node:assert/strict';
import {StartBarrier} from './start-barrier.mjs';
import {bindClientStartPreparation} from './start-preparation-channel.mjs';
class Channel extends EventTarget {
  readyState='open';ordered=true;sent=[];
  send(data){this.sent.push(JSON.parse(data));}
  close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}
  message(data){this.dispatchEvent(new MessageEvent('message',{data}));}
}
const flush=()=>new Promise(resolve=>setImmediate(resolve));
function fixture(hold=false,callerSignal) {
  const snapshot={revision:1,membership:1,total:100,assetId:'a'.repeat(64),players:
    ['host','guest'].map((peer,slot)=>({peer,slot,bytes:100,verified:true,shaders:true,ready:true}))};
  const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:1,buildId:'b'.repeat(64),checkpoint:42});
  let release,disposed=0,runs=0,prepares=0,signal;
  const gate=hold?new Promise(resolve=>release=resolve):Promise.resolve();
  const runtime={assetId:snapshot.assetId,sessionSeed:1,checkpoint:42,
    engine:{phase:'Idle',preparedCheckpoint:()=>42,resume(){runs++;this.phase='Running';return Promise.resolve();}},
    dispose:async()=>disposed++};
  const errors=[],channel=new Channel();
  const owner=bindClientStartPreparation(channel,{peer:'guest',buildId:'b'.repeat(64),snapshot:()=>snapshot,signal:callerSignal,
    prepare:async(_proposal,options)=>{prepares++;signal=options.signal;await gate;return runtime;}},
    {error:cause=>errors.push(cause.message)});
  return {channel,owner,errors,proposal:barrier.proposal,release,
    stats:()=>({disposed,runs,prepares,aborted:signal?.aborted})};
}
test('channel prepares and acknowledges once, then consumes a matching commit',async()=>{
  const f=fixture();f.channel.message(JSON.stringify(f.proposal));f.channel.message(JSON.stringify(f.proposal));
  await flush();assert.equal(f.channel.sent.length,1);assert.equal(f.channel.sent[0].type,'session-start-ack');
  assert.equal(f.stats().prepares,1);assert.equal(f.stats().runs,0);
  f.channel.message(JSON.stringify({...f.proposal,type:'session-start-commit'}));
  await f.owner.execution;assert.equal(f.stats().runs,1);assert.equal(f.owner.committed,true);
  await f.owner.close();assert.equal(f.stats().disposed,1);assert.deepEqual(f.errors,[]);
});
test('disconnect during held preparation aborts and releases late runtime without ack',async()=>{
  const f=fixture(true);f.channel.message(JSON.stringify(f.proposal));await flush();f.channel.close();
  assert.equal(f.stats().aborted,true);f.release();await f.owner.close();
  assert.equal(f.channel.sent.length,0);assert.equal(f.stats().disposed,1);assert.equal(f.stats().runs,0);
  assert.equal(f.errors.length,1);assert.throws(()=>f.owner.handoff());
});
test('ack send failure releases runtime and blocks commit execution',async()=>{
  const f=fixture();f.channel.send=()=>{throw Error('Send failed');};
  f.channel.message(JSON.stringify(f.proposal));await flush();await f.owner.close();
  assert.equal(f.stats().disposed,1);assert.equal(f.errors.length,1);
  f.channel.message(JSON.stringify({...f.proposal,type:'session-start-commit'}));assert.equal(f.stats().runs,0);
});
test('committed handoff leaves cleanup to the session owner',async()=>{
  const f=fixture();f.channel.message(JSON.stringify(f.proposal));await flush();
  f.channel.message(JSON.stringify({...f.proposal,type:'session-start-commit'}));
  const session=f.owner.handoff();await f.owner.close();f.channel.close();
  assert.equal(f.stats().disposed,0);assert.deepEqual(f.errors,[]);
  await session.runtime.dispose();assert.equal(f.stats().disposed,1);
});

test('invalid proposal data closes before preparing or acknowledging',async()=>{
  for(const data of ['{','x'.repeat(2049),new ArrayBuffer(4)]) {
    const f=fixture();f.channel.message(data);await flush();await f.owner.close();
    assert.equal(f.stats().prepares,0);assert.equal(f.channel.sent.length,0);
    assert.equal(f.channel.readyState,'closed');assert.equal(f.errors.length,1);
  }
});

test('conflicting proposal during preparation cancels without acknowledging stale state',async()=>{
  const f=fixture(true);f.channel.message(JSON.stringify(f.proposal));await flush();
  f.channel.message(JSON.stringify({...f.proposal,seed:2}));
  assert.equal(f.stats().aborted,true);f.release();await f.owner.close();
  assert.equal(f.stats().prepares,1);assert.equal(f.stats().disposed,1);
  assert.equal(f.channel.sent.length,0);assert.equal(f.stats().runs,0);
  assert.match(f.errors[0],/changed proposal/);
});

test('duplicate proposal while preparation is pending does not restart work',async()=>{
  const f=fixture(true);const proposal=JSON.stringify(f.proposal);
  f.channel.message(proposal);await flush();f.channel.message(proposal);
  assert.equal(f.stats().aborted,false);f.release();await flush();
  assert.equal(f.stats().prepares,1);assert.equal(f.channel.sent.length,1);
  assert.deepEqual(f.errors,[]);await f.owner.close();assert.equal(f.stats().disposed,1);
});

test('caller cancellation aborts held preparation without reporting a host error',async()=>{
  const abort=new AbortController(),f=fixture(true,abort.signal);
  f.channel.message(JSON.stringify(f.proposal));await flush();abort.abort();
  assert.equal(f.stats().aborted,true);f.release();await f.owner.close();
  assert.equal(f.channel.sent.length,0);assert.equal(f.stats().disposed,1);assert.equal(f.stats().runs,0);
  assert.deepEqual(f.errors,[]);
});
test('already cancelled caller never starts preparation',async()=>{
  const abort=new AbortController();abort.abort();const f=fixture(false,abort.signal);
  f.channel.message(JSON.stringify(f.proposal));await f.owner.close();
  assert.equal(f.stats().prepares,0);assert.equal(f.channel.readyState,'closed');assert.deepEqual(f.errors,[]);
});
test('caller cancellation after session handoff cannot release transferred resources',async()=>{
  const abort=new AbortController(),f=fixture(false,abort.signal);
  f.channel.message(JSON.stringify(f.proposal));await flush();
  f.channel.message(JSON.stringify({...f.proposal,type:'session-start-commit'}));
  const session=f.owner.handoff();abort.abort();await flush();
  assert.equal(f.channel.readyState,'open');assert.equal(f.stats().disposed,0);assert.deepEqual(f.errors,[]);
  await session.runtime.dispose();session.channel.close();
});
