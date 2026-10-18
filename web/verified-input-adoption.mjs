import {consumePausedStateAgreement} from './paused-state-agreement.mjs';
// Adopt transport while retaining the paused queue. Agreement is necessary
// for adoption; a separate all-player recovery commit still authorizes resume.
export function adoptVerifiedInput({receipt,identity,peer,remotePeer,nonce,input,control,router,capture,current}){
 if(typeof current!=='function'||current()!==true||typeof capture!=='function'||typeof router?.replace!=='function'||
    input?.label!=='partyboard-input-v1'||input.readyState!=='open'||input.ordered!==true||input.maxRetransmits!=null||input.maxPacketLifeTime!=null||
    control?.label!=='partyboard-recovery-v1'||control.readyState!=='open')throw Error('Current paused attempt and reliable replacement input required.');
 if(!consumePausedStateAgreement(receipt,{channel:control,identity,captured:capture(),peer,remotePeer,nonce}))
  throw Error('Current owned paused-state agreement required.');
 const previous=router.replace(remotePeer,input);
 return Object.freeze({peer:remotePeer,previous,input,control,nonce});
}
