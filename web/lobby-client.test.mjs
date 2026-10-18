import {test} from 'node:test';
import assert from 'node:assert/strict';
import {LobbyClient} from './lobby-client.mjs';
import {LobbyState} from './lobby-state.mjs';
const id='a'.repeat(64);
const pack=snapshot=>JSON.stringify({type:'lobby-snapshot',version:1,snapshot});

test('client independently enforces snapshot agreement despite a claimed ready flag',()=>{
  const host=new LobbyState(id,100,{requireSave:true});host.join('h','Host');host.join('g','Guest');
  host.setSaveSnapshot('b'.repeat(64));
  for(const peer of ['h','g'])host.report(peer,{assetId:id,sequence:1,bytes:100,verified:true,shaders:true,ready:true,membership:2,saveHash:'b'.repeat(64)});
  const good=host.snapshot();
  assert.equal(new LobbyClient(id,100).receive(pack(good)).canStart,true);
  for(const saveHash of [null,'c'.repeat(64)]) {
    const bad=structuredClone(good);bad.players[1].saveHash=saveHash;
    const result=new LobbyClient(id,100).receive(pack(bad));
    assert.equal(result.canStart,false);assert.equal(result.players[1].phase,'Awaiting host save');
  }
  const malformed=structuredClone(good);malformed.saveHash='invalid';
  assert.throws(()=>new LobbyClient(id,100).receive(pack(malformed)));
});
test('client accepts host state, recomputes display/readiness and ignores replay',()=>{
  const host=new LobbyState(id,100); host.join('h','Host');host.join('g','Guest');
  const client=new LobbyClient(id,100),snapshot=host.snapshot();
  snapshot.canStart=true;snapshot.players[0].progress=1;snapshot.players[0].phase='Ready';
  const result=client.receive(pack(snapshot));
  assert.equal(result.canStart,false);assert.equal(result.players[0].progress,0);
  assert.equal(result.players[0].phase,'Downloading assets');
  assert.equal(client.receive(pack(snapshot)),null);
});
test('malformed snapshots cannot advance revision or cross asset boundaries',()=>{
  const host=new LobbyState(id,100);host.join('h','Host');host.join('g','Guest');
  const client=new LobbyClient(id,100),original=host.snapshot();
  for(const mutate of [s=>s.assetId='b'.repeat(64),s=>s.players[1].slot=0,s=>s.players[1].peer='h',s=>s.players[0].ready=true,s=>s.players[0].bytes=101,s=>s.players.push(...s.players,...s.players)]) {
    const s=structuredClone(original);mutate(s);assert.throws(()=>client.receive(pack(s)));
  }
  assert.throws(()=>client.receive('x'.repeat(16385)));
  assert.deepEqual(client.receive(pack(original)),original);
});
