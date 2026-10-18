const test=require('node:test'),assert=require('node:assert/strict');
const {orderInitialInvitations}=require('./initial-invitation-order.cjs');
test('out-of-order ICE completions preserve the correct native and browser guest identities',()=>{
 const one={type:'initial-invitation',peer:'guest1',code:'native-code'},two={type:'initial-invitation',peer:'guest2',code:'browser-two'},three={type:'initial-invitation',peer:'guest3',code:'browser-three'};
 for(const input of [[two,one,three],[three,two,one],[one,three,two]]){
  const original=input.slice(),ordered=orderInitialInvitations(input,3);
  assert.deepEqual(input,original);assert.equal(ordered[0],one);assert.equal(ordered[1],two);assert.equal(ordered[2],three);
 }
 assert.deepEqual(orderInitialInvitations([one],1),[one]);
});
test('missing, duplicated or unexpected identities cannot be silently reassigned',()=>{
 const one={type:'initial-invitation',peer:'guest1'};
 assert.throws(()=>orderInitialInvitations([one],3),/Complete/);
 assert.throws(()=>orderInitialInvitations([one,one,one],3),/duplicate/);
 assert.throws(()=>orderInitialInvitations([{...one,peer:'guest2'}],1),/Unexpected/);
 assert.throws(()=>orderInitialInvitations([{...one,type:'other'}],1),/Unexpected/);
 assert.throws(()=>orderInitialInvitations([],0),/bounded/);
});
