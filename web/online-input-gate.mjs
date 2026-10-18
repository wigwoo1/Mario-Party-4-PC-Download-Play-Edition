// Checks installation, not peer authentication or synchronized queue readiness.
export function requirePreparedOnlineInput(runtime,required=false) {
  if(typeof required!=='boolean')throw Error('Boolean online input requirement required.');
  if(!required)return;
  const module=runtime?.module;
  if(runtime?.inputExchangeInstalled!==true||
      typeof module?.partyBoardInputExchange!=='function'||
      typeof module?.partyBoardCancelInputExchange!=='function'||
      typeof module?._PartyBoardBrowserOnlineInputRequired!=='function'||
      module._PartyBoardBrowserOnlineInputRequired()!==1)
    throw Error('Prepared online input hook required before start.');
}
