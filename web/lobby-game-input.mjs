import {SessionInput} from './session-input.mjs';
import {createSessionInputExchange} from './session-input-exchange.mjs';
import {installBrowserInputExchange} from './browser-input-exchange.mjs';
// Installs controls while Idle. Only the separate start agreement may resume.
export function prepareLobbyGameInput(runtime,{plan,session,bind,now=()=>performance.now()}){
 if(runtime?.engine?.phase!=='Idle'||typeof bind!=='function'||runtime.module?._PartyBoard_GameFrameCount()!==0)
  throw Error('Idle frame-zero runtime required for lobby input.');
 const queue=new SessionInput(session,plan.owners),observed=[];let bridge,router,binding,rejectedPoll=null,pollCheckpointComponents=null;
 try{
  router=bind((owner,bytes)=>{
   const result=queue.receive(owner,bytes,now());
   if(!['Accepted','Duplicate','Stale'].includes(result))throw Error('Peer input rejected: '+result);
   bridge?.notify();
  },owners=>{for(const owner of owners)queue.disconnected(owner,now());bridge?.notify();});
  bridge=createSessionInputExchange(queue,{session,owners:plan.owners,peer:plan.owner,
   localPadSlots:plan.localPadSlots,suspendOnPause:true,now,
   checkpoint:metadata=>{if(!Number.isInteger(metadata.checkpoint))throw Error('Actual game checkpoint required.');return metadata.checkpoint;},
   send:bytes=>router.send(bytes)});
  runtime.module.partyBoardGlyphLocalPadSlots=Object.freeze([...plan.localPadSlots]);
  binding=installBrowserInputExchange(runtime.module,async(pads,metadata)=>{
   const frame=queue.status(now()).frame.toString();
   if(runtime.module._PartyBoard_GameCheckpointComponent)pollCheckpointComponents={frame,
    components:[0,1,2,3].map(field=>runtime.module._PartyBoard_GameCheckpointComponent(field))};
   const result=await bridge.exchange(pads,metadata);
   if(result){observed.push({frame,checkpoint:metadata.checkpoint,frand:metadata.frand,rand8:metadata.rand8,pads:Array.from(result)});if(observed.length>256)observed.shift();}
   else rejectedPoll={frame,checkpoint:metadata.checkpoint,frand:metadata.frand,rand8:metadata.rand8};
   return result;
  });
 }catch(error){delete runtime.module.partyBoardGlyphLocalPadSlots;router?.close();queue.cancel();throw error;}
 let closed=false;
 return {queue,bridge,installed:true,pollCheckpointComponents(){return structuredClone(pollCheckpointComponents);},rejectedPoll(){return rejectedPoll&&{...rejectedPoll};},timing(){return binding.timing();},observations(){return structuredClone(observed);},cancel(){queue.cancel();binding.cancel();bridge.notify();},
  close(){if(closed)return;closed=true;queue.cancel();binding.close();bridge.notify();router.close();delete runtime.module.partyBoardGlyphLocalPadSlots;}};
}
