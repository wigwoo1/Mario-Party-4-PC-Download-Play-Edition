import test from 'node:test';import assert from 'node:assert/strict';
import {receiveSessionInputLoss,sessionInputLossNotice} from './session-input-loss.mjs';
const session='a'.repeat(32),plan={peer:'guest1',bindings:[{peer:'host',owner:1},{peer:'guest1',owner:2},{peer:'guest2',owner:3}]};
test('bound-host input loss notice rejects foreign sessions, unassigned peers and local-owner loss',()=>{
 const notice=sessionInputLossNotice('guest2',{session,plan});assert.equal(receiveSessionInputLoss(notice,{session,plan}),'guest2');
 for(const bad of [{...notice,session:'b'.repeat(32)},{...notice,peer:'unknown'},{...notice,peer:'guest1'},{...notice,version:2}])assert.throws(()=>receiveSessionInputLoss(bad,{session,plan}),/Invalid/);
});
