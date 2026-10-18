import test from 'node:test';import assert from 'node:assert/strict';
import {prepareLobbyGameInput} from './lobby-game-input.mjs';import {lobbyInputOwnership} from './lobby-input-ownership.mjs';
test('four idle game input hooks merge each local PAD0 into global ownership without resuming engines',async()=>{
 const snapshot={membership:1,players:Array.from({length:4},(_,slot)=>({peer:'p'+slot,slot}))},session=new Uint8Array(16).fill(1),receivers=[],inputs=[],modules=[];
 for(let slot=0;slot<4;slot++){
  const module={_PartyBoard_GameFrameCount:()=>0,_PartyBoardBrowserRequireOnlineInput:value=>assert.equal(value,1)};modules.push(module);
  inputs.push(prepareLobbyGameInput({engine:{phase:'Idle'},module},{session,plan:lobbyInputOwnership(snapshot,'p'+slot),now:()=>0,
   bind:receive=>{receivers[slot]=receive;return {send:bytes=>receivers.forEach((fn,i)=>{if(i!==slot)fn(slot+1,bytes.slice());}),close(){}};}}));
 }
 modules.forEach((module,slot)=>{
  assert.deepEqual(module.partyBoardGlyphLocalPadSlots,Array.from({length:4},(_,lane)=>lane===slot?0:null));
  assert.ok(Object.isFrozen(module.partyBoardGlyphLocalPadSlots));
 });
 const merged=await Promise.all(modules.map((module,slot)=>{const pads=new Uint8Array(44);pads.fill(slot+10,0,11);return module.partyBoardInputExchange(pads,{checkpoint:42,frand:1,rand8:2});}));
 for(const bytes of merged)for(let slot=0;slot<4;slot++)assert.ok(bytes.slice(slot*11,slot*11+11).every(byte=>byte===slot+10));
 for(const input of inputs){const [record]=input.observations();assert.equal(record.frame,'0');assert.equal(record.checkpoint,42);assert.equal(record.frand,1);assert.equal(record.rand8,2);assert.deepEqual(record.pads,Array.from(merged[0]));record.pads[0]=99;assert.equal(input.observations()[0].pads[0],10);}
 inputs.forEach(input=>input.close());modules.forEach(module=>{
  assert.equal(module.partyBoardInputExchange,undefined);
  assert.equal(module.partyBoardGlyphLocalPadSlots,undefined);
 });
});
test('transport loss preserves the pending game poll and starts the existing sixty-second deadline',async()=>{
 const snapshot={membership:1,players:[{peer:'host',slot:0},{peer:'guest',slot:1}]},session=new Uint8Array(16).fill(1);
 let lost,time=0,settled=false;
 const module={_PartyBoard_GameFrameCount:()=>0,_PartyBoardBrowserRequireOnlineInput(){}};
 const input=prepareLobbyGameInput({engine:{phase:'Idle'},module},{session,plan:lobbyInputOwnership(snapshot,'host'),now:()=>time,
  bind:(_receive,disconnected)=>{lost=disconnected;return {send(){},close(){}};}});
 const poll=module.partyBoardInputExchange(new Uint8Array(44),{checkpoint:42,frand:1,rand8:2}).then(result=>{settled=true;return result;});
 lost([2]);await Promise.resolve();assert.equal(settled,false);assert.equal(input.queue.status(0).state,'Paused');
 assert.equal(input.bridge.pendingState().frame,0n);time=59999;assert.equal(input.queue.status(time).state,'Paused');
 time=60000;assert.equal(input.queue.status(time).state,'ReturnToMenu');input.bridge.notify();await assert.rejects(poll,/Complete merged pad input required/);
 input.close();assert.equal(module.partyBoardInputExchange,undefined);
});
