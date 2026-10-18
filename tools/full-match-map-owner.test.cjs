const test=require('node:test'),assert=require('node:assert/strict');
const {mapViewOwnerIndex}=require('./drive-production-full-match.cjs');
test('map owner excludes JSON null remote lanes from all four peers',()=>{
 const states=Array.from({length:4},(_,peer)=>({input:{localPadSlots:Array.from({length:4},(_,port)=>port===peer?0:null)}}));
 for(let port=0;port<4;port++)assert.equal(mapViewOwnerIndex(states,port),port);
});
test('map owner rejects missing and duplicate ownership and invalid physical bindings',()=>{
 for(const binding of [null,undefined,-1,4,NaN,0.5,'0']){
  assert.throws(()=>mapViewOwnerIndex([{input:{localPadSlots:[binding]}}],0),/exactly one local owner/);
 }
 assert.throws(()=>mapViewOwnerIndex([{input:{localPadSlots:[0]}},{input:{localPadSlots:[1]}}],0),/exactly one local owner/);
 assert.equal(mapViewOwnerIndex([{input:{localPadSlots:[null]}},{input:{localPadSlots:[3]}}],0),1);
});
