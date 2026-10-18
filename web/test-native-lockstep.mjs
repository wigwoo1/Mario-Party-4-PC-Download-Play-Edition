import {execFileSync} from 'node:child_process';
import assert from 'node:assert/strict';
import {Lockstep} from './lockstep.mjs';
import {encodeInput} from './input-packet.mjs';
if(!process.argv[2]) throw Error('Pass compiled lockstep-vectors executable');
const native=execFileSync(process.argv[2],[],{encoding:'utf8',timeout:10000}).trim().split(/\r?\n/);
assert.equal(native.length,612);
const session=Uint8Array.of(9,...Array(15).fill(0)), owners=[11,22,11,22];
const queue=new Lockstep(session,owners);
let hash=42;
for(let frame=0;frame<600;frame++) {
  // Deliberately differs from native arrival order.
  for(const slot of [0,2,1,3]) {
    const pad=Uint8Array.from({length:11},(_,i)=>(frame+slot*13+i)&255);
    const packet=encodeInput({session,frame:BigInt(frame),checkpoint:hash,slot,pad});
    assert.equal(queue.receive(owners[slot],packet),'Accepted');
    assert.equal(queue.receive(owners[slot],packet),'Duplicate');
  }
  const result=queue.advance(hash); assert.ok(result);
  for(const pad of result.pads) for(const byte of pad) hash=Math.imul(hash^byte,16777619)>>>0;
  assert.equal(`${result.index} ${hash}`,native[frame],`Native/JS mismatch at frame ${frame}`);
}
console.log('C++/JavaScript lockstep conformance passed: 600 per-frame input hashes');
const results=['Accepted','Duplicate','Stale','TooFar','WrongSession','Unauthorized','Malformed','Conflict','Divergence','Halted'];
const observed=[];
const emit=result=>observed.push(`RECEIVE ${results.indexOf(result)}`);
const make=(frame=0n,slot=0,checkpoint=42)=>encodeInput({session,frame,slot,checkpoint,pad:new Uint8Array(11)});
const negative=new Lockstep(session,[11,22,0,0]), packet=make();
emit(negative.receive(22,packet));
let bad=packet.slice(); bad[8]=8; emit(negative.receive(11,bad));
emit(negative.receive(11,packet.slice(0,47)));
emit(negative.receive(11,make(120n)));
emit(negative.receive(11,packet)); emit(negative.receive(11,packet));
bad=packet.slice(); bad[36]=1; emit(negative.receive(11,bad));
emit(negative.receive(11,packet));
const diverged=new Lockstep(session,[11,22,0,0]);
emit(diverged.receive(11,packet)); emit(diverged.receive(22,make(0n,1,43)));
const stale=new Lockstep(session,[11,0,0,0]);
stale.receive(11,packet); stale.advance(42); emit(stale.receive(11,packet));
const mismatch=new Lockstep(session,[11,0,0,0]); mismatch.receive(11,packet);
const advanced=mismatch.advance(43);
observed.push(`LOCAL ${Number(!!advanced)} ${['None','Conflict','Divergence','Cancelled','FrameLimit'].indexOf(mismatch.stopped)}`);
assert.deepEqual(observed,native.slice(600));
console.log('C++/JavaScript rejection conformance passed: 12 outcomes');
