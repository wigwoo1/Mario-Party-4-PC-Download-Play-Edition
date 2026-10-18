import test from 'node:test';import assert from 'node:assert/strict';
import {lobbyInputOwnership,requireCurrentInputOwnership} from './lobby-input-ownership.mjs';
test('global player slots map every peer keyboard/controller zero into its owned lane',()=>{
 const snapshot={membership:4,players:[{peer:'host',slot:0},{peer:'guest5',slot:3},{peer:'guest2',slot:1},{peer:'guest3',slot:2}]};
 for(const player of snapshot.players){const plan=lobbyInputOwnership(snapshot,player.peer);assert.deepEqual(plan.owners,[1,2,3,4]);assert.equal(plan.owner,player.slot+1);assert.equal(plan.localPadSlots[player.slot],0);assert.equal(plan.localPadSlots.filter(n=>n!==null).length,1);assert.equal(requireCurrentInputOwnership(plan,snapshot),plan);assert.throws(()=>requireCurrentInputOwnership(plan,{...snapshot,membership:5}),/changed/);}
 const plan=lobbyInputOwnership(snapshot,'host');assert.throws(()=>requireCurrentInputOwnership(plan,{...snapshot,players:snapshot.players.map(p=>p.slot===3?{...p,peer:'guest6'}:p)}),/changed/);
 assert.throws(()=>lobbyInputOwnership({...snapshot,players:[snapshot.players[0],snapshot.players[0]]},'host'),/Unique/);
 assert.deepEqual(lobbyInputOwnership({membership:1,players:[{peer:'guest5',slot:3}]},'guest5').owners,[0,0,0,4]);
});
