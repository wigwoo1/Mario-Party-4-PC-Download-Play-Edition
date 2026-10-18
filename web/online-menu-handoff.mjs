// The synchronous C callback only queues ownership transfer. Never enter another
// Asyncify ccall while the menu's GameRun call is still executing.
export function installOnlineMenuHandoff(runtime,{openLobby,onError}) {
  if(!runtime?.module||typeof runtime.dispose!=='function'||
      typeof openLobby!=='function'||typeof onError!=='function')
    throw Error('Owned runtime and lobby callbacks required.');
  const module=runtime.module;
  if(module.partyBoardOnlineMenuRequest!==undefined)
    throw Error('Online menu handler already installed.');
  let active=true,pending;
  const handler=action=>{
    if(!active||pending||(action!==0&&action!==1))return false;
    pending=Promise.resolve().then(async()=>{
      await runtime.dispose();
      if(active)await openLobby(action===0?'host':'join');
    }).catch(error=>{if(active)onError(error);});
    return true;
  };
  module.partyBoardOnlineMenuRequest=handler;
  return {
    get pending(){return pending;},
    close(){
      active=false;
      if(module.partyBoardOnlineMenuRequest===handler)delete module.partyBoardOnlineMenuRequest;
    },
  };
}
