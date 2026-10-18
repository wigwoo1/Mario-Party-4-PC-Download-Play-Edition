'use strict';
const net=require('node:net');

function publicIpv4(value){
 if(typeof value!=='string'||net.isIP(value)!==4)return false;
 const [a,b,c]=value.split('.').map(Number);
 return a!==0&&a!==10&&a!==127&&a<224&&!(a===100&&b>=64&&b<=127)&&!(a===169&&b===254)&&
  !(a===172&&b>=16&&b<=31)&&!(a===192&&(b===168||b===0||(b===88&&c===99)))&&
  !(a===198&&(b===18||b===19||(b===51&&c===100)))&&!(a===203&&b===0&&c===113);
}
function ipOrigin(address){
 if(!publicIpv4(address))throw Error('No usable public IPv4 address.');
 return 'https://partyboard.'+address.replaceAll('.','-')+'.sslip.io';
}
function tunnelOrigin(output){
 const match=String(output).match(/https:\/\/([a-z0-9]+(?:-[a-z0-9]+)*\.trycloudflare\.com)(?![a-z0-9.-])/i);
 return match?'https://'+match[1].toLowerCase():null;
}
// Adapter owns processes, bounded probes and cancellation. Nothing is published
// until the exact instance and runtime have passed HTTPS checks. Once published,
// do not change origin or restart the room registry underneath a running match.
async function automaticHosting(adapter){
 let service,direct,fallback,closed=false,closing;
 const close=()=>closing??=(async()=>{closed=true;await Promise.allSettled([direct?.close(),fallback?.close()]);await service?.close();})();
 const check=async resource=>{if(closed||adapter.signal?.aborted){await resource?.close();throw Error('Online hosting startup cancelled.');}};
 const abort=()=>{close().catch(()=>{});};
 adapter.signal?.addEventListener('abort',abort,{once:true});
 try{
  service=await adapter.service();
  await check(service);
  let result;
  try{
   const origin=ipOrigin(await adapter.publicAddress());
   await check();
   direct=await adapter.direct(origin,service);
   await check(direct);
   await adapter.verify(origin,service,direct);
   result={mode:'direct',origin};
  }catch(error){
   await direct?.close();direct=null;
   await check();
   adapter.notice('Direct hosting unavailable; trying the free fallback.');
   fallback=await adapter.fallback(service);
   await check(fallback);
   if(tunnelOrigin(fallback.origin)!==fallback.origin)throw Error('The fallback did not provide a valid HTTPS address.');
   await adapter.verify(fallback.origin,service,fallback);
   result={mode:'fallback',origin:fallback.origin};
  }
  await check();await service.publish(result.origin);await check();
  return {...result,service,close};
 }catch(error){await close();throw error;}
 finally{adapter.signal?.removeEventListener('abort',abort);}
}
module.exports={publicIpv4,ipOrigin,tunnelOrigin,automaticHosting};
