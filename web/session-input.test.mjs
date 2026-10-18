import test from 'node:test';
import assert from 'node:assert/strict';
import {SessionInput} from './session-input.mjs';
import {encodeInput} from './input-packet.mjs';
const session=Uint8Array.of(1,...Array(15).fill(0));
function fill(q,frame,now){for(const [slot,peer] of [[0,11],[1,22]])assert.equal(q.receive(peer,encodeInput({session,frame,slot,checkpoint:42,pad:new Uint8Array(11)}),now),'Accepted');}

test('queued complete frame remains frozen until explicit matching recovery and resume',()=>{
 const q=new SessionInput(session,[11,22,0,0]);fill(q,0n,0);
 assert.ok(q.disconnected(22,10));const token=q.status(10);
 assert.equal(q.advance(42,20),null);assert.equal(q.status(20).frame,0n);
 assert.equal(q.resume(token,21),false);
 assert.equal(q.recovered(22,{...token,frame:1n},22),false);
 assert.ok(q.recovered(22,token,23));assert.equal(q.advance(42,24),null);
 assert.ok(q.resume(token,25));assert.equal(q.advance(42,25).index,0n);
});
test('timeout cancels buffered input and cannot be recovered or resumed',()=>{
 const q=new SessionInput(session,[11,22,0,0]);fill(q,0n,0);q.disconnected(22,1);
 const token=q.status(1);assert.equal(q.advance(42,60000),null);
 assert.equal(q.status(60001).state,'ReturnToMenu');
 assert.equal(q.recovered(22,token,60002),false);assert.equal(q.resume(token,60002),false);
 assert.equal(q.advance(42,60002),null);assert.equal(q.receive(11,new Uint8Array(),60002),'Halted');
});
test('checkpoint divergence is terminal at session level',()=>{
 const q=new SessionInput(session,[11,22,0,0]);fill(q,0n,0);
 assert.equal(q.advance(43,1),null);
 assert.equal(q.status(1).state,'ReturnToMenu');assert.equal(q.status(1).stopReason,'Divergence');
});

test('pending missing input starts the interruption budget; successful polls clear its anchor',()=>{
 const q=new SessionInput(session,[11,22,0,0]);
 assert.equal(q.advance(42,100),null);
 assert.equal(q.advance(42,1000),null);
 assert.equal(q.disconnected(22,20440),true);
 let token=q.status(20440);assert.equal(token.remainingMs,39660);
 assert.equal(q.recovered(22,token,20440),true);assert.equal(q.resume(token,20440),true);
 fill(q,0n,20440);assert.equal(q.advance(42,20440).index,0n);
 // A completed poll must not leave its old wait attached to a later loss.
 assert.equal(q.disconnected(22,30000),true);token=q.status(30000);
 assert.equal(token.remainingMs,60000);
});

test('input that remains missing for sixty seconds expires immediately when loss is detected',()=>{
 const q=new SessionInput(session,[11,22,0,0]);
 assert.equal(q.advance(42,0),null);
 assert.equal(q.disconnected(22,60000),false);
 assert.equal(q.status(60000).state,'ReturnToMenu');
 assert.equal(q.status(60000).frame,0n);
 assert.equal(q.receive(11,new Uint8Array(),60000),'Halted');
});
