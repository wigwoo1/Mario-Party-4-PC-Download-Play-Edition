import test from 'node:test';
import assert from 'node:assert/strict';
import {Lockstep} from './lockstep.mjs';
import {encodeInput} from './input-packet.mjs';
const session=Uint8Array.of(1,...Array(15).fill(0));
const packet=(frame,slot=0,checkpoint=42,pad=new Uint8Array(11))=>encodeInput({session,frame,slot,checkpoint,pad});
test('two queues match 600 frames across ring wraps and reordered duplicates',()=>{
  const owners=[11,22,11,22], a=new Lockstep(session,owners), b=new Lockstep(session,owners);
  for(let frame=0n;frame<600n;frame++) {
    const packets=owners.map((_,slot)=>packet(frame,slot,Number(frame),new Uint8Array(11).fill(Number(frame+BigInt(slot))%256)));
    for(const slot of [3,1,0,2]) assert.equal(a.receive(owners[slot],packets[slot]),'Accepted');
    for(const slot of [0,2,1,3]) {
      assert.equal(b.receive(owners[slot],packets[slot]),'Accepted');
      assert.equal(b.receive(owners[slot],packets[slot]),'Duplicate');
    }
    assert.deepEqual(a.advance(Number(frame)),b.advance(Number(frame)));
    assert.equal(a.receive(11,packets[0]),'Stale');
  }
  assert.equal(a.nextFrame,600n);
});
test('ownership, bounded window, missing input and terminal divergence',()=>{
  const q=new Lockstep(session,[11,22,0,0]);
  assert.equal(q.receive(22,packet(0n)),'Unauthorized');
  assert.equal(q.receive(11,packet(120n)),'TooFar');
  assert.equal(q.receive(11,packet(10000000n)),'TooFar');
  assert.equal(q.receive(11,new Uint8Array(1)),'Malformed');
  const wrong=packet(0n); wrong[8]=2;
  assert.equal(q.receive(11,wrong),'WrongSession');
  assert.equal(q.receive(11,packet(0n)),'Accepted');
  assert.equal(q.missingSlots(),2); assert.equal(q.advance(42),null);
  assert.equal(q.receive(22,packet(0n,1)),'Accepted');
  const f=q.advance(42); assert.equal(f.pads[2][10],255);
  assert.equal(q.advance(42),null);
  q.receive(11,packet(1n)); q.receive(22,packet(1n,1));
  assert.equal(q.advance(43),null); assert.equal(q.stopped,'Divergence');
  assert.equal(q.receive(11,packet(1n)),'Halted');
  const conflict=new Lockstep(session,[11,0,0,0]);
  conflict.receive(11,packet(0n));
  assert.equal(conflict.receive(11,packet(0n,0,42,new Uint8Array(11).fill(1))),'Conflict');
  const cancelled=new Lockstep(session,[11,0,0,0]); cancelled.cancel();
  assert.equal(cancelled.advance(42),null);
});
