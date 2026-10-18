import test from 'node:test';
import assert from 'node:assert/strict';
import {ReconnectPolicy} from './reconnect-policy.mjs';
test('one deadline, stale attempts rejected, explicit synchronized resume',()=>{
  const p=new ReconnectPolicy([11,22,22,33]);
  assert.equal(p.disconnected(99,0),false);
  assert.ok(p.disconnected(22,100)); const epoch=p.epoch, old=p.revision;
  assert.deepEqual(p.missingSlots,[false,true,true,false]);
  assert.ok(p.disconnected(33,1000));
  assert.equal(p.remainingMs(1100),59000);
  assert.equal(p.markRecovered(22,epoch,old,1100),false);
  assert.ok(p.markRecovered(22,epoch,p.revision,1200));
  assert.equal(p.readyToResume(1200),false);
  assert.ok(p.markRecovered(33,epoch,p.revision,1300));
  assert.equal(p.state,'Paused');
  assert.equal(p.resume(epoch,old,1400),false);
  assert.ok(p.resume(epoch,p.revision,60099));
  assert.ok(p.disconnected(11,61000));
  assert.equal(p.markRecovered(11,p.epoch,p.revision,121000),false);
  assert.equal(p.state,'ReturnToMenu');
  assert.equal(p.resume(p.epoch,p.revision,121001),false);
});
test('recovery remains available until the exact 60-second boundary',()=>{
  const before=new ReconnectPolicy([11,22,33,44]);before.disconnected(22,100);
  assert.equal(before.remainingMs(60099),1);
  assert.equal(before.state,'Paused');
  assert.equal(before.markRecovered(22,before.epoch,before.revision,60099),true);
  const at=new ReconnectPolicy([11,22,33,44]);at.disconnected(22,100);
  assert.equal(at.remainingMs(60100),0);
  assert.equal(at.state,'ReturnToMenu');
  assert.equal(at.markRecovered(22,at.epoch,at.revision,60100),false);
});
test('clock failures and cancellation are terminal; fractional monotonic time works',()=>{
  for(const bad of [NaN,Infinity,-1,Number.MAX_SAFE_INTEGER+1]){
    const p=new ReconnectPolicy([11,0,0,0]);assert.equal(p.update(bad),'ReturnToMenu');
  }
  const p=new ReconnectPolicy([11,0,0,0]);
  p.disconnected(11,10.5);assert.equal(p.remainingMs(11),59999.5);
  assert.equal(p.update(10),'ReturnToMenu');
  const c=new ReconnectPolicy([11,0,0,0]);c.cancel();assert.equal(c.update(0),'ReturnToMenu');
});

test('delayed loss detection includes the existing input interruption without rewinding the clock',()=>{
 const p=new ReconnectPolicy([11,22,33,44]);
 p.update(20000);
 assert.equal(p.disconnected(22,20340,0),true);
 assert.equal(p.remainingMs(20340),39660);
 assert.equal(p.disconnected(33,25000,24000),true);
 assert.equal(p.remainingMs(25000),35000);
 assert.equal(p.markRecovered(22,p.epoch,p.revision,59999),true);
 assert.equal(p.markRecovered(33,p.epoch,p.revision,59999),true);
 assert.equal(p.resume(p.epoch,p.revision,59999),true);
 // A later completed interruption gets its own locally captured anchor.
 assert.equal(p.disconnected(22,80000,70000),true);
 assert.equal(p.remainingMs(80000),50000);
 assert.equal(p.remainingMs(130000),0);
 assert.equal(p.state,'ReturnToMenu');
});

test('already expired and invalid interruption anchors cannot extend recovery',()=>{
 for(const [observed,anchor] of [[60000,0],[60001,0],[100,101],[100,-1],[100,NaN],[100,Infinity]]){
  const p=new ReconnectPolicy([11,22,0,0]);
  assert.equal(p.disconnected(22,observed,anchor),false);
  assert.equal(p.state,'ReturnToMenu');
 }
});
