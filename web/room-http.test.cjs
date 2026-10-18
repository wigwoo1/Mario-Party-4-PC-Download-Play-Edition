const test=require('node:test'),assert=require('node:assert/strict'),http=require('node:http');
const {roomHttpHandler}=require('./room-service.cjs');
async function fixture(t){const handle=roomHttpHandler(),server=http.createServer((req,res)=>{if(!handle(req,res)){res.writeHead(404);res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));t.after(()=>new Promise(r=>{server.close(r);server.closeAllConnections();}));return 'http://127.0.0.1:'+server.address().port;}

test('local close succeeds even when remote room revocation is unavailable',async()=>{
 const {createRoomClient}=await import('./room-client.mjs');let calls=0;
 const client=createRoomClient({baseURL:'https://lobby.example/',fetch:async()=>{if(calls++)throw Error('offline');return new Response(JSON.stringify({version:1,room:'r'.repeat(22),joinKey:'k'.repeat(43),peer:'host',token:'t'.repeat(43)}));}});
 await client.host();await client.close();assert.equal(client.identity,null);
});
test('real HTTP room clients exchange private offers and replies automatically through a join link',async t=>{
 const baseURL=await fixture(t),{createRoomClient,roomJoinLink,readRoomJoinLink}=await import('./room-client.mjs');
 const host=createRoomClient({baseURL}),guest=createRoomClient({baseURL});
 const room=await host.host(),link=roomJoinLink(baseURL+'/',room);
 assert.ok(!link.includes(room.token));assert.equal(new URL(link).search,'');assert.equal(new URL(link).hash.startsWith('#room='),true);
 await guest.join(readRoomJoinLink(link));
 const received=[],errors=[];let resolve;const answered=new Promise(r=>{resolve=r;});
 const watchHost=host.watch(async event=>{received.push(event);if(event.type==='joined')await host.send(event.peer,'offer','private-offer');else if(event.type==='signal'){assert.equal(event.kind,'answer');assert.equal(event.payload,'private-answer');resolve();}}).catch(e=>{if(e.name!=='AbortError')errors.push(e.message);});
 const watchGuest=guest.watch(async event=>{assert.equal(event.kind,'offer');assert.equal(event.from,'host');assert.equal(event.payload,'private-offer');await guest.send('host','answer','private-answer');}).catch(e=>{if(e.name!=='AbortError')errors.push(e.message);});
 let timer;try{await Promise.race([answered,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Automatic exchange timed out')),10000);})]);}finally{clearTimeout(timer);await guest.close();await host.close();await Promise.all([watchHost,watchGuest]);}
 assert.equal(received.filter(e=>e.type==='signal').length,1);assert.deepEqual(errors,[]);
});
test('HTTP API rejects cross-origin writes, non-JSON requests and unauthenticated mailbox reads',async t=>{
 const url=await fixture(t),post=(headers,body)=>fetch(url+'/api/lobbies',{method:'POST',headers,body});
 assert.equal((await post({'Content-Type':'application/json',Origin:'https://unrelated.example'},'{"version":1}')).status,403);
 assert.equal((await post({'Content-Type':'text/plain'},'{"version":1}')).status,415);
 const h=await(await post({'Content-Type':'application/json'},'{"version":1}')).json();
 assert.equal((await fetch(url+'/api/lobbies/'+h.room+'/events')).status,403);
 const headers={Authorization:'Bearer '+h.token};
 assert.equal((await fetch(url+'/api/lobbies/'+h.room+'/events?after=bad',{headers})).status,400);
 assert.equal((await fetch(url+'/private-game.ciso')).status,404);
 await fetch(url+'/api/lobbies/'+h.room,{method:'DELETE',headers});
 assert.equal((await fetch(url+'/api/lobbies/'+h.room+'/events',{headers})).status,410);
});
