import {StartBarrier} from './start-barrier.mjs';
import {requirePreparedOnlineInput} from './online-input-gate.mjs';

// Local host gate only; authenticated transport must distribute a commit to
// clients separately. This does not authorize unsynchronized network frames.
export function prepareAgreedHostCommit(barrier,runtime,snapshot,{requireOnlineInput=false}={}) {
  if(!(barrier instanceof StartBarrier)||typeof snapshot!=='function')
    throw Error('Host start barrier and current lobby snapshot required.');
  if(!barrier.update(snapshot())||barrier.state!=='Agreed')
    throw Error('Every current player must acknowledge before host execution.');
  const proposal=barrier.proposal;
  requirePreparedOnlineInput(runtime,requireOnlineInput);
  if(runtime?.buildId!==undefined&&runtime.buildId!==proposal.buildId)
    throw Error('Prepared host runtime build differs from agreed start.');
  if(runtime?.assetId!==undefined&&runtime.assetId!==proposal.assetId)
    throw Error('Prepared host runtime asset differs from agreed start.');
  if(!runtime?.engine||runtime.engine.phase!=='Idle'||runtime.sessionSeed!==proposal.seed||
      runtime.checkpoint!==proposal.checkpoint||runtime.engine.preparedCheckpoint()!==proposal.checkpoint)
    throw Error('Prepared host runtime differs from the agreed start.');
  // Validate the host before caller-owned delivery, without advancing frames.
  // This does not verify remote channel readiness or atomic commit delivery.
  return {...proposal,type:'session-start-commit'};
}

export function resumeAgreedHostRuntime(barrier,runtime,snapshot,options) {
  prepareAgreedHostCommit(barrier,runtime,snapshot,options);
  // Revalidate immediately before resume, even if a commit was prepared earlier.
  return runtime.engine.resume();
}
