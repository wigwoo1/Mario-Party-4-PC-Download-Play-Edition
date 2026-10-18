import {prepare, guest, digest} from './transfer.mjs';
const hex = bytes => Array.from(bytes, n=>n.toString(16).padStart(2,'0')).join('');
const hash = value => typeof value==='string' && /^[a-f0-9]{64}$/.test(value);
const validSize = size => Number.isSafeInteger(size) && size>=8192 && size<=32*1024*1024 && size%8192===0;
function context(assetId,sessionId) {
  if(!hash(assetId)||typeof sessionId!=='string'||!/^[a-zA-Z0-9_-]{16,128}$/.test(sessionId))throw Error('Invalid save session identity.');
}
// Send offer over the authenticated lobby channel. Use transfer.host on a
// dedicated ordered save channel with the returned asset. No disk reads here.
export async function prepareSaveTransfer(bytes,{assetId,sessionId,slot=0}) {
  context(assetId,sessionId);
  if(!(bytes instanceof Uint8Array)||!validSize(bytes.length)||![0,1].includes(slot))throw Error('Invalid save snapshot.');
  const snapshot=bytes.slice(), sha256=hex(await digest(snapshot));
  const asset=await prepare(new Blob([snapshot]));
  return {asset,offer:{version:1,assetId,sessionId,slot,size:snapshot.length,sha256,transferId:asset.manifest.id}};
}
// Expected context must come from this lobby, not the incoming offer. Completion
// means verified bytes, never overall game readiness. An optional async install
// callback must succeed before acknowledgment; it must honor cancellation.
export function receiveSaveTransfer(channel,offer,{assetId,sessionId},callbacks={}) {
  context(assetId,sessionId);
  if(!offer||offer.version!==1||offer.assetId!==assetId||offer.sessionId!==sessionId||
    ![0,1].includes(offer.slot)||!validSize(offer.size)||!hash(offer.sha256)||!hash(offer.transferId))throw Error('Save offer does not match this session.');
  // Never spread untrusted offer fields into the verified result: an extra
  // `bytes` property could otherwise replace the payload we just validated.
  const expected={version:1,assetId,sessionId,slot:offer.slot,size:offer.size,
    sha256:offer.sha256,transferId:offer.transferId},chunks=new Map();
  let verified,stopped=false;
  const installation=new AbortController();
  const clear=()=>{stopped=true;installation.abort();chunks.clear();verified=undefined;};
  channel.addEventListener('close',clear);
  const fail=error=>{clear();channel.close();callbacks.error?.(error);};
  const receiver=guest(channel,{
    async get(_id,index){return chunks.get(index);},
    async put(_id,index,chunk){if(!stopped)chunks.set(index,chunk);},
    async complete(manifest){
      const bytes=new Uint8Array(expected.size);
      for(let i=0;i<manifest.count;i++)bytes.set(new Uint8Array(chunks.get(i).bytes),i*manifest.chunk);
      if(hex(await digest(bytes))!==expected.sha256)throw Error('Save snapshot integrity check failed.');
      if(stopped)return;
      chunks.clear();
      if(callbacks.install)await callbacks.install({bytes,...expected},installation.signal);
      if(stopped)return;
      verified=bytes;
    }
  },{
    offer:manifest=>{
      if(manifest.id!==expected.transferId||manifest.size!==expected.size){fail(Error('Unexpected save transfer manifest.'));return;}
      receiver.accept().catch(fail);
    },
    progress:callbacks.progress,
    error:fail,
    complete:()=>{if(!stopped){const bytes=verified;verified=undefined;callbacks.complete?.({bytes,...expected});}}
  });
  return {cancel:()=>{clear();receiver.cancel();}};
}
