const {test}=require('node:test'),assert=require('node:assert/strict');
const {assignedSlot}=require('./drive-automatic-full-match.cjs');
test('automatic full-match adapter preserves actual lobby controller assignments',async()=>{
 const {lobbyInputOwnership}=await import('../../web/lobby-input-ownership.mjs');
 const snapshot={membership:1,players:Array.from({length:4},(_,slot)=>({peer:slot===0?'host':'guest'+slot,slot}))};
 for(const player of snapshot.players){const plan=lobbyInputOwnership(snapshot,player.peer);const slot=assignedSlot(plan.owner);assert.equal(slot,player.slot);assert.equal(plan.localPadSlots[slot],0);}
 for(const owner of [0,5,8,null,undefined,1.5])assert.throws(()=>assignedSlot(owner));
});
