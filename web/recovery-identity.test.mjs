import test from 'node:test';import assert from 'node:assert/strict';
import {createRecoveryCredential,receiveRecoveryCredential,signRecoveryDescription,verifyRecoveryDescription} from './recovery-identity.mjs';
const configuration={session:'1'.repeat(32),assetId:'a'.repeat(64)};
test('replacement descriptions authenticate original peer, session, asset and exact SDP before consuming replay nonce',async()=>{
 const credential=createRecoveryCredential(configuration,'guest1');
 const packet=await signRecoveryDescription(credential,{type:'offer',sdp:'v=0\r\na=fingerprint:sha-256 ORIGINAL'});
 const usedNonces=new Set();
 assert.equal(await verifyRecoveryDescription({...packet,description:{...packet.description,sdp:'v=0\r\na=fingerprint:sha-256 ATTACKER'}},credential,{type:'offer',usedNonces}),null);
 assert.equal(usedNonces.size,0);
 for(const altered of [{...packet,peer:'guest2'},{...packet,session:'2'.repeat(32)},{...packet,assetId:'b'.repeat(64)}])
  await assert.rejects(verifyRecoveryDescription(altered,credential,{type:'offer',usedNonces}),/Invalid/);
 const accepted=await verifyRecoveryDescription(packet,credential,{type:'offer',usedNonces});assert.equal(accepted.description.sdp,packet.description.sdp);
 assert.equal(await verifyRecoveryDescription(packet,credential,{type:'offer',usedNonces}),null);
});
test('concurrent replay accepts once and answers bind to the offered nonce',async()=>{
 const credential=createRecoveryCredential(configuration,'guest1'),packet=await signRecoveryDescription(credential,{type:'offer',sdp:'offer'}),usedNonces=new Set();
 const results=await Promise.all([1,2].map(()=>verifyRecoveryDescription(packet,credential,{type:'offer',usedNonces})));
 assert.equal(results.filter(Boolean).length,1);
 const answer=await signRecoveryDescription(credential,{type:'answer',sdp:'answer'},{nonce:packet.nonce});
 assert.equal(await verifyRecoveryDescription(answer,credential,{type:'answer',usedNonces,expectedNonce:'0'.repeat(32)}),null);
 assert.ok(await verifyRecoveryDescription(answer,credential,{type:'answer',usedNonces,expectedNonce:packet.nonce}));
 await assert.rejects(verifyRecoveryDescription(packet,credential,{type:'answer',usedNonces,expectedNonce:packet.nonce}),/Invalid/);
});
test('credentials remain bound and immutable; a new unrelated peer credential cannot authenticate replacement',async()=>{
 const credential=createRecoveryCredential(configuration,'guest1');
 assert.equal(receiveRecoveryCredential({...credential},{...configuration,peer:'guest1',current:credential}),credential);
 assert.throws(()=>receiveRecoveryCredential({...credential,secret:'b'.repeat(64)},{...configuration,peer:'guest1',current:credential}),/changed/);
 const packet=await signRecoveryDescription(credential,{type:'offer',sdp:'offer'}),other=createRecoveryCredential(configuration,'guest1');
 assert.equal(await verifyRecoveryDescription(packet,other,{type:'offer',usedNonces:new Set()}),null);
 await assert.rejects(signRecoveryDescription(credential,{type:'offer',sdp:'x'.repeat(100001)}),/Invalid/);
});
