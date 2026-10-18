const test=require('node:test'),assert=require('node:assert/strict');
const {createRoomService}=require('./room-service.cjs');
const create=s=>s.create({version:1}),join=(s,h)=>s.join(h.room,{version:1,joinKey:h.joinKey});

test('secret registration identifiers make lost create/join responses retryable without duplicate rooms or slots',()=>{
 const s=createRoomService({maxRooms:1}),h=s.create({version:1,id:'a'.repeat(32)});
 assert.deepEqual(s.create({version:1,id:'a'.repeat(32)}),h);
 assert.throws(()=>s.create({version:1,id:'b'.repeat(32)}),/busy/);
 const body={version:1,joinKey:h.joinKey,id:'c'.repeat(32)},g=s.join(h.room,body);
 assert.deepEqual(s.join(h.room,body),g);assert.equal(s.poll(h.room,h.token,0).events.length,1);
 assert.throws(()=>s.join(h.room,{...body,joinKey:'wrong'}),/Invalid join link/);
 s.seal(h.room,h.token);assert.deepEqual(s.join(h.room,body),g);
 assert.throws(()=>s.join(h.room,{...body,id:'d'.repeat(32)}),/already started/);
 for(const id of ['',123,'x'.repeat(32),'a'.repeat(31)])assert.throws(()=>s.create({version:1,id}),/Unsupported/);
});

test('only host may remove guests or seal a game; sealing prevents new joins but preserves reconnect signaling',()=>{
 const s=createRoomService(),h=create(s),g=join(s,h),g2=join(s,h);
 assert.throws(()=>s.removeGuest(h.room,g.token,g2.peer),/Only the host/);
 assert.throws(()=>s.seal(h.room,g.token),/Only the host/);
 s.removeGuest(h.room,h.token,g2.peer);assert.throws(()=>s.poll(h.room,g2.token,0),/access denied/);
 s.seal(h.room,h.token);assert.throws(()=>join(s,h),/already started/);
 s.send(h.room,h.token,{version:1,to:g.peer,kind:'recovery-offer',payload:'signed reconnect fixture'});
 assert.equal(s.poll(h.room,g.token,0).events[0].kind,'recovery-offer');
});
test('four-player room relays offers/answers with server-owned identities and replayable cursors',()=>{
 const s=createRoomService(),h=create(s),guests=[join(s,h),join(s,h),join(s,h)];
 assert.equal(h.token.length,43);assert.notEqual(h.token,h.joinKey);
 assert.throws(()=>join(s,h),/four players/);
 const hostEvents=s.poll(h.room,h.token,0);assert.deepEqual(hostEvents.events.map(e=>e.peer),guests.map(g=>g.peer));
 const g=guests[0];s.send(h.room,h.token,{version:1,to:g.peer,kind:'offer',payload:'private-offer'});
 const offer=s.poll(h.room,g.token,0);assert.equal(offer.events[0].from,'host');assert.equal(offer.events[0].payload,'private-offer');
 assert.deepEqual(s.poll(h.room,g.token,0),offer);assert.deepEqual(s.poll(h.room,g.token,offer.cursor).events,[]);
 s.send(h.room,g.token,{version:1,to:'host',kind:'answer',payload:'private-answer'});
 assert.equal(s.poll(h.room,h.token,hostEvents.cursor).events[0].from,g.peer);
 assert.equal(s.poll(h.room,guests[1].token,0).events.length,0);
});
test('room credentials and direction checks reject cross-room access, impersonation and guest forwarding',()=>{
 const s=createRoomService(),h=create(s),other=create(s),g=join(s,h),g2=join(s,h);
 for(const invalid of [undefined,h.joinKey,other.token])assert.throws(()=>s.poll(h.room,invalid,0),/access denied/);
 assert.throws(()=>s.join(h.room,{version:1,joinKey:'bad'}),/Invalid join link/);
 for(const body of [{to:g2.peer,kind:'answer'},{to:'host',kind:'offer'}])assert.throws(()=>s.send(h.room,g.token,{version:1,payload:'x',...body}),/not allowed/);
 assert.throws(()=>s.send(h.room,g.token,{version:1,to:'host',kind:'answer',payload:'x',from:'host'}),/Invalid connection/);
 assert.throws(()=>s.send(h.room,h.token,{version:1,to:g.peer,kind:'offer',payload:'x'.repeat(100001)}),/Invalid connection/);
});
test('leaving removes access, frees a slot and host closure revokes the room',()=>{
 const s=createRoomService(),h=create(s),g=join(s,h);s.leave(h.room,g.token);
 assert.throws(()=>s.poll(h.room,g.token,0),/access denied/);assert.equal(join(s,h).peer,'guest2');
 assert.equal(s.poll(h.room,h.token,0).events[1].type,'left');s.leave(h.room,h.token);
 assert.throws(()=>join(s,h),/expired or closed/);
});
test('retried connection messages deliver once and cannot reuse an identifier for different data',()=>{
 const s=createRoomService(),h=create(s),g=join(s,h),message={version:1,to:g.peer,kind:'offer',payload:'opaque-sdp',id:'x'.repeat(32)};
 s.send(h.room,h.token,message);s.send(h.room,h.token,message);
 assert.equal(s.poll(h.room,g.token,0).events.length,1);
 assert.throws(()=>s.send(h.room,h.token,{...message,payload:'changed'}),/identifier was reused/);
});
test('only the host keeps a room alive, with bounded capacity, lifetime and mailbox history',()=>{
 let time=0;const s=createRoomService({now:()=>time,maxRooms:1,idleMs:120,maxEvents:2,lifetimeMs:1000}),h=create(s),g=join(s,h);
 assert.throws(()=>create(s),/busy/);
 for(let i=0;i<3;i++)s.send(h.room,h.token,{version:1,to:g.peer,kind:'recovery-offer',payload:String(i)});
 assert.throws(()=>s.poll(h.room,g.token,0),/history expired/);
 assert.equal(s.poll(h.room,g.token,2).events.length,2);
 time=100;s.poll(h.room,g.token,2);time=121;assert.throws(()=>s.poll(h.room,h.token,0),/expired/);
 assert.ok(create(s));
});
