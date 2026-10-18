'use strict';
const {randomBytes,createHash}=require('node:crypto');
class RoomError extends Error{constructor(status,message){super(message);this.status=status;}}
const fail=(status,message)=>{throw new RoomError(status,message);};
const token=()=>randomBytes(32).toString('base64url');
const keys=(value,allowed)=>value&&typeof value==='object'&&!Array.isArray(value)&&Object.keys(value).every(key=>allowed.includes(key));
// Signaling only: authenticated, bounded in-memory mailboxes. Game assets,
// inputs and renderer state continue over the existing peer connections.
function createRoomService({now=Date.now,maxRooms=32,idleMs=120000,lifetimeMs=12*60*60*1000,maxEvents=256,maxMailboxBytes=1024*1024}={}){
 const rooms=new Map();
 const expired=r=>now()-r.hostSeen>=idleMs||now()-r.created>=lifetimeMs;
 const prune=()=>{for(const [id,r] of rooms)if(expired(r))rooms.delete(id);};
 function room(id){prune();const r=rooms.get(id);if(!r)fail(410,'Lobby expired or closed.');return r;}
 function member(r,bearer){const p=[...r.peers.values()].find(p=>p.token===bearer);if(!p)fail(403,'Lobby access denied.');if(p.id==='host')r.hostSeen=now();return p;}
 function emit(r,p,message){
  const event={sequence:++r.sequence,...message},bytes=Buffer.byteLength(JSON.stringify(event));
  p.events.push({event,bytes});p.bytes+=bytes;
  while(p.events.length>maxEvents||p.bytes>maxMailboxBytes){const old=p.events.shift();p.bytes-=old.bytes;p.floor=old.event.sequence;}
 }
 const peer=id=>({id,token:token(),events:[],bytes:0,floor:0,sent:new Map()});
 return {
  create(body){
   if(!keys(body,['version','id'])||body.version!==1||(body.id!==undefined&&(typeof body.id!=='string'||!/^[a-f0-9]{32}$/.test(body.id))))fail(400,'Unsupported lobby request.');
   prune();if(body.id)for(const r of rooms.values())if(r.registrationId===body.id){const h=r.peers.get('host');return {version:1,room:r.id,joinKey:r.key,peer:'host',token:h.token};}
   if(rooms.size>=maxRooms)fail(503,'Connection service is busy. Try again shortly.');
   const id=randomBytes(16).toString('base64url'),host=peer('host'),r={id,registrationId:body.id,key:token(),created:now(),hostSeen:now(),sequence:0,joins:0,peers:new Map([['host',host]])};rooms.set(id,r);
   return {version:1,room:id,joinKey:r.key,peer:'host',token:host.token};
  },
  join(id,body){
   if(!keys(body,['version','joinKey','id'])||body.version!==1||typeof body.joinKey!=='string'||(body.id!==undefined&&(typeof body.id!=='string'||!/^[a-f0-9]{32}$/.test(body.id))))fail(400,'Invalid join request.');
   const r=room(id);if(body.joinKey!==r.key)fail(403,'Invalid join link.');
   if(body.id)for(const p of r.peers.values())if(p.registrationId===body.id)return {version:1,room:id,peer:p.id,token:p.token};
   if(r.sealed)fail(409,'This game has already started.');
   if(r.peers.size>=4)fail(409,'This lobby already has four players.');
   if(r.joins>=256)fail(409,'Create a fresh lobby to invite more players.');
   const p=peer('guest'+(++r.joins));p.registrationId=body.id;r.peers.set(p.id,p);emit(r,r.peers.get('host'),{type:'joined',peer:p.id});
   return {version:1,room:id,peer:p.id,token:p.token};
  },
  poll(id,bearer,after){
   const r=room(id),p=member(r,bearer);
   if(!Number.isSafeInteger(after)||after<0||after>r.sequence)fail(400,'Invalid lobby event cursor.');
   if(after<p.floor)fail(409,'Lobby event history expired. Rejoin the lobby.');
   const events=p.events.filter(row=>row.event.sequence>after).slice(0,64).map(row=>row.event);
   return {version:1,events,cursor:events.at(-1)?.sequence??after};
  },
  send(id,bearer,body){
   const r=room(id),sender=member(r,bearer);
   if(!keys(body,['version','to','kind','payload','id'])||body.version!==1||typeof body.to!=='string'||typeof body.payload!=='string'||Buffer.byteLength(body.payload)>100000||
      (body.id!==undefined&&(typeof body.id!=='string'||!/^[A-Za-z0-9_-]{16,64}$/.test(body.id))))fail(400,'Invalid connection message.');
   const recipient=r.peers.get(body.to);if(!recipient||recipient===sender)fail(400,'Unavailable lobby player.');
   const allowed=sender.id==='host'?['offer','recovery-offer']:['answer','recovery-answer'];
   if(!allowed.includes(body.kind)||(sender.id!=='host'&&recipient.id!=='host'))fail(403,'Connection message is not allowed for this player.');
   const digest=createHash('sha256').update(JSON.stringify([body.to,body.kind,body.payload])).digest('hex');
   if(body.id&&sender.sent.has(body.id)){if(sender.sent.get(body.id)!==digest)fail(409,'Connection message identifier was reused.');return {version:1,sent:true};}
   emit(r,recipient,{type:'signal',from:sender.id,kind:body.kind,payload:body.payload});
   if(body.id){sender.sent.set(body.id,digest);if(sender.sent.size>256)sender.sent.delete(sender.sent.keys().next().value);}
   return {version:1,sent:true};
  },
  removeGuest(id,bearer,peer){const r=room(id),p=member(r,bearer);if(p.id!=='host')fail(403,'Only the host can remove a player.');if(!/^guest[1-9][0-9]{0,8}$/.test(peer))fail(400,'Guest player required.');r.peers.delete(peer);return {version:1,closed:true};},
  seal(id,bearer){const r=room(id),p=member(r,bearer);if(p.id!=='host')fail(403,'Only the host can start a game.');r.sealed=true;return {version:1,sealed:true};},
  leave(id,bearer){const r=room(id),p=member(r,bearer);if(p.id==='host')rooms.delete(id);else{r.peers.delete(p.id);emit(r,r.peers.get('host'),{type:'left',peer:p.id});}return {version:1,closed:true};},
 };
}
function roomHttpHandler({service=createRoomService(),now=Date.now,publicOrigin}={}){
 const creationRate=new Map();
 const reply=(res,status,value)=>{res.writeHead(status,{'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store','X-Content-Type-Options':'nosniff'});res.end(JSON.stringify(value));};
 return function handle(req,res){
  let url;try{url=new URL(req.url,'http://localhost');}catch{return false;}
  if(!url.pathname.startsWith('/api/lobbies'))return false;
  (async()=>{
   const expected=publicOrigin??`${req.socket.encrypted?'https':'http'}://${req.headers.host}`;
   if(req.headers.origin&&req.headers.origin!==expected)fail(403,'Use the lobby page on this connection service.');
   const match=/^\/api\/lobbies(?:\/([A-Za-z0-9_-]{22})(?:\/(join|events|signals|seal|players\/guest[1-9][0-9]{0,8}))?)?$/.exec(url.pathname);
   if(!match)fail(404,'Lobby endpoint not found.');
   const [,id,action]=match;
   if(!id&&req.method==='POST'){
    const ip=req.socket.remoteAddress??'unknown',previous=creationRate.get(ip),bucket=previous&&now()-previous.start<60000?previous:{start:now(),count:0};
    if(++bucket.count>10)fail(429,'Too many new lobbies. Try again shortly.');
    creationRate.set(ip,bucket);if(creationRate.size>256)creationRate.delete(creationRate.keys().next().value);
   }
   let body;
   if(req.method==='POST'){
    if(req.headers['content-type']?.split(';')[0]!=='application/json')fail(415,'JSON request required.');
    if(Number(req.headers['content-length'])>110000)fail(413,'Connection message is too large.');
    const chunks=[];let size=0;
    for await(const chunk of req){size+=chunk.length;if(size>110000)fail(413,'Connection message is too large.');chunks.push(chunk);}
    try{body=JSON.parse(Buffer.concat(chunks).toString('utf8'));}catch{fail(400,'Invalid JSON request.');}
   }
   const bearer=/^Bearer ([A-Za-z0-9_-]{43})$/.exec(req.headers.authorization??'')?.[1];
   let result;
   if(!id&&req.method==='POST')result=service.create(body);
   else if(action==='join'&&req.method==='POST')result=service.join(id,body);
   else if(action==='events'&&req.method==='GET')result=service.poll(id,bearer,Number(url.searchParams.get('after')??0));
   else if(action==='signals'&&req.method==='POST')result=service.send(id,bearer,body);
   else if(action==='seal'&&req.method==='POST')result=service.seal(id,bearer);
   else if(action?.startsWith('players/')&&req.method==='DELETE')result=service.removeGuest(id,bearer,action.slice(8));
   else if(id&&!action&&req.method==='DELETE')result=service.leave(id,bearer);
   else fail(405,'Lobby operation not allowed.');
   reply(res,200,result);
  })().catch(error=>reply(res,error instanceof RoomError?error.status:500,{error:error instanceof RoomError?error.message:'Connection service failed.'}));
  return true;
 };
}
module.exports={createRoomService,roomHttpHandler,RoomError};
