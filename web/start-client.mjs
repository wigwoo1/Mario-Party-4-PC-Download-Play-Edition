import {StartBarrier} from './start-barrier.mjs';
import {requirePreparedOnlineInput} from './online-input-gate.mjs';

// Called only for the current authenticated host connection. The preparation
// callback must return a paused initialized runtime, its actual checkpoint and
// an idempotent dispose(). Caller owns returned runtime and aborts on leaving.
export async function prepareStartAcknowledgment(proposal,{peer,buildId,snapshot,prepare,signal,requireOnlineInput=false}) {
  signal?.throwIfAborted();
  if(typeof snapshot!=='function'||typeof prepare!=='function'||!proposal||proposal.type!=='session-start'||
    proposal.version!==1||typeof proposal.session!=='string'||!/^[a-f0-9]{32}$/.test(proposal.session))
    throw Error('Invalid host start proposal.');
  const session=Uint8Array.from(proposal.session.match(/../g),byte=>parseInt(byte,16));
  const barrier=new StartBarrier(snapshot(),{session,seed:proposal.seed,buildId,checkpoint:proposal.checkpoint});
  const expected=barrier.proposal;
  for(const key of ['revision','membership','assetId','total','buildId','profile'])
    if(proposal[key]!==expected[key])throw Error('Start proposal does not match this lobby or build.');
  if(JSON.stringify(proposal.players)!==JSON.stringify(expected.players)||
    !expected.players.some(player=>player.peer===peer))throw Error('Start player assignment differs.');
  let runtime;
  try {
    runtime=await prepare(structuredClone(expected),{signal});
    signal?.throwIfAborted();
    requirePreparedOnlineInput(runtime,requireOnlineInput);
    if(!runtime||typeof runtime.dispose!=='function'||!Number.isInteger(runtime.checkpoint)||
      runtime.checkpoint<0||runtime.checkpoint>0xffffffff)throw Error('Initialized runtime checkpoint required.');
    // Mounted session runtimes carry their verified asset identity. Historical
    // engine-only diagnostics have no mount, but may not override this identity.
    if(runtime.assetId!==undefined&&runtime.assetId!==expected.assetId)
      throw Error('Prepared runtime asset differs from start proposal.');
    if(runtime.buildId!==undefined&&runtime.buildId!==expected.buildId)
      throw Error('Prepared runtime build differs from start proposal.');
    if(runtime.sessionSeed!==undefined&&runtime.sessionSeed!==expected.seed)
      throw Error('Prepared runtime seed differs from start proposal.');
    if(runtime.engine&&(runtime.engine.phase!=='Idle'||
        typeof runtime.engine.preparedCheckpoint!=='function'||
        runtime.engine.preparedCheckpoint()!==expected.checkpoint))
      throw Error('Prepared engine is not idle at the agreed checkpoint.');
    if(!barrier.update(snapshot()))throw Error('Lobby changed during preparation.');
    if(runtime.checkpoint!==expected.checkpoint)throw Error('Initialized simulation state differs.');
    return {acknowledgment:{...expected,type:'session-start-ack',checkpoint:runtime.checkpoint},runtime};
  } catch(error) {
    try {if(typeof runtime?.dispose==='function')await runtime.dispose();}
    catch(cleanupError){throw new AggregateError([error,cleanupError],'Start preparation and cleanup failed.');}
    throw error;
  }
}

// Invoke only for the current authenticated host connection. Authentication,
// commit delivery and synchronized input ownership remain transport concerns.
export function resumeCommittedClientRuntime(commit,prepared,{peer,buildId,snapshot,requireOnlineInput=false}) {
  const expected=prepared?.acknowledgment;
  if(!expected||expected.type!=='session-start-ack'||!commit||commit.type!=='session-start-commit')
    throw Error('Accepted preparation and host start commit required.');
  for(const key of ['version','session','revision','membership','assetId','total','buildId','seed','profile','checkpoint'])
    if(commit[key]!==expected[key])throw Error('Host commit differs from accepted preparation.');
  if(JSON.stringify(commit.players)!==JSON.stringify(expected.players))throw Error('Host commit player assignment differs.');
  if(typeof snapshot!=='function'||buildId!==expected.buildId||!expected.players.some(player=>player.peer===peer))
    throw Error('Client identity or build differs from preparation.');
  const session=Uint8Array.from(expected.session.match(/../g),byte=>parseInt(byte,16));
  const barrier=new StartBarrier(snapshot(),{session,seed:expected.seed,buildId,checkpoint:expected.checkpoint});
  const current=barrier.proposal;
  for(const key of ['revision','membership','assetId','total','profile'])
    if(current[key]!==expected[key])throw Error('Lobby changed since preparation.');
  if(JSON.stringify(current.players)!==JSON.stringify(expected.players))throw Error('Lobby player assignment changed.');
  const runtime=prepared.runtime;
  requirePreparedOnlineInput(runtime,requireOnlineInput);
  if(runtime?.buildId!==undefined&&runtime.buildId!==expected.buildId)
    throw Error('Prepared runtime build differs from accepted preparation.');
  if(runtime?.assetId!==undefined&&runtime.assetId!==expected.assetId)
    throw Error('Prepared runtime asset differs from accepted preparation.');
  if(runtime?.sessionSeed!==expected.seed||runtime.checkpoint!==expected.checkpoint||runtime.engine?.phase!=='Idle'||
      runtime.engine.preparedCheckpoint()!==expected.checkpoint)throw Error('Prepared client runtime differs from commit.');
  return runtime.engine.resume();
}
