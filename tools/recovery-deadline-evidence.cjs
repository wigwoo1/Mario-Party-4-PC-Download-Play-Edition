const assert=require('node:assert/strict');

// Acceptance evidence only. Uses observed wall time and the live native timer;
// sixty seconds after a delayed pause alone is not the requested total budget.
function verifyInterruptionDeadline({elapsedMs,nativePauseObservedMs,nativeInitialRemainingMs,observations}){
 assert.ok(elapsedMs>=59000&&elapsedMs<63000,'Cleanup must occur about60 seconds after the real guest closure, including detection');
 assert.ok(Number.isFinite(nativePauseObservedMs)&&nativePauseObservedMs>=0);
 assert.ok(Number.isFinite(nativeInitialRemainingMs)&&nativeInitialRemainingMs>0&&nativeInitialRemainingMs<=60000);
 const nativeDeadlineFromLossMs=nativePauseObservedMs+nativeInitialRemainingMs;
 assert.ok(nativeDeadlineFromLossMs>=58000&&nativeDeadlineFromLossMs<=61500,'Native countdown must include the earlier missing-input wait');
 const paused=observations.filter(row=>row.states.length===2&&row.states.every(state=>state.game?.input.state==='Paused'));
 assert.ok(paused.length>=3,'Need repeated actual paused-game observations');
 const expected=paused[0].states[0].game;
 assert.ok(expected.pending&&Number.isInteger(expected.frame));
 for(const row of observations.filter(row=>row.elapsedMs>=paused[0].elapsedMs&&row.elapsedMs<58000))
  assert.ok(row.states.every(state=>state.game?.input.state==='Paused'),'A surviving player resumed or disappeared before deadline cleanup');
 for(const row of paused)for(const state of row.states){
  assert.equal(state.game.frame,expected.frame,'Surviving game advanced during recovery');
  assert.equal(state.game.input.frame,expected.pending.frame);
  assert.deepEqual(state.game.pending,expected.pending,'Paused checkpoint/RNG/input state changed');
 }
 const visible=paused.filter(row=>row.states.every(state=>state.recoveryVisible));
 assert.ok(visible.length>=1,'Both remaining browser players must see recovery controls');
 for(const row of visible)for(const state of row.states){
  const match=/^(\d+) seconds remaining\.$/.exec(state.countdown);
  assert.ok(match,'Recovery countdown must be displayed');
  const remaining=Number(match[1]),expectedSeconds=(60000-row.elapsedMs)/1000;
  assert.ok(remaining<=Math.ceil(expectedSeconds)+2&&remaining>=Math.max(0,Math.floor(expectedSeconds)-3),
   'Visible countdown must include detection time instead of starting a fresh minute');
 }
 return {nativeDeadlineFromLossMs,pausedObservations:paused.length,visibleCountdownObservations:visible.length,
  scope:'One local actual guest closure; native diagnostic expiry and remaining browser state/countdown. Ordinary native menu return and physical/WAN loss are separate gates.'};
}
module.exports={verifyInterruptionDeadline};
