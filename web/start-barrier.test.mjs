import {test} from 'node:test';
import assert from 'node:assert/strict';
import {LobbyState} from './lobby-state.mjs';
import {StartBarrier} from './start-barrier.mjs';
function fixture() {
  const lobby=new LobbyState('a'.repeat(64),100);
  lobby.join('host','Host');lobby.join('guest','Guest');
  for(const peer of ['host','guest'])lobby.report(peer,{assetId:lobby.assetId,sequence:1,bytes:100,
    verified:true,shaders:true,ready:true,membership:lobby.snapshot().membership});
  const barrier=new StartBarrier(lobby.snapshot(),{session:new Uint8Array(16).fill(1),seed:1,
    buildId:'b'.repeat(64),checkpoint:42});
  const ack={...barrier.proposal,type:'session-start-ack'};
  return {lobby,barrier,ack};
}
test('all assigned peers must agree; duplicates cannot replace another player',()=>{
  const {lobby,barrier,ack}=fixture();
  assert.ok(barrier.acknowledge('host',ack,lobby.snapshot()));
  assert.ok(barrier.acknowledge('host',ack,lobby.snapshot()));
  assert.equal(barrier.state,'Awaiting');assert.deepEqual(barrier.pending,['guest']);
  assert.equal(barrier.acknowledge('outsider',ack,lobby.snapshot()),false);
  assert.ok(barrier.acknowledge('guest',ack,lobby.snapshot()));assert.equal(barrier.state,'Agreed');
});
test('join or leave cancels a proposal, including an already agreed proposal',()=>{
  const {lobby,barrier,ack}=fixture();
  for(const peer of ['host','guest'])barrier.acknowledge(peer,ack,lobby.snapshot());
  lobby.leave('guest');assert.equal(barrier.update(lobby.snapshot()),false);
  assert.equal(barrier.state,'Cancelled');
  assert.equal(barrier.acknowledge('host',ack,lobby.snapshot()),false);
});
test('stale identity is rejected; same session with different initialized state blocks start',()=>{
  const {lobby,barrier,ack}=fixture();
  for(const change of [{session:'0'.repeat(32)},{membership:0},{seed:2},{buildId:'c'.repeat(64)},
    {assetId:'d'.repeat(64)},{profile:'save-enabled'},{players:undefined},{players:[]},
    {players:[{peer:'guest',slot:0},{peer:'host',slot:1}]}])
    assert.equal(barrier.acknowledge('guest',{...ack,...change},lobby.snapshot()),false);
  assert.equal(barrier.state,'Awaiting');
  assert.equal(barrier.acknowledge('guest',{...ack,checkpoint:43},lobby.snapshot()),false);
  assert.equal(barrier.state,'Mismatch');
  assert.equal(barrier.acknowledge('guest',ack,lobby.snapshot()),false);
});
test('proposal copies cannot change authoritative lineup or identity',()=>{
  const {barrier}=fixture(),proposal=barrier.proposal;
  proposal.players[0].peer='intruder';proposal.seed=99;
  assert.equal(barrier.proposal.seed,1);assert.equal(barrier.proposal.players[0].peer,'host');
});
test('four-player agreement remains blocked until the last acknowledgment',()=>{
  const {lobby}=fixture();lobby.join('third','Third');lobby.join('fourth','Fourth');
  for(const player of lobby.snapshot().players)lobby.report(player.peer,{assetId:lobby.assetId,
    sequence:player.sequence+1,bytes:100,verified:true,shaders:true,ready:true,membership:lobby.snapshot().membership});
  const snapshot=lobby.snapshot(),barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(2),
    seed:1,buildId:'b'.repeat(64),checkpoint:42}),ack={...barrier.proposal,type:'session-start-ack'};
  for(const peer of ['host','guest','third'])barrier.acknowledge(peer,ack,snapshot);
  assert.equal(barrier.state,'Awaiting');assert.deepEqual(barrier.pending,['fourth']);
  barrier.acknowledge('fourth',ack,snapshot);assert.equal(barrier.state,'Agreed');
  assert.throws(()=>new StartBarrier({...snapshot,total:undefined},{session:new Uint8Array(16).fill(2),
    seed:1,buildId:'b'.repeat(64),checkpoint:42}),/Lobby/);
});
test('unfinished shader preparation and subsequent readiness changes block agreement',()=>{
  const {lobby,barrier,ack}=fixture();
  lobby.report('guest',{assetId:lobby.assetId,sequence:2,bytes:100,verified:true,shaders:false,ready:false});
  assert.equal(barrier.acknowledge('host',ack,lobby.snapshot()),false);
  assert.equal(barrier.state,'Cancelled');
  assert.throws(()=>new StartBarrier(lobby.snapshot(),{session:new Uint8Array(16).fill(1),seed:1,
    buildId:'b'.repeat(64),checkpoint:42}),/preparation/);
});
