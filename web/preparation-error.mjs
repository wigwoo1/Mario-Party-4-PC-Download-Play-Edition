// Cleanup can wrap the original browser exception. Keep storage guidance even
// when that exception is nested, without mistaking unrelated text for quota.
export function preparationFailureMessage(error,{automatic=false}={}){
  const prefix=automatic?'Automatic game preparation failed: ':'Lobby game preparation failed: ';
  const pending=[error],seen=new Set();
  while(pending.length&&seen.size<32){
    const cause=pending.pop();
    if(!cause||typeof cause!=='object'||seen.has(cause))continue;
    seen.add(cause);
    if(cause.name==='QuotaExceededError')return prefix+
      'This browser could not reserve enough game storage. Stop this session, then free space for this site or open the lobby in a regular browser window and rejoin. Verified downloads are not removed automatically.';
    if(cause.cause)pending.push(cause.cause);
    if(Array.isArray(cause.errors))pending.push(...cause.errors.slice(0,32));
  }
  return prefix+String(error?.message??error)+(automatic?' Stop the session and try again.':'');
}
