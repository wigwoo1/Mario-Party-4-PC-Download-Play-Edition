const uint32=value=>Number.isInteger(value)&&value>=0&&value<=0xffffffff;
const hex=(value,length)=>typeof value==='string'&&new RegExp(`^[a-f0-9]{${length}}$`).test(value);

// Identity is the trusted accepted start proposal, not a received message.
// Caller captures actual paused state and authenticates the sending host.
export function createRecoveryCommit(identity,attempt,state,input) {
  if(!hex(identity?.session,32)||!hex(identity?.buildId,64)||!hex(identity?.assetId,64)||
      identity.profile!=='unlocked-no-save-v1'||!uint32(identity.seed)||
      !Number.isSafeInteger(identity.revision)||identity.revision<0||
      !Number.isSafeInteger(identity.membership)||identity.membership<0||identity.membership>identity.revision||
      !Array.isArray(identity.players)||identity.players.length<2||identity.players.length>4||
      !uint32(attempt?.peer)||attempt.peer===0||typeof attempt.epoch!=='bigint'||attempt.epoch<1n||
      typeof attempt.revision!=='bigint'||attempt.revision<1n||
      !['frame','checkpoint','frand','rand8'].every(key=>uint32(state?.[key]))||
      input?.state!=='Paused'||typeof input.frame!=='bigint'||input.frame<0n||input.frame>0xffffffffffffffffn||
      input.epoch!==attempt.epoch||input.revision!==attempt.revision)
    throw Error('Trusted session identity, current recovery attempt and actual paused state required.');
  const peers=new Set(),slots=new Set();
  for(const player of identity.players) {
    if(typeof player?.peer!=='string'||!player.peer||player.peer.length>128||peers.has(player.peer)||
        !Number.isInteger(player.slot)||player.slot<0||player.slot>3||slots.has(player.slot))
      throw Error('Trusted assigned player list required.');
    peers.add(player.peer);slots.add(player.slot);
  }
  return {type:'session-recovery-commit',version:1,session:identity.session,buildId:identity.buildId,
    assetId:identity.assetId,profile:identity.profile,seed:identity.seed,
    membership:identity.membership,lobbyRevision:identity.revision,players:structuredClone(identity.players),
    peer:attempt.peer,epoch:attempt.epoch.toString(),revision:attempt.revision.toString(),
    frame:state.frame,checkpoint:state.checkpoint,frand:state.frand,rand8:state.rand8,inputFrame:input.frame.toString()};
}

export function matchesRecoveryCommit(message,identity,attempt,state,input) {
  const expected=createRecoveryCommit(identity,attempt,state,input);
  if(!message||typeof message!=='object')return false;
  for(const key of Object.keys(expected)) {
    if(key==='players') {if(JSON.stringify(message.players)!==JSON.stringify(expected.players))return false;}
    else if(message[key]!==expected[key])return false;
  }
  return true;
}
