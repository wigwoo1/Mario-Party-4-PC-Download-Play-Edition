// Test-only synthetic session. Never use these fixed identities for an online lobby.
import {Lockstep} from './lockstep.mjs';
import {encodeInput} from './input-packet.mjs';
export function probe(channel, local, peer, disconnectAt = null) {
  if(disconnectAt !== null && (!Number.isInteger(disconnectAt) || disconnectAt < 0 || disconnectAt >= 120))
    throw new TypeError('Invalid test disconnect frame');
  const session=Uint8Array.of(9,...Array(15).fill(0)), owners=[11,22,11,22];
  const queue=new Lockstep(session,owners);
  let hash=42, submitted=-1n, finished=false;
  channel.binaryType='arraybuffer';
  return new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>fail(Error('Lockstep probe deadline')),15000);
    function fail(error){if(!finished){finished=true;clearTimeout(timer);reject(error);}}
    function pump(){
      if(finished || channel.readyState!=='open')return;
      try {
        const frame=queue.nextFrame;
        if(disconnectAt !== null && frame===BigInt(disconnectAt)){
          channel.close();fail(Error('Injected lockstep probe disconnect'));return;
        }
        if(submitted!==frame){
          submitted=frame;
          for(let slot=0;slot<4;slot++)if(owners[slot]===local){
            const pad=Uint8Array.from({length:11},(_,i)=>frame===120n?0:(Number(frame)+slot*13+i)&255);
            const bytes=encodeInput({session,frame,slot,checkpoint:hash,pad});
            if(queue.receive(local,bytes)!=='Accepted')throw Error('Local input rejected');
            channel.send(bytes);
          }
        }
        const result=queue.advance(hash);
        if(!result){if(queue.stopped!=='None')throw Error(`Lockstep stopped: ${queue.stopped}`);return;}
        if(frame===120n){finished=true;clearTimeout(timer);resolve({frames:120,hash});return;}
        for(const pad of result.pads)for(const byte of pad)hash=Math.imul(hash^byte,16777619)>>>0;
        pump();
      }catch(error){fail(error);}
    }
    channel.onmessage=event=>{
      if(finished)return;
      const result=queue.receive(peer,new Uint8Array(event.data));
      if(result!=='Accepted' && result!=='Duplicate'){fail(Error(`Remote input rejected: ${result}`));return;}
      pump();
    };
    channel.onclose=()=>{if(!finished)fail(Error('Lockstep probe disconnected'));};
    channel.onerror=()=>fail(Error('Lockstep probe channel error'));
    channel.onopen=pump; pump();
  });
}
