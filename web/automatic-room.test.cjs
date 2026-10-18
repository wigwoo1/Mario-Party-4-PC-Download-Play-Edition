const test=require('node:test'),assert=require('node:assert/strict'),http=require('node:http');
const {roomHttpHandler}=require('./room-service.cjs');
async function fixture(t){const handle=roomHttpHandler(),server=http.createServer((q,s)=>handle(q,s));await new Promise(r=>server.listen(0,'127.0.0.1',r));t.roomServer=server;return 'http://127.0.0.1:'+server.address().port+'/';}
async function until(check){const deadline=Date.now()+10000;while(!check()){if(Date.now()>deadline)throw Error('Lobby exchange timed out');await new Promise(r=>setTimeout(r,20));}}
test('three simultaneous link joins serialize offers and automatically apply all replies without blocking polling',async t=>{
 const baseURL=await fixture(t),{automaticRoom}=await import('./automatic-room.mjs');const owners=[],errors=[],accepted=[];let offers=0,active=false,recovered=false,replySent=false;
 t.after(async()=>{await Promise.all(owners.reverse().map(o=>o.close()));await new Promise(r=>{t.roomServer.close(r);t.roomServer.closeAllConnections();});});
 const host=automaticRoom({baseURL,createOffer:async()=>{assert.equal(active,false);active=true;return 'offer'+(++offers);},applyAnswer:async payload=>{assert.equal(payload,'answer'+offers);active=false;accepted.push(payload);return 'transport'+offers;},createRecoveryOffer:async peer=>{assert.equal(peer,'transport2');return 'signed-fixture-offer';},applyRecoveryAnswer:async(peer,reply)=>{assert.equal(peer,'transport2');assert.equal(reply,'signed-fixture-answer');recovered=true;},error:e=>errors.push(e.message)});owners.push(host);const link=await host.host();
 const guests=Array.from({length:3},()=>automaticRoom({baseURL,answerOffer:async payload=>'answer'+payload.slice(5),answerRecoveryOffer:async payload=>{assert.equal(payload,'signed-fixture-offer');return 'signed-fixture-answer';},recoveryAnswerSent:()=>{replySent=true;},error:e=>errors.push(e.message)}));owners.push(...guests);
 await Promise.all(guests.map(o=>o.join(link)));await until(()=>accepted.length===3);assert.equal(offers,3);
 await host.seal();await host.recover('transport2');await until(()=>recovered&&replySent);assert.deepEqual(errors,[]);
});
test('unanswered guest times out, frees its room slot and lets the next queued guest connect',async t=>{
 const baseURL=await fixture(t),{automaticRoom}=await import('./automatic-room.mjs'),{createRoomClient,readRoomJoinLink}=await import('./room-client.mjs');let offers=0,cancelled=0,accepted=false;const errors=[];
 const host=automaticRoom({baseURL,timeoutMs:3000,createOffer:async()=>''+(++offers),cancelOffer:()=>cancelled++,applyAnswer:async()=>{accepted=true;return 'transport2';},error:e=>errors.push(e.message)});
 const link=await host.host(),silent=createRoomClient({baseURL});await silent.join(readRoomJoinLink(link));
 const guest=automaticRoom({baseURL,answerOffer:async()=> 'answer',error:e=>errors.push(e.message)});await guest.join(link);
 t.after(async()=>{await guest.close();await silent.close().catch(()=>{});await host.close();await new Promise(r=>{t.roomServer.close(r);t.roomServer.closeAllConnections();});});
 await until(()=>accepted);assert.equal(cancelled,1);assert.equal(offers,2);assert.equal(errors.length,1);assert.match(errors[0],/did not finish/);
});
