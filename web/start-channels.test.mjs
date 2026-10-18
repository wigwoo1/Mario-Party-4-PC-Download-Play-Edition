import {test} from 'node:test';
import assert from 'node:assert/strict';
import {StartBarrier} from './start-barrier.mjs';
import {StartChannels} from './start-channels.mjs';
class Channel extends EventTarget {
  readyState='open';ordered=true;sent=[];
  send(data){this.sent.push(JSON.parse(data));}
  close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}
  message(data){this.dispatchEvent(new MessageEvent('message',{data}));}
}
function fixture() {
  const snapshot={revision:4,membership:2,total:100,assetId:'a'.repeat(64),players:
    ['host','guest'].map((peer,slot)=>({peer,slot,bytes:100,verified:true,shaders:true,ready:true}))};
  const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:1,
    buildId:'b'.repeat(64),checkpoint:42}),errors=[];
  const transport=new StartChannels(barrier,{snapshot:()=>snapshot,error:(peer,e)=>errors.push([peer,e.message])});
  const ack=JSON.stringify({...barrier.proposal,type:'session-start-ack'});
  return {snapshot,barrier,transport,errors,ack};
}

test('runtime commit checks idle host state and open acknowledged remote channels without sending',()=>{
  const {snapshot,transport,ack}=fixture(),channel=new Channel();
  let frames=0;
  const runtime={assetId:snapshot.assetId,sessionSeed:1,checkpoint:42,
    engine:{phase:'Idle',preparedCheckpoint:()=>42,resume(){frames++;}}};
  transport.attach('guest',channel);
  assert.throws(()=>transport.prepareRuntimeCommit('host',runtime));
  channel.message(ack);transport.acknowledgeLocal('host',JSON.parse(ack));
  runtime.assetId='c'.repeat(64);
  assert.throws(()=>transport.prepareRuntimeCommit('host',runtime),/asset differs/);
  runtime.assetId=snapshot.assetId;
  const commit=transport.prepareRuntimeCommit('host',runtime);
  assert.equal(commit.type,'session-start-commit');assert.equal(frames,0);
  assert.equal(channel.sent.length,1);assert.equal(channel.sent[0].type,'session-start');
  channel.readyState='closed';
  assert.throws(()=>transport.prepareRuntimeCommit('host',runtime),/open start channel/);
  assert.equal(frames,0);transport.close();
});
test('bound channel routes acknowledgment; body cannot impersonate another player',()=>{
  const {barrier,transport,ack}=fixture(),channel=new Channel();transport.attach('guest',channel);
  assert.equal(channel.sent[0].type,'session-start');
  channel.message(JSON.stringify({...JSON.parse(ack),peer:'host'}));
  assert.deepEqual(barrier.pending,['host']);
  transport.acknowledgeLocal('host',JSON.parse(ack));assert.equal(barrier.state,'Agreed');transport.close();
});
test('replacement invalidates agreement and old events cannot affect the new binding',()=>{
  const {barrier,transport,ack,errors}=fixture(),old=new Channel(),next=new Channel();
  transport.attach('guest',old);transport.attach('guest',next);
  old.message('broken');old.dispatchEvent(new Event('close'));next.message(ack);
  assert.equal(barrier.state,'Cancelled');assert.equal(errors.length,0);assert.equal(next.sent.length,0);
  transport.close();transport.close();assert.equal(next.readyState,'closed');
});
test('malformed or oversized active data fails closed',()=>{
  for(const data of ['{','x'.repeat(2049),new ArrayBuffer(3)]) {
    const {barrier,transport,errors}=fixture(),channel=new Channel();transport.attach('guest',channel);
    channel.message(data);assert.equal(barrier.state,'Cancelled');assert.equal(errors.length,1);
    assert.equal(channel.readyState,'closed');transport.close();
  }
});
test('stale acknowledgment does not advance; readiness revocation cancels',()=>{
  const {snapshot,barrier,transport,ack}=fixture(),channel=new Channel();transport.attach('guest',channel);
  channel.message(JSON.stringify({...JSON.parse(ack),seed:99}));assert.equal(barrier.state,'Awaiting');
  snapshot.players[1].ready=false;transport.refresh();assert.equal(barrier.state,'Cancelled');transport.close();
});
test('unassigned and shared or unreliable channels cannot disturb valid connection',()=>{
  const {transport}=fixture(),channel=new Channel();transport.attach('guest',channel);
  assert.throws(()=>transport.attach('outsider',new Channel()),/Invalid/);
  assert.throws(()=>transport.attach('host',channel),/already assigned/);
  const unreliable=new Channel();unreliable.maxRetransmits=0;
  assert.throws(()=>transport.attach('host',unreliable),/Invalid/);
  assert.equal(channel.readyState,'open');transport.close();
});
test('proposal waits for open and send failure cancels the barrier',()=>{
  const {barrier,transport,errors}=fixture(),channel=new Channel();channel.readyState='connecting';
  channel.send=()=>{throw Error('Send failed');};transport.attach('guest',channel);
  assert.equal(barrier.state,'Awaiting');assert.equal(errors.length,0);
  channel.readyState='open';channel.dispatchEvent(new Event('open'));
  assert.equal(barrier.state,'Cancelled');assert.equal(errors.length,1);transport.close();
});

test('commit preparation requires complete agreement and every assigned remote channel',()=>{
  const {barrier,transport,ack}=fixture(),channel=new Channel();
  assert.throws(()=>transport.prepareCommit('host'));transport.attach('guest',channel);
  channel.message(ack);transport.acknowledgeLocal('host',JSON.parse(ack));
  assert.throws(()=>transport.prepareCommit('outsider'));
  const commit=transport.prepareCommit('host');assert.equal(commit.type,'session-start-commit');
  assert.equal(commit.session,barrier.proposal.session);commit.players.length=0;
  assert.equal(transport.prepareCommit('host').players.length,2);
  assert.equal(channel.sent.length,1);transport.close();assert.throws(()=>transport.prepareCommit('host'));
});
test('agreement without current remote channel cannot prepare a commit',()=>{
  const {barrier,transport,ack}=fixture();
  transport.acknowledgeLocal('host',JSON.parse(ack));transport.acknowledgeLocal('guest',JSON.parse(ack));
  assert.equal(barrier.state,'Agreed');assert.throws(()=>transport.prepareCommit('host'));
  assert.equal(barrier.state,'Cancelled');transport.close();
});

test('runtime commit broadcast validates host and sends without resuming it',()=>{
  const f=fixture(),channel=new Channel();f.transport.attach('guest',channel);
  channel.message(f.ack);f.transport.acknowledgeLocal('host',JSON.parse(f.ack));
  const runtime={assetId:f.snapshot.assetId,sessionSeed:1,checkpoint:42,
    engine:{phase:'Idle',preparedCheckpoint:()=>42,resume(){throw Error('Must not resume');}}};
  const commit=f.transport.sendRuntimeCommit('host',runtime);
  assert.deepEqual(channel.sent[1],commit);assert.equal(runtime.engine.phase,'Idle');f.transport.close();
});
test('partial commit delivery failure cancels and closes every connection',()=>{
  const snapshot={revision:1,membership:1,total:100,assetId:'a'.repeat(64),players:
    ['host','second','third'].map((peer,slot)=>({peer,slot,bytes:100,verified:true,shaders:true,ready:true}))};
  const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:1,buildId:'b'.repeat(64),checkpoint:42});
  const transport=new StartChannels(barrier,{snapshot:()=>snapshot}),first=new Channel(),second=new Channel();
  transport.attach('second',first);transport.attach('third',second);
  for(const {peer} of snapshot.players)transport.acknowledgeLocal(peer,{...barrier.proposal,type:'session-start-ack'});
  second.send=()=>{throw Error('Commit send failed');};
  const runtime={sessionSeed:1,checkpoint:42,engine:{phase:'Idle',preparedCheckpoint:()=>42}};
  assert.throws(()=>transport.sendRuntimeCommit('host',runtime),/Commit send failed/);
  assert.equal(first.sent[1].type,'session-start-commit');
  assert.equal(barrier.state,'Cancelled');assert.equal(first.readyState,'closed');assert.equal(second.readyState,'closed');
});
test('one failing close does not strand another start channel',()=>{
  const f=fixture(),first=new Channel(),second=new Channel();
  f.transport.attach('host',first);f.transport.attach('guest',second);
  first.close=()=>{throw Error('Close failed');};
  assert.throws(()=>f.transport.close(),AggregateError);assert.equal(second.readyState,'closed');
  assert.equal(f.barrier.state,'Cancelled');first.close=()=>{first.readyState='closed';};
  f.transport.close();assert.equal(first.readyState,'closed');
});


test('host handoff requires delivered commit and releases start listeners without closing session',()=>{
  const f=fixture(),channel=new Channel();f.transport.attach('guest',channel);
  channel.message(f.ack);f.transport.acknowledgeLocal('host',JSON.parse(f.ack));
  const runtime={sessionSeed:1,checkpoint:42,engine:{phase:'Idle',preparedCheckpoint:()=>42}};
  assert.throws(()=>f.transport.handoff('host',runtime),/Delivered/);
  f.transport.sendRuntimeCommit('host',runtime);
  assert.throws(()=>f.transport.handoff('host',{...runtime}),/Delivered/);
  const session=f.transport.handoff('host',runtime);
  assert.equal(session.runtime,runtime);assert.equal(session.channels[0].channel,channel);
  f.transport.close();assert.equal(channel.readyState,'open');
  channel.close();channel.message('broken');assert.equal(f.errors.length,0);assert.equal(f.barrier.state,'Agreed');
  assert.throws(()=>f.transport.handoff('host',runtime),/Delivered/);
});
test('lobby change after delivery prevents host handoff',()=>{
  const f=fixture(),channel=new Channel();f.transport.attach('guest',channel);
  channel.message(f.ack);f.transport.acknowledgeLocal('host',JSON.parse(f.ack));
  const runtime={sessionSeed:1,checkpoint:42,engine:{phase:'Idle',preparedCheckpoint:()=>42}};
  f.transport.sendRuntimeCommit('host',runtime);f.snapshot.revision++;
  assert.throws(()=>f.transport.handoff('host',runtime),/agreement/);
  f.transport.close();assert.equal(channel.readyState,'closed');
});
