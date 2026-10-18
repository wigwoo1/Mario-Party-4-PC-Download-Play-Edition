import test from 'node:test';import assert from 'node:assert/strict';
import {createRecoveryCredential,signRecoveryDescription} from './recovery-identity.mjs';
import {createReplacementTransport} from './replacement-transport.mjs';
const credential=createRecoveryCredential({session:'1'.repeat(32),assetId:'a'.repeat(64)},'guest1');
class Channel extends EventTarget{ordered=true;maxRetransmits=null;maxPacketLifeTime=null;readyState='connecting';constructor(label){super();this.label=label;}close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}}
class Connection extends EventTarget{iceGatheringState='complete';channels=[];closed=false;createDataChannel(label){const channel=new Channel(label);this.channels.push(channel);return channel;}async createOffer(){return {type:'offer',sdp:'offer'};}async setLocalDescription(value){this.localDescription=value;}async setRemoteDescription(value){this.remoteDescription=value;}close(){this.closed=true;}}
test('invalid authenticated offer allocates no RTC resources',async()=>{
 let allocations=0;const owner=createReplacementTransport({credential,role:'guest',usedNonces:new Set(),deadline:1000,now:()=>0,createConnection(){allocations++;return new Connection();}});
 const offer=await signRecoveryDescription(credential,{type:'offer',sdp:'original'});
 await assert.rejects(owner.acceptOffer({...offer,description:{type:'offer',sdp:'altered'}}),/authentication/);
 assert.equal(allocations,0);owner.close();
});
test('mismatched signed answer closes owned connection before applying its SDP',async()=>{
 const connection=new Connection(),owner=createReplacementTransport({credential,role:'host',usedNonces:new Set(),deadline:1000,now:()=>0,createConnection:()=>connection});
 const offer=await owner.createOffer(),wrong=await signRecoveryDescription(credential,{type:'answer',sdp:'answer'},{nonce:offer.nonce==='0'.repeat(32)?'1'.repeat(32):'0'.repeat(32)});
 await assert.rejects(owner.acceptAnswer(wrong),/authentication/);assert.equal(connection.remoteDescription,undefined);assert.equal(connection.closed,true);
 assert.ok(connection.channels.every(channel=>channel.readyState==='closed'));
});
test('handoff requires authenticated answer and both reliable channels; abort owns pending readiness',async()=>{
 const controller=new AbortController(),connection=new Connection(),owner=createReplacementTransport({credential,role:'host',usedNonces:new Set(),signal:controller.signal,deadline:1000,now:()=>0,createConnection:()=>connection});
 const offer=await owner.createOffer();assert.throws(()=>owner.handoff(),/Authenticated open/);
 await owner.acceptAnswer(await signRecoveryDescription(credential,{type:'answer',sdp:'answer'},{nonce:offer.nonce}));
 const ready=owner.ready();controller.abort(Error('Session expired'));await assert.rejects(ready,/Session expired/);assert.equal(connection.closed,true);
});
test('successful handoff transfers channel ownership and old owner cannot close it',async()=>{
 const connection=new Connection(),owner=createReplacementTransport({credential,role:'host',usedNonces:new Set(),deadline:1000,now:()=>0,createConnection:()=>connection});
 const offer=await owner.createOffer();await owner.acceptAnswer(await signRecoveryDescription(credential,{type:'answer',sdp:'answer'},{nonce:offer.nonce}));
 for(const channel of connection.channels){channel.readyState='open';channel.dispatchEvent(new Event('open'));}
 assert.equal((await owner.ready()).peer,'guest1');const transferred=owner.handoff();owner.close();assert.equal(connection.closed,false);assert.equal(transferred.channels.size,2);
 assert.throws(()=>owner.handoff(),/handed off/);connection.close();
});
