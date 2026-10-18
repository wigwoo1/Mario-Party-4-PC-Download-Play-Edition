import test from 'node:test';import assert from 'node:assert/strict';
import {createLobbyInputRouter} from './lobby-input-router.mjs';import {lobbyInputOwnership} from './lobby-input-ownership.mjs';import {encodeInput} from './input-packet.mjs';
class Channel extends EventTarget{readyState='open';sent=[];send(bytes){this.sent.push(bytes);}message(bytes){const event=new Event('message');event.data=bytes.buffer;this.dispatchEvent(event);}}
test('open input channel send exception retains diagnostic cause and exact lost owner',()=>{
 const snapshot={membership:1,players:[{peer:'host',slot:0},{peer:'g1',slot:1}]},session=new Uint8Array(16).fill(1),channel=new Channel(),losses=[];
 channel.bufferedAmount=4096;channel.send=()=>{throw new DOMException('Send buffer unavailable','OperationError');};
 const router=createLobbyInputRouter({plan:lobbyInputOwnership(snapshot,'host'),session,host:true,channels:new Map([['g1',channel]]),receive:assert.fail,
  disconnected:(owners,diagnostic)=>losses.push({owners,diagnostic})});
 router.send(encodeInput({session,frame:0n,checkpoint:42,slot:0,pad:new Uint8Array(11)}));
 assert.deepEqual(losses,[{owners:[2],diagnostic:{peer:'g1',reason:'send-failed',readyState:'open',bufferedAmount:4096,
  error:{name:'OperationError',message:'Send buffer unavailable'}}}]);
 assert.equal(channel.readyState,'open');router.close();
});
test('host binds guest lanes before forwarding and prevents spoofed/session-crossing input',()=>{
 const plan=lobbyInputOwnership({membership:1,players:[{peer:'host',slot:0},{peer:'g1',slot:1},{peer:'g2',slot:2}]},'host');const session=new Uint8Array(16).fill(1),a=new Channel(),b=new Channel(),received=[],errors=[];
 const router=createLobbyInputRouter({plan,session,host:true,channels:new Map([['g1',a],['g2',b]]),receive:(owner,bytes)=>received.push(owner),error:(_,e)=>errors.push(e.message)});
 const packet=slot=>encodeInput({session,frame:0n,checkpoint:1,slot,pad:new Uint8Array(11)});
 a.message(packet(1));assert.deepEqual(received,[2]);assert.equal(b.sent.length,1);assert.equal(a.sent.length,0);
 a.message(packet(2));const wrong=packet(1);wrong[8]=2;a.message(wrong);assert.equal(errors.length,2);assert.equal(b.sent.length,1);
 router.send(packet(0));assert.equal(a.sent.length,1);assert.equal(b.sent.length,2);
 router.close();a.message(packet(1));assert.deepEqual(received,[2]);assert.throws(()=>router.send(packet(0)),/closed/);
});
test('channel loss reports exact lockstep owners and deliberate router close removes loss listeners',()=>{
 const snapshot={membership:1,players:['host','g1','g2','g3'].map((peer,slot)=>({peer,slot}))},session=new Uint8Array(16).fill(1);
 const a=new Channel(),b=new Channel(),c=new Channel(),hostLost=[],guestLost=[];
 const host=createLobbyInputRouter({plan:lobbyInputOwnership(snapshot,'host'),session,host:true,channels:new Map([['g1',a],['g2',b],['g3',c]]),receive(){},disconnected:owners=>hostLost.push(owners)});
 b.dispatchEvent(new Event('close'));assert.deepEqual(hostLost,[[3]]);
 const upstream=new Channel(),guest=createLobbyInputRouter({plan:lobbyInputOwnership(snapshot,'g2'),session,channels:new Map([['host',upstream]]),receive(){},disconnected:owners=>guestLost.push(owners)});
 upstream.dispatchEvent(new Event('close'));assert.deepEqual(guestLost,[[1,2,4]]);
 host.close();guest.close();a.dispatchEvent(new Event('close'));upstream.dispatchEvent(new Event('close'));
 assert.deepEqual(hostLost,[[3]]);assert.deepEqual(guestLost,[[1,2,4]]);
});
test('unavailable route pauses ownership before accepting another guest packet and never advances on send failure',()=>{
 const snapshot={membership:1,players:[{peer:'host',slot:0},{peer:'g1',slot:1},{peer:'g2',slot:2}]},session=new Uint8Array(16).fill(1);
 const a=new Channel(),b=new Channel(),events=[],errors=[];
 b.readyState='closed';
 const router=createLobbyInputRouter({plan:lobbyInputOwnership(snapshot,'host'),session,host:true,channels:new Map([['g1',a],['g2',b]]),
  disconnected:owners=>events.push(['lost',...owners]),receive:owner=>events.push(['received',owner]),error:(_,cause)=>errors.push(cause)});
 a.message(encodeInput({session,frame:0n,checkpoint:42,slot:1,pad:new Uint8Array(11)}));
 assert.deepEqual(events,[['lost',3],['received',2]]);
 router.send(encodeInput({session,frame:0n,checkpoint:42,slot:0,pad:new Uint8Array(11)}));
 assert.deepEqual(events,[['lost',3],['received',2]]);assert.equal(errors.length,0);assert.equal(b.sent.length,0);
 a.send=()=>{throw Error('Transport closed during send');};router.send(encodeInput({session,frame:0n,checkpoint:42,slot:0,pad:new Uint8Array(11)}));
 assert.deepEqual(events.at(-1),['lost',2]);assert.equal(errors.length,0);router.close();
});
test('replacement retains assigned owner and detaches stale messages and loss without resuming a queue',()=>{
 const snapshot={membership:7,players:[{peer:'host',slot:0},{peer:'g1',slot:1},{peer:'g2',slot:2}]},session=new Uint8Array(16).fill(1),old=new Channel(),other=new Channel(),replacement=new Channel(),received=[],lost=[];
 replacement.ordered=true;replacement.maxRetransmits=null;replacement.maxPacketLifeTime=null;
 const router=createLobbyInputRouter({plan:lobbyInputOwnership(snapshot,'host'),session,host:true,channels:new Map([['g1',old],['g2',other]]),receive:owner=>received.push(owner),disconnected:owners=>lost.push(owners)});
 const packet=encodeInput({session,frame:0n,checkpoint:42,slot:1,pad:new Uint8Array(11)});
 assert.throws(()=>router.replace('unknown',replacement),/Assigned/);
 assert.throws(()=>router.replace('g1',{readyState:'open',ordered:false}),/reliable/);
 assert.equal(router.replace('g1',replacement),old);
 old.message(packet);old.dispatchEvent(new Event('close'));assert.deepEqual(received,[]);assert.deepEqual(lost,[]);
 replacement.message(packet);assert.deepEqual(received,[2]);assert.equal(other.sent.length,1);
 assert.throws(()=>router.replace('g2',replacement),/already assigned/);
 router.close();replacement.message(packet);replacement.dispatchEvent(new Event('close'));assert.deepEqual(received,[2]);assert.deepEqual(lost,[]);
});
