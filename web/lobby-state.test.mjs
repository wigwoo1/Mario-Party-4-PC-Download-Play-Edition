import {test} from 'node:test';
import assert from 'node:assert/strict';
import {LobbyState} from './lobby-state.mjs';
const id='a'.repeat(64);
const report=(sequence,bytes,verified=false,shaders=false,ready=false)=>({assetId:id,sequence,bytes,verified,shaders,ready,membership:4});

test('save agreement blocks start until every player acknowledges the current snapshot',()=>{
  const lobby=new LobbyState(id,100,{requireSave:true});
  lobby.join('h','Host');lobby.join('g','Guest');
  const send=(peer,sequence,saveHash)=>lobby.report(peer,{...report(sequence,100,true,true,true),membership:2,saveHash});
  for(const peer of ['h','g'])send(peer,1,null);
  assert.equal(lobby.snapshot().canStart,false);
  assert.equal(lobby.snapshot().players[0].phase,'Awaiting host save');
  const first='b'.repeat(64),second='c'.repeat(64);
  lobby.setSaveSnapshot(first);
  send('h',2,first);send('g',2,second);
  assert.equal(lobby.snapshot().canStart,false);
  send('g',3,first);assert.equal(lobby.snapshot().canStart,true);
  lobby.setSaveSnapshot(second);assert.equal(lobby.snapshot().canStart,false);
  send('h',3,first);send('g',4,first);assert.equal(lobby.snapshot().canStart,false);
  send('h',4,second);send('g',5,second);assert.equal(lobby.snapshot().canStart,true);
  const before=lobby.snapshot();
  assert.throws(()=>send('h',5,'invalid'));assert.deepEqual(lobby.snapshot(),before);
});
test('ready reports crossing a membership change cannot rearm the barrier',()=>{
  const lobby=new LobbyState(id,100);lobby.join('a','A');lobby.join('b','B');
  const old=lobby.snapshot().membership;
  for(const peer of ['a','b'])lobby.report(peer,{...report(1,100,true,true,true),membership:old});
  assert.equal(lobby.snapshot().canStart,true);
  lobby.join('c','C');lobby.leave('c');
  for(const peer of ['a','b'])lobby.report(peer,{...report(2,100,true,true,true),membership:old});
  assert.equal(lobby.snapshot().canStart,false);
  assert.ok(lobby.snapshot().players.every(p=>p.verified&&p.shaders&&!p.ready));
  for(const peer of ['a','b'])lobby.report(peer,{...report(3,100,true,true,true),membership:lobby.snapshot().membership});
  assert.equal(lobby.snapshot().canStart,true);
});
test('four independent download bars gate start on verification, shaders and ready',()=>{
  const lobby=new LobbyState(id,100);
  for(let i=0;i<4;i++) lobby.join('p'+i,'Player '+i);
  assert.throws(()=>lobby.join('extra','Extra'));
  for(let i=0;i<4;i++) lobby.report('p'+i,report(1,i*25));
  assert.deepEqual(lobby.snapshot().players.map(p=>p.progress),[0,.25,.5,.75]);
  for(let i=0;i<4;i++) lobby.report('p'+i,report(2,100,true,true,i!==3));
  assert.equal(lobby.snapshot().canStart,false);
  lobby.report('p3',report(3,100,true,true,true));
  assert.equal(lobby.snapshot().canStart,true);
  lobby.leave('p2'); assert.equal(lobby.snapshot().canStart,false);
  assert.equal(lobby.join('new','New'),2);
});
test('rejects unknown peers, stale reports, wrong assets and premature readiness atomically',()=>{
  const lobby=new LobbyState(id,100); lobby.join('p','Player'); lobby.report('p',report(2,40));
  const before=lobby.snapshot();
  for(const value of [report(2,50),report(3,30),report(3,101),report(3,40,true),report(3,100,false,true),report(3,100,true,false,true),{...report(3,50),assetId:'b'.repeat(64)}]) assert.throws(()=>lobby.report('p',value));
  assert.throws(()=>lobby.report('unknown',report(3,100,true,true,true)));
  assert.deepEqual(lobby.snapshot(),before);
  const copy=lobby.snapshot(); copy.players[0].ready=true;
  assert.deepEqual(lobby.snapshot(),before);
});
