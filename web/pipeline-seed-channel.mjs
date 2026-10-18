const limit=32*1024**2,chunk=16384;
export async function sendPipelineSeed(channel,seed,{signal}={}){
 if(!(seed?.blob instanceof Blob)||seed.blob.size<100||seed.blob.size>limit||!/^[a-f0-9]{64}$/.test(seed.sha256))throw Error('Invalid pipeline transfer.');
 signal?.throwIfAborted();channel.send(JSON.stringify({type:'pipeline-seed',version:1,size:seed.blob.size,sha256:seed.sha256}));
 for(let offset=0;offset<seed.blob.size;offset+=chunk){signal?.throwIfAborted();if(channel.readyState!=='open')throw Error('Pipeline channel closed.');
  if(channel.bufferedAmount>1024*1024)await new Promise((resolve,reject)=>{
    channel.bufferedAmountLowThreshold=512*1024;
    const cleanup=()=>{channel.removeEventListener('bufferedamountlow',ready);channel.removeEventListener('close',lost);signal?.removeEventListener('abort',lost);};
    const ready=()=>{cleanup();resolve();},lost=()=>{cleanup();reject(Error('Pipeline transfer cancelled.'));};
    channel.addEventListener('bufferedamountlow',ready,{once:true});channel.addEventListener('close',lost,{once:true});signal?.addEventListener('abort',lost,{once:true});
    if(signal?.aborted||channel.readyState!=='open')lost();else if(channel.bufferedAmount<=channel.bufferedAmountLowThreshold)ready();
  });
  channel.send(await seed.blob.slice(offset,offset+chunk).arrayBuffer());
 }
}
export function receivePipelineSeed(channel,{complete,error,signal}){
 let header,parts=[],received=0,finished=false;channel.binaryType='arraybuffer';
 const close=()=>{finished=true;parts=[];channel.removeEventListener('message',message);};
 const message=async event=>{if(finished)return;try{
  signal?.throwIfAborted();
  if(!header){if(typeof event.data!=='string'||event.data.length>512)throw Error('Invalid pipeline header.');header=JSON.parse(event.data);
   if(header.type!=='pipeline-seed'||header.version!==1||!Number.isInteger(header.size)||header.size<100||header.size>limit||!/^[a-f0-9]{64}$/.test(header.sha256))throw Error('Invalid pipeline header.');return;}
  if(!(event.data instanceof ArrayBuffer)||event.data.byteLength!==Math.min(chunk,header.size-received))throw Error('Invalid pipeline chunk.');
  parts.push(event.data.slice(0));received+=event.data.byteLength;
  if(received===header.size){finished=true;const blob=new Blob(parts);parts=[];
   const bytes=await blob.arrayBuffer(),hash=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),b=>b.toString(16).padStart(2,'0')).join('');
   signal?.throwIfAborted();if(hash!==header.sha256)throw Error('Pipeline transfer integrity mismatch.');
   complete({blob,sha256:hash});close();}
 }catch(cause){close();error(cause);}};
 channel.addEventListener('message',message);signal?.addEventListener('abort',close,{once:true});return {close};
}
