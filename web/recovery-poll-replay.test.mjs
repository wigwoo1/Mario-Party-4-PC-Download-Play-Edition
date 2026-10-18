import test from 'node:test';import assert from 'node:assert/strict';
import {validateRecoveryPollReplay} from './recovery-poll-replay.mjs';
import {pausedStateReport} from './paused-state-agreement.mjs';
import {encodeInput,decodeInput} from './input-packet.mjs';
import {SessionInput} from './session-input.mjs';
import {createSessionInputExchange} from './session-input-exchange.mjs';
function fixture(){
 const identity={session:'01'.repeat(16),buildId:'b'.repeat(64),assetId:'a'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,players:[{peer:'host',slot:0},{peer:'guest1',slot:1}]};
 const captured={attempt:{peer:22,epoch:1n,revision:1n,frame:7n,checkpoint:42,frand:3,rand8:5},state:{frame:10,checkpoint:42,frand:3,rand8:5},input:{state:'Paused',frame:7n,epoch:1n,revision:1n}};
 const nonce='c'.repeat(32),local=pausedStateReport(identity,captured,{peer:'host',nonce}),pads=new Uint8Array(44);pads.fill(7,0,11);pads.fill(9,11,22);pads[32]=pads[43]=255;
 return {identity,captured,nonce,peer:'host',remotePeer:'guest1',owners:[11,22,0,0],report:{...local,reporter:'guest1',frame:11,inputFrame:'8',checkpoint:99},
  receipt:{frame:'7',checkpoint:42,frand:3,rand8:5,pads:Array.from(pads)},pendingPackets:[encodeInput({session:new Uint8Array(16).fill(1),frame:7n,checkpoint:42,slot:0,pad:pads.slice(0,11)})]};
}
test('one-poll replay preserves original local sample and returns every assigned candidate packet',()=>{
 const options=fixture(),before=structuredClone(options.captured),result=validateRecoveryPollReplay(options);
 assert.deepEqual(options.captured,before);assert.deepEqual(result.packets.map(packet=>packet.owner),[11,22]);
 assert.deepEqual(result.packets.map(packet=>decodeInput(packet.bytes).frame),[7n,7n]);
 assert.equal(decodeInput(result.packets[1].bytes).pad[0],9);assert.equal(result.target.checkpoint,99);
 options.receipt.pads.fill(0);assert.equal(decodeInput(result.packets[0].bytes).pad[0],7);
});
test('session mismatch, wrong reporter, larger gap, changed RNG and original input are rejected',()=>{
 const mutations=[o=>o.report.nonce='d'.repeat(32),o=>o.report.buildId='d'.repeat(64),o=>o.report.reporter='guest2',
  o=>o.report.inputFrame='9',o=>o.report.frame=12,o=>o.receipt.checkpoint++,o=>o.receipt.frand++,o=>o.receipt.rand8++,
  o=>o.receipt.pads[0]++,o=>o.receipt.pads[22]=1,o=>o.receipt.pads[32]=0,o=>o.receipt.pads[1]=256,
  o=>o.pendingPackets=[],o=>o.pendingPackets.push(o.pendingPackets[0]),o=>o.report.extra=true,o=>o.receipt.extra=true];
 for(const mutate of mutations){const options=fixture();mutate(options);assert.throws(()=>validateRecoveryPollReplay(options));}
});
test('four assigned players and multi-slot local ownership preserve original samples',()=>{
 const options=fixture();options.identity.players.push({peer:'guest2',slot:2},{peer:'guest3',slot:3});options.owners=[11,22,11,33];
 options.report.players=structuredClone(options.identity.players);options.receipt.pads.fill(6,22);
 options.pendingPackets.push(encodeInput({session:new Uint8Array(16).fill(1),frame:7n,checkpoint:42,slot:2,pad:new Uint8Array(11).fill(6)}));
 assert.equal(validateRecoveryPollReplay(options).packets.length,4);
 options.pendingPackets.pop();assert.throws(()=>validateRecoveryPollReplay(options),/Complete assigned/);
});
test('validated replay completes original promise once, preserving pause, missing owners and deadline',async()=>{
 const options=fixture(),session=new Uint8Array(16).fill(1),queue=new SessionInput(session,options.owners);let time=0;
 const bridge=createSessionInputExchange(queue,{session,owners:options.owners,peer:11,suspendOnPause:true,
  checkpoint:metadata=>metadata.checkpoint,send:()=>{},now:()=>time});
 // Consume the earlier polls normally so this is a real pending queue at seven.
 for(let frame=0n;frame<7n;frame++){
  const wait=bridge.exchange(new Uint8Array(44),{checkpoint:42,frand:3,rand8:5});
  queue.receive(22,encodeInput({session,frame,checkpoint:42,slot:1,pad:new Uint8Array(11)}),time);bridge.notify();await wait;
 }
 const pads=Uint8Array.from(options.receipt.pads),wait=bridge.exchange(pads,{checkpoint:42,frand:3,rand8:5});
 queue.disconnected(22,time);time=500;
 const before=queue.status(time);options.captured.input=before;options.captured.attempt={peer:22,...before,...bridge.pendingState()};
 const target=bridge.replayCompletedPoll(options);assert.equal(target.checkpoint,99);
 assert.deepEqual(await wait,pads);const after=queue.status(time);
 assert.equal(after.state,'Paused');assert.equal(after.frame,8n);assert.equal(after.remainingMs,before.remainingMs);
 assert.equal(after.epoch,before.epoch);assert.equal(after.revision,before.revision);assert.deepEqual(after.missingSlots,before.missingSlots);
 assert.throws(()=>bridge.replayCompletedPoll(options),/original paused poll/);
 let settled=false;const next=bridge.exchange(new Uint8Array(44).fill(55),{checkpoint:99,frand:3,rand8:5}).then(value=>{settled=true;return value;});
 await Promise.resolve();assert.equal(settled,false);bridge.notify();assert.equal(queue.status(time).frame,8n);
 // A second candidate in this same epoch cannot consume another poll.
 const second=fixture();second.captured.input=queue.status(time);second.captured.attempt={peer:22,...second.captured.input,...bridge.pendingState()};
 second.captured.state={frame:11,checkpoint:99,frand:3,rand8:5};
 second.report={...pausedStateReport(second.identity,second.captured,{peer:second.peer,nonce:second.nonce}),reporter:second.remotePeer,frame:12,inputFrame:'9',checkpoint:100};
 second.receipt={frame:'8',checkpoint:99,frand:3,rand8:5,pads:Array(44).fill(55)};second.receipt.pads.fill(0,22);second.receipt.pads[32]=second.receipt.pads[43]=255;
 assert.throws(()=>bridge.replayCompletedPoll(second),/consumption rejected/);assert.equal(queue.status(time).frame,8n);
 assert.equal(queue.receive(22,encodeInput({session,frame:8n,checkpoint:99,slot:1,pad:new Uint8Array(11)}),time),'Accepted');
 time=60000;bridge.notify();assert.equal(await next,null);assert.equal(queue.status(time).state,'ReturnToMenu');
});
test('replay conflicting with already received remote input halts instead of replacing it',async()=>{
 const options=fixture(),session=new Uint8Array(16).fill(1),queue=new SessionInput(session,options.owners);
 const bridge=createSessionInputExchange(queue,{session,owners:options.owners,peer:11,suspendOnPause:true,
  checkpoint:()=>42,send:()=>{},now:()=>0});
 const wait=bridge.exchange(Uint8Array.from(options.receipt.pads),{frand:3,rand8:5});queue.disconnected(22,0);
 options.captured.input=queue.status(0);options.captured.attempt={peer:22,...options.captured.input,...bridge.pendingState()};
 options.report.inputFrame='1';options.receipt.frame='0';
 queue.receive(22,encodeInput({session,frame:0n,checkpoint:42,slot:1,pad:new Uint8Array(11).fill(8)}),0);
 assert.throws(()=>bridge.replayCompletedPoll(options),/Conflict/);assert.equal(await wait,null);
 assert.equal(queue.status(0).state,'ReturnToMenu');assert.equal(queue.status(0).frame,0n);
});
