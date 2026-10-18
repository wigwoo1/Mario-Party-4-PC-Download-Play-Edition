const hex=(value,length)=>typeof value==='string'&&new RegExp(`^[a-f0-9]{${length}}$`).test(value);
const encode=bytes=>Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('');
const decode=value=>Uint8Array.from(value.match(/../g),b=>parseInt(b,16));
export function receiveRecoveryCredential(message,{session,assetId,peer,current}={}){
 if(message?.type!=='recovery-credential'||message.version!==1||!hex(message.session,32)||!hex(message.assetId,64)||
    !hex(message.secret,64)||/^0+$/.test(message.secret)||message.session!==session||message.assetId!==assetId||message.peer!==peer||
    typeof peer!=='string'||!/^guest[1-9][0-9]{0,8}$/.test(peer))throw Error('Invalid recovery credential.');
 const next=Object.freeze({type:message.type,version:1,session,assetId,peer,secret:message.secret});
 if(current&&JSON.stringify(next)!==JSON.stringify(current))throw Error('Recovery credential changed.');
 return current||next;
}
export function createRecoveryCredential({session,assetId},peer,random=crypto){
 return receiveRecoveryCredential({type:'recovery-credential',version:1,session,assetId,peer,
  secret:encode(random.getRandomValues(new Uint8Array(32)))},{session,assetId,peer});
}
function canonicalDescription(message,credential,type){
 receiveRecoveryCredential(credential,credential);
 if(message?.type!=='recovery-description'||message.version!==1||message.session!==credential.session||
    message.assetId!==credential.assetId||message.peer!==credential.peer||!hex(message.nonce,32)||
    !['offer','answer'].includes(message.description?.type)||(type!==undefined&&message.description.type!==type)||
    typeof message.description.sdp!=='string'||message.description.sdp.length<1||message.description.sdp.length>100000)
  throw Error('Invalid authenticated recovery description.');
 return Object.freeze({type:message.type,version:1,session:message.session,assetId:message.assetId,peer:message.peer,
  nonce:message.nonce,description:Object.freeze({type:message.description.type,sdp:message.description.sdp})});
}
async function key(credential,subtle){return subtle.importKey('raw',decode(credential.secret),{name:'HMAC',hash:'SHA-256'},false,['sign','verify']);}
export async function signRecoveryDescription(credential,description,{nonce,cryptoProvider=crypto}={}){
 const packet=canonicalDescription({type:'recovery-description',version:1,session:credential.session,assetId:credential.assetId,
  peer:credential.peer,nonce:nonce??encode(cryptoProvider.getRandomValues(new Uint8Array(16))),description},credential);
 const signature=await cryptoProvider.subtle.sign('HMAC',await key(credential,cryptoProvider.subtle),new TextEncoder().encode(JSON.stringify(packet)));
 return Object.freeze({...packet,signature:encode(new Uint8Array(signature))});
}
export async function verifyRecoveryDescription(message,credential,{type,usedNonces,expectedNonce,cryptoProvider=crypto}={}){
 if(!(usedNonces instanceof Set)||!['offer','answer'].includes(type))throw Error('Owned replay set and expected description type required.');
 const packet=canonicalDescription(message,credential,type),nonce=type+':'+packet.nonce;
 if(!hex(message.signature,64)||usedNonces.has(nonce)||(type==='answer'&&packet.nonce!==expectedNonce))return null;
 const valid=await cryptoProvider.subtle.verify('HMAC',await key(credential,cryptoProvider.subtle),decode(message.signature),new TextEncoder().encode(JSON.stringify(packet)));
 // Concurrent verification of the same valid packet can authorize only once.
 if(!valid||usedNonces.has(nonce))return null;
 usedNonces.add(nonce);return packet;
}
