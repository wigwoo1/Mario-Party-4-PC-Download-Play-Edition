'use strict';
// An unresponsive Wasm page may reject ordinary evaluate/screenshot requests.
// Ask its existing owner connection for debugger stacks, then always resume.
async function captureBrowserStack(page,timeoutMs=5000){
 let session,timer,paused;
 const bounded=operation=>Promise.race([operation,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Debugger observation timed out')),timeoutMs);})]).finally(()=>clearTimeout(timer));
 try{
  session=await bounded(page.context().newCDPSession(page));
  await bounded(session.send('Debugger.enable'));
  const event=new Promise(resolve=>session.once('Debugger.paused',resolve));
  await bounded(session.send('Debugger.pause'));
  paused=await bounded(event);
  return {reason:paused.reason,frames:paused.callFrames.map(frame=>({functionName:frame.functionName,url:frame.url,location:frame.location}))};
 }catch(error){return {error:String(error)};}
 finally{if(session){await bounded(session.send('Debugger.resume')).catch(()=>{});await bounded(session.send('Debugger.disable')).catch(()=>{});await bounded(session.detach()).catch(()=>{});}}
}
module.exports={captureBrowserStack};
