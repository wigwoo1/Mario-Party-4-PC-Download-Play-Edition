// Cleanup can wrap the original browser exception. Keep storage guidance even
// when that exception is nested, without mistaking unrelated text for quota.
export function preparationFailureMessage(error,{automatic=false}={}){
  const prefix=automatic?'Automatic game preparation failed: ':'Lobby game preparation failed: ';
  const pending=[error],seen=new Set();
  const details=[],startupLogs=[];
  while(pending.length&&seen.size<32){
    const cause=pending.pop();
    if(!cause||typeof cause!=='object'||seen.has(cause))continue;
    seen.add(cause);
    if(cause.name==='QuotaExceededError')return prefix+
      'This browser could not reserve enough game storage. Stop this session, then free space for this site or open the lobby in a regular browser window and rejoin. Verified downloads are not removed automatically.';
    if(Array.isArray(cause.partyBoardRuntimeLogs))for(const line of cause.partyBoardRuntimeLogs){
      if(typeof line!=='string'||!/(?:error|failed|fatal|abort|warning|warn)/i.test(line))continue;
      const safe=line.replace(/(?:https?|blob):\/\/\S+/gi,'[resource]').replace(/[A-Za-z]:\\[^\s]+/g,'[path]').slice(0,180);
      if(!startupLogs.includes(safe))startupLogs.push(safe);
    }
    const children=[];
    if(cause.cause)children.push(cause.cause);
    if(Array.isArray(cause.errors))children.push(...cause.errors.slice(0,32));
    // Keep the wrapper message so the user can tell that cleanup also failed,
    // but include the underlying cause that the previous UI hid.
    const message=String(cause.message??'').trim();
    if(message&&!details.includes(message))details.push(message.slice(0,240));
    pending.push(...children.reverse());
  }
  const summary=details.slice(0,4).join(' Details: ')
    ||String(error?.message??error);
  const logSummary=startupLogs.length?' Graphics log: '+startupLogs.slice(-3).join(' / '):'';
  return prefix+summary+logSummary+(automatic?' Stop the session and try again.':'');
}
