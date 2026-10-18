import {test} from 'node:test';
import assert from 'node:assert/strict';
import {StartBarrier} from './start-barrier.mjs';
import {bindClientStartCommit} from './start-commit-channel.mjs';
class Channel extends EventTarget {
  readyState='open';ordered=true;
  close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}
  message(data){this.dispatchEvent(new MessageEvent('message',{data}));}
}
function fixture() {
  const snapshot={revision:1,membership:1,total:100,assetId:'a'.repeat(64),players:
    ['host','guest'].map((peer,slot)=>({peer,slot,bytes:100,verified:true,shaders:true,ready:true}))};
  const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:1,buildId:'b'.repeat(64),checkpoint:42});
  let runs=0,disposals=0;const errors=[];
  const prepared={acknowledgment:{...barrier.proposal,type:'session-start-ack'},runtime:{sessionSeed:1,checkpoint:42,
    engine:{phase:'Idle',preparedCheckpoint:()=>42,resume(){this.phase='Running';runs++;return Promise.resolve();}},
    dispose:async()=>disposals++}};
  const channel=new Channel(),receiver=bindClientStartCommit(channel,prepared,
    {peer:'guest',buildId:'b'.repeat(64),snapshot:()=>snapshot},{error:e=>errors.push(e.message)});
  return {channel,receiver,errors,commit:{...barrier.proposal,type:'session-start-commit'},runs:()=>runs,disposals:()=>disposals};
}
test('matching duplicate commit resumes once; channel loss releases runtime',async()=>{
  const f=fixture(),message=JSON.stringify(f.commit);f.channel.message(message);f.channel.message(message);
  await f.receiver.execution;assert.equal(f.runs(),1);assert.equal(f.receiver.committed,true);
  f.channel.close();await f.receiver.close();assert.equal(f.disposals(),1);assert.equal(f.errors.length,1);
  f.channel.message(message);assert.equal(f.runs(),1);
});
test('malformed, oversized, mismatched and conflicting commits close and clean up',async()=>{
  for(const kind of ['malformed','oversized','mismatch','conflict']) {
    const f=fixture();if(kind==='conflict')f.channel.message(JSON.stringify(f.commit));
    f.channel.message(kind==='malformed'?'{':kind==='oversized'?'x'.repeat(2049):JSON.stringify({...f.commit,seed:2}));
    await f.receiver.close();assert.equal(f.disposals(),1);assert.equal(f.runs(),kind==='conflict'?1:0);assert.equal(f.errors.length,1);
  }
});

test('committed session handoff transfers cleanup and channel-loss ownership exactly once',async()=>{
  const f=fixture();assert.throws(()=>f.receiver.handoff());f.channel.message(JSON.stringify(f.commit));
  const session=f.receiver.handoff();assert.equal(session.channel,f.channel);assert.equal(session.runtime.engine.phase,'Running');
  assert.throws(()=>f.receiver.handoff());await f.receiver.close();assert.equal(f.channel.readyState,'open');
  f.channel.close();await session.execution;assert.equal(f.disposals(),0);assert.deepEqual(f.errors,[]);
  await session.runtime.dispose();assert.equal(f.disposals(),1);
});
test('failed start cannot hand off resources to session recovery',async()=>{
  const f=fixture();f.channel.message('{');assert.throws(()=>f.receiver.handoff());await f.receiver.close();assert.equal(f.disposals(),1);
});

test('reentrant channel cleanup joins the existing disposal operation',async()=>{
  const f=fixture();let joined,closes=0;
  f.channel.close=()=>{closes++;joined=f.receiver.close();};
  const closing=f.receiver.close();await closing;
  assert.equal(joined,closing);assert.equal(closes,1);assert.equal(f.disposals(),1);
  assert.equal(f.receiver.close(),closing);
});

test('channel close failure still releases prepared runtime before rejecting',async()=>{
  const f=fixture();f.channel.close=()=>{throw Error('Close failed');};
  await assert.rejects(f.receiver.close(),/Close failed/);
  assert.equal(f.disposals(),1);assert.throws(()=>f.receiver.handoff());
  f.channel.message(JSON.stringify(f.commit));assert.equal(f.runs(),0);
  f.channel.close=()=>{f.channel.readyState='closed';};
  await f.receiver.close();assert.equal(f.disposals(),1);
});
