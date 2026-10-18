import {createRoomClient,roomJoinLink,readRoomJoinLink} from './room-client.mjs';

// Serialize offer creation, without blocking the mailbox that delivers answers.
// Broker identities and transport identities are deliberately kept separate.
export function automaticRoom({baseURL,pageURL=baseURL,client=createRoomClient({baseURL}),
 createOffer,answerOffer,applyAnswer,cancelOffer=()=>{},left=()=>{},changed=()=>{},error=()=>{},timeoutMs=45000,
 createRecoveryOffer,answerRecoveryOffer,applyRecoveryAnswer,recoveryAnswerSent=()=>{}}){
 let closed=false,role,pending,pumping=false,watch;const waiting=[],bindings=new Map(),recovering=new Set();
 const notifyFailure=cause=>{if(!closed)error(cause);};
 async function pump(){
  if(closed||pumping||pending||!waiting.length)return;pumping=true;
  const peer=waiting.shift();
  try{
   pending={peer};const payload=await createOffer();
   if(closed||pending?.peer!==peer)return;
   const current=pending;current.payload=payload;
   await client.send(peer,'offer',payload);
   if(pending===current)current.timer=setTimeout(()=>{void rejectPeer(peer,Error('A guest did not finish connecting. They can open the lobby link again.'));},timeoutMs);
  }catch(cause){await rejectPeer(peer,cause);}finally{pumping=false;if(!pending)void pump();}
 }
 async function rejectPeer(peer,cause){
  if(pending?.peer===peer){clearTimeout(pending.timer);pending=undefined;await cancelOffer();}
  bindings.delete(peer);try{await client.removeGuest(peer);}catch(e){if(!closed)notifyFailure(e);}
  notifyFailure(cause);if(!pumping)void pump();
 }
 async function event(e){
  if(closed)return;
  if(e.type==='signal'&&e.kind==='recovery-offer'&&role==='guest'&&e.from==='host'){
   if(recovering.has('host')||!answerRecoveryOffer)throw Error('Unexpected reconnect invitation.');recovering.add('host');
   try{const reply=await answerRecoveryOffer(e.payload);if(closed)return;await client.send('host','recovery-answer',reply);void Promise.resolve(recoveryAnswerSent()).catch(notifyFailure).finally(()=>recovering.delete('host'));}
   catch(cause){recovering.delete('host');throw cause;}return;
  }
  if(e.type==='signal'&&e.kind==='recovery-answer'&&role==='host'){
   if(!recovering.has(e.from)||!applyRecoveryAnswer)throw Error('Unexpected reconnect reply.');
   try{await applyRecoveryAnswer(bindings.get(e.from),e.payload);changed({role,recovered:bindings.get(e.from)});}finally{recovering.delete(e.from);}return;
  }
  if(role==='host'){
   if(e.type==='joined'){waiting.push(e.peer);void pump();return;}
   if(e.type==='left'){
    const index=waiting.indexOf(e.peer);if(index>=0)waiting.splice(index,1);
    if(pending?.peer===e.peer){clearTimeout(pending.timer);pending=undefined;await cancelOffer();}
    const transport=bindings.get(e.peer);bindings.delete(e.peer);if(transport)await left(transport);void pump();return;
   }
   if(e.type!=='signal'||e.kind!=='answer'||pending?.peer!==e.from||!pending.payload)throw Error('Unexpected lobby connection reply.');
   const current=pending;clearTimeout(current.timer);
   const transport=await applyAnswer(e.payload);if(closed)return;
   bindings.set(e.from,transport);pending=undefined;changed({role,connected:bindings.size});void pump();
  }else{
   if(e.type!=='signal'||e.kind!=='offer'||e.from!=='host'||pending)throw Error('Unexpected host connection invitation.');
   pending={peer:'host'};const reply=await answerOffer(e.payload);if(closed)return;
   await client.send('host','answer',reply);changed({role,connected:1});
  }
 }
 function observe(){watch=client.watch(event).catch(cause=>{if(!closed){closed=true;clearTimeout(pending?.timer);void client.close().catch(()=>{});error(cause);}});}
 return {
  async remove(transportPeer){const peer=[...bindings].find(([,transport])=>transport===transportPeer)?.[0];if(!peer||closed)return;bindings.delete(peer);await client.removeGuest(peer);},
  async recover(transportPeer){
   const peer=[...bindings].find(([,transport])=>transport===transportPeer)?.[0];
   if(closed||role!=='host'||!peer||recovering.has(peer)||!createRecoveryOffer)throw Error('Assigned reconnecting guest required.');
   recovering.add(peer);try{const payload=await createRecoveryOffer(transportPeer);if(!closed)await client.send(peer,'recovery-offer',payload);}catch(cause){recovering.delete(peer);throw cause;}
  },
  async seal(){if(role!=='host'||closed||pending||waiting.length)throw Error('Finish connecting players before starting.');await client.seal();},
  async host(){if(role||closed)throw Error('Lobby already opened.');role='host';const identity=await client.host();if(closed){await client.close();return;}observe();const link=roomJoinLink(pageURL,identity);changed({role,link,connected:0});return link;},
  async join(link){if(role||closed)throw Error('Lobby already opened.');const url=new URL(link,pageURL);if(url.origin!==new URL(baseURL).origin)throw Error('Open the join link on the host’s lobby website.');const invitation=readRoomJoinLink(url.href);if(!invitation)throw Error('Paste a complete lobby join link.');role='guest';await client.join(invitation);if(closed){await client.close();return;}observe();changed({role,connected:0});},
  async close(){if(closed)return;closed=true;clearTimeout(pending?.timer);waiting.length=0;await client.close();},
 };
}
