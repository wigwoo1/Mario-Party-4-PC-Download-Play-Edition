// Exchange must validate/merge owned slots before returning 44 canonical bytes.
// Cancellation settles the C Asyncify wait even if the provider ignores signal.
export function installBrowserInputExchange(module,exchange,now=()=>performance.now()) {
  if(typeof exchange!=='function'||typeof module?._PartyBoardBrowserRequireOnlineInput!=='function'||
      module.partyBoardInputExchange!==undefined||module.partyBoardCancelInputExchange!==undefined)throw Error('Unused browser input hook and provider required.');
  const controller=new AbortController();let pending=false,pendingStarted=null;
  let completedPolls=0,lastExchangeMs=0,totalExchangeMs=0,maxExchangeMs=0;
  const cancelled=new Promise(resolve=>controller.signal.addEventListener('abort',()=>resolve(null),{once:true}));
  const hook=async (input,metadata)=>{
    if(controller.signal.aborted)return null;
    if(pending||!(input instanceof Uint8Array)||input.length!==44)throw Error('Invalid or overlapping input exchange.');
    const checkpoint=metadata?.checkpoint,frand=metadata?.frand,rand8=metadata?.rand8;
    if(checkpoint!==undefined&&(!Number.isInteger(checkpoint)||checkpoint<0||checkpoint>0xffffffff))throw Error('Invalid input checkpoint.');
    for(const value of [frand,rand8])if(value!==undefined&&(!Number.isInteger(value)||value<0||value>0xffffffff))throw Error('Invalid input RNG state.');
    pending=true;
    const started=now();
    pendingStarted=started;
    try {
      const result=await Promise.race([Promise.resolve().then(()=>{
        if(controller.signal.aborted)return null;
        return exchange(input.slice(),{signal:controller.signal,checkpoint,frand,rand8});
      }),cancelled]);
      if(controller.signal.aborted)return null;
      if(!(result instanceof Uint8Array)||result.length!==44)throw Error('Complete merged pad input required.');
      lastExchangeMs=Math.max(0,now()-started);completedPolls++;
      totalExchangeMs+=lastExchangeMs;maxExchangeMs=Math.max(maxExchangeMs,lastExchangeMs);
      return result.slice();
    } finally {pending=false;pendingStarted=null;}
  };
  const cancel=()=>controller.abort();
  module.partyBoardInputExchange=hook;module.partyBoardCancelInputExchange=cancel;
  try {module._PartyBoardBrowserRequireOnlineInput(1);}
  catch(error){cancel();delete module.partyBoardInputExchange;delete module.partyBoardCancelInputExchange;throw error;}
  return {cancel,timing(){return {completedPolls,lastExchangeMs,totalExchangeMs,maxExchangeMs,pending,pendingExchangeMs:pendingStarted===null?0:Math.max(0,now()-pendingStarted)};},close(){cancel();if(module.partyBoardInputExchange===hook)delete module.partyBoardInputExchange;
    if(module.partyBoardCancelInputExchange===cancel)delete module.partyBoardCancelInputExchange;}};
}
