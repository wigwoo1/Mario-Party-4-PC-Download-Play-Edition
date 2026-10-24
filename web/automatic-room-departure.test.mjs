import test from 'node:test';
import assert from 'node:assert/strict';
import {automaticRoom} from './automatic-room.mjs';

async function fixture(t){
 let receive,finishWatch,offer;
 const sent=[],removed=[],errors=[];
 const client={
  host:async()=>({room:'a'.repeat(22),joinKey:'b'.repeat(43)}),
  watch:handler=>{receive=handler;return new Promise(resolve=>{finishWatch=resolve;});},
  send:async(to,kind)=>{sent.push({to,kind});},removeGuest:async peer=>removed.push(peer),close:async()=>finishWatch?.()
 };
 const until=async predicate=>{const deadline=Date.now()+1000;while(!predicate()){assert.ok(Date.now()<deadline,'Fixture transition must settle');await new Promise(resolve=>setTimeout(resolve,1));}};
 const room=automaticRoom({baseURL:'http://127.0.0.1/',client,createOffer:async()=>'fixture-offer',applyAnswer:async()=>'transport1',
  createRecoveryOffer:()=>new Promise((resolve,reject)=>{offer={resolve,reject};}),error:error=>errors.push(error.message)});
 t.after(()=>room.close());await room.host();await receive({type:'joined',peer:'guest1'});await until(()=>sent.length===1);
 await receive({type:'signal',kind:'answer',from:'guest1',payload:'fixture-answer'});
 return {room,sent,removed,errors,leave:()=>receive({type:'left',peer:'guest1'}),
  async begin(){offer=undefined;const result=room.recover('transport1');await until(()=>offer);return {result,...offer};}};
}

test('guest departure cancels an offer that is still being prepared',async t=>{
 const f=await fixture(t),offer=await f.begin();await f.leave();offer.resolve('signed-fixture');await offer.result;
 assert.deepEqual(f.sent,[{to:'guest1',kind:'offer'}]);assert.deepEqual(f.errors,[]);
});
test('explicit removal cancels the pending offer without sending to the removed guest',async t=>{
 const f=await fixture(t),offer=await f.begin();await f.room.remove('transport1');offer.resolve('signed-fixture');await offer.result;
 assert.deepEqual(f.removed,['guest1']);assert.deepEqual(f.sent,[{to:'guest1',kind:'offer'}]);
});
test('closing during offer preparation sends no reconnect message',async t=>{
 const f=await fixture(t),offer=await f.begin();await f.room.close();offer.resolve('signed-fixture');await offer.result;
 assert.deepEqual(f.sent,[{to:'guest1',kind:'offer'}]);assert.deepEqual(f.errors,[]);
});
test('a preparation error after departure is canceled with the departed attempt',async t=>{
 const f=await fixture(t),offer=await f.begin();await f.leave();offer.reject(Error('Canceled transport'));await offer.result;
 assert.deepEqual(f.sent,[{to:'guest1',kind:'offer'}]);assert.deepEqual(f.errors,[]);
});
test('active guest errors remain visible and a healthy retry can still send',async t=>{
 const f=await fixture(t),first=await f.begin(),failure=Error('Active transport failure');
 const rejected=assert.rejects(first.result,error=>error===failure);first.reject(failure);await rejected;
 const retry=await f.begin();retry.resolve('signed-fixture');await retry.result;
 assert.deepEqual(f.sent,[{to:'guest1',kind:'offer'},{to:'guest1',kind:'recovery-offer'}]);assert.deepEqual(f.errors,[]);
});
