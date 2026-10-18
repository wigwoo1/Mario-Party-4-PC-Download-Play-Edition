import {test} from 'node:test';
import assert from 'node:assert/strict';
import {LobbyState} from './lobby-state.mjs';
import {LobbyHost} from './lobby-host.mjs';
import {LobbyClient} from './lobby-client.mjs';
class Channel extends EventTarget {
  readyState='open'; messages=[];
  send(data){this.messages.push(JSON.parse(data));}
  close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}
  report(report){this.dispatchEvent(new MessageEvent('message',{data:JSON.stringify({type:'lobby-report',version:1,report})}));}
}
const id='a'.repeat(64);
const report=bytes=>({assetId:id,sequence:1,bytes,verified:false,shaders:false,ready:false});

test('repeated guest replacement removes old listeners without changing current lobby',()=>{
  class TrackedChannel extends Channel {
    listeners=new Set();
    addEventListener(type,fn,...args){this.listeners.add(fn);super.addEventListener(type,fn,...args);}
    removeEventListener(type,fn,...args){this.listeners.delete(fn);super.removeEventListener(type,fn,...args);}
  }
  const state=new LobbyState(id,100);state.join('host','Host');
  const host=new LobbyHost(state),removed=[];
  for(let i=0;i<32;i++) {
    const channel=new TrackedChannel();host.attach('guest','Guest',channel);
    assert.equal(channel.listeners.size,3);
    if(i%2)channel.close();else host.detach('guest');
    assert.equal(channel.listeners.size,0);removed.push(channel);
  }
  const current=new TrackedChannel();host.attach('guest','Current',current);
  const before=state.snapshot();
  for(const old of removed){old.report(report(100));old.dispatchEvent(new Event('open'));old.close();}
  assert.deepEqual(state.snapshot(),before);assert.equal(current.listeners.size,3);
  host.close();assert.equal(current.listeners.size,0);
  assert.deepEqual(state.snapshot().players.map(p=>p.peer),['host']);
});
test('three guests receive identical four-player snapshots and cannot report for another peer',()=>{
  const state=new LobbyState(id,100);state.join('host','Host');
  const host=new LobbyHost(state), channels=[new Channel(),new Channel(),new Channel()];
  channels.forEach((channel,i)=>host.attach('p'+i,'Player '+i,channel));
  channels[1].report({...report(50),peer:'host'});
  const snapshot=state.snapshot();
  assert.equal(snapshot.players.find(p=>p.peer==='host').bytes,0);
  assert.equal(snapshot.players.find(p=>p.peer==='p1').bytes,50);
  for(const channel of channels)assert.deepEqual(channel.messages.at(-1).snapshot,snapshot);
  channels[1].close();
  assert.equal(state.snapshot().players.length,3);
  assert.deepEqual(channels[0].messages.at(-1).snapshot,channels[2].messages.at(-1).snapshot);
});
test('malformed peer is removed without changing other progress',()=>{
  const state=new LobbyState(id,100);state.join('host','Host');
  const errors=[],host=new LobbyHost(state,{error:(peer)=>errors.push(peer)}),channel=new Channel();
  host.attach('bad','Bad',channel);channel.report(report(101));
  assert.deepEqual(errors,['bad']);assert.equal(channel.readyState,'closed');
  assert.deepEqual(state.snapshot().players.map(p=>p.peer),['host']);
});

test('host close releases every guest despite failure and retries only failed closes',()=>{
  const state=new LobbyState(id,100);state.join('host','Host');
  let changed=0;const host=new LobbyHost(state,{changed:()=>changed++});
  const channels=[new Channel(),new Channel(),new Channel()],attempts=[0,0,0];
  channels.forEach((channel,i)=>{
    host.attach('p'+i,'Player '+i,channel);
    channel.close=()=>{attempts[i]++;host.close();if(i===0&&attempts[i]===1)throw Error('Close failed');channel.readyState='closed';};
  });
  const before=changed;
  assert.throws(()=>host.close(),AggregateError);
  assert.deepEqual(attempts,[1,1,1]);assert.equal(changed,before);
  assert.deepEqual(state.snapshot().players.map(p=>p.peer),['host']);
  assert.throws(()=>host.attach('new','New',new Channel()),/Unavailable/);
  assert.throws(()=>host.reportLocal('host',report(10)),/closed/);
  host.close();host.close();assert.deepEqual(attempts,[2,1,1]);
  assert.ok(channels.every(channel=>channel.readyState==='closed'));
});


test('host save replacement broadcasts and invalidates ready across client views',()=>{
  const state=new LobbyState(id,100,{requireSave:true});state.join('host','Host');
  const host=new LobbyHost(state),channel=new Channel(),client=new LobbyClient(id,100);
  host.attach('guest','Guest',channel);
  const first='b'.repeat(64),next='c'.repeat(64);
  host.setSaveSnapshot(first);
  const ready={assetId:id,sequence:1,bytes:100,verified:true,shaders:true,ready:true,membership:state.snapshot().membership,saveHash:first};
  host.reportLocal('host',ready);channel.report(ready);
  assert.equal(client.receive(JSON.stringify(channel.messages.at(-1))).canStart,true);
  host.setSaveSnapshot(next);
  let view=client.receive(JSON.stringify(channel.messages.at(-1)));
  assert.equal(view.saveHash,next);assert.equal(view.canStart,false);
  assert.ok(view.players.every(p=>!p.ready&&p.saveHash===null));
  channel.report({...ready,sequence:2});
  view=client.receive(JSON.stringify(channel.messages.at(-1)));
  assert.equal(view.canStart,false);assert.equal(view.players[1].ready,false);
  const revised={...ready,sequence:3,saveHash:next};
  channel.report(revised);host.reportLocal('host',revised);
  assert.equal(client.receive(JSON.stringify(channel.messages.at(-1))).canStart,true);
  const before=channel.messages.length;
  assert.throws(()=>host.setSaveSnapshot('invalid'),/Invalid save/);
  assert.equal(channel.messages.length,before);
});
test('running membership freeze retains lost players until owned session teardown',()=>{
 const state=new LobbyState(id,100);state.join('host','Host');const host=new LobbyHost(state),channel=new Channel();
 host.attach('guest','Guest',channel);const before=host.freezeMembership();
 channel.close();channel.report(report(50));assert.deepEqual(state.snapshot(),before);
 assert.throws(()=>host.attach('replacement','Replacement',new Channel()),/Unavailable/);
 assert.throws(()=>host.detach('guest'),/owned session teardown/);
 assert.throws(()=>host.reportLocal('host',report(50)),/unavailable/);
 host.close();assert.deepEqual(state.snapshot().players.map(player=>player.peer),['host']);
});
