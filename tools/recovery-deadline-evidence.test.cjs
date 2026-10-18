const {test}=require('node:test'),assert=require('node:assert/strict');
const {verifyInterruptionDeadline}=require('./recovery-deadline-evidence.cjs');
function fixture(){
 const game={frame:5,input:{state:'Paused',frame:'5'},pending:{frame:'5',checkpoint:42,frand:3,rand8:7}};
 return {elapsedMs:60300,nativePauseObservedMs:20340,nativeInitialRemainingMs:39660,
  observations:[21000,40000,58000].map(elapsedMs=>({elapsedMs,states:[0,1].map(()=>({game:structuredClone(game),
   recoveryVisible:true,countdown:Math.ceil((60000-elapsedMs)/1000)+' seconds remaining.'}))}))};
}
test('accepts observed total deadline, charged native countdown and frozen visible players',()=>{
 assert.deepEqual(verifyInterruptionDeadline(fixture()).pausedObservations,3);
});
test('rejects a fresh sixty-second grace period after delayed detection',()=>{
 const old=fixture();old.elapsedMs=80061;old.nativeInitialRemainingMs=60000;
 assert.throws(()=>verifyInterruptionDeadline(old),/including detection/);
 const truncated=fixture();truncated.nativeInitialRemainingMs=60000;
 assert.throws(()=>verifyInterruptionDeadline(truncated),/earlier missing-input wait/);
});
test('rejects checkpoint drift or simulation advancement while paused',()=>{
 for(const field of ['frame','rng']){
  const f=fixture();
  if(field==='frame')f.observations[1].states[0].game.frame++;
  else f.observations[1].states[0].game.pending.frand++;
  assert.throws(()=>verifyInterruptionDeadline(f),/advanced|state changed/);
 }
});
test('rejects hidden recovery controls and a misleading restarted countdown',()=>{
 const hidden=fixture();for(const row of hidden.observations)row.states[0].recoveryVisible=false;
 assert.throws(()=>verifyInterruptionDeadline(hidden),/see recovery controls/);
 const restarted=fixture();restarted.observations[0].states[1].countdown='60 seconds remaining.';
 assert.throws(()=>verifyInterruptionDeadline(restarted),/include detection time/);
});
test('cleanup snapshots are terminal evidence rather than extra paused game frames',()=>{
 const f=fixture();f.observations.push({elapsedMs:60300,states:[{game:null,runtime:null},{game:null,runtime:null}]});
 assert.equal(verifyInterruptionDeadline(f).pausedObservations,3);
});
test('a resumed survivor cannot be hidden by filtering only paused observations',()=>{
 const f=fixture();const rogue=structuredClone(f.observations[1]);rogue.elapsedMs=45000;
 rogue.states[0].game.input.state='Running';f.observations.splice(2,0,rogue);
 assert.throws(()=>verifyInterruptionDeadline(f),/resumed or disappeared/);
});
