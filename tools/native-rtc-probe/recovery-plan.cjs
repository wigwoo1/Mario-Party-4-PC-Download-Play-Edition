'use strict';
const assert=require('node:assert/strict');
function recoveryPlan(env,{focusedPlay,fullMatch,fullMatchTurns}){
 const recoverFrontend=!focusedPlay&&env.PARTYBOARD_NATIVE_FRONTEND_RECOVERY==='1';
 const nativeSelfRecovery=!focusedPlay&&env.PARTYBOARD_NATIVE_GUEST_SELF_RECOVERY==='1';
 const recoveryRounds=recoverFrontend?Number(env.PARTYBOARD_NATIVE_FRONTEND_RECOVERY_ROUNDS||1):0;
 assert(Number.isInteger(recoveryRounds)&&recoveryRounds>=0&&recoveryRounds<=2,'Recovery rounds must be 0 through 2');
 const recoveryTurns=focusedPlay?[]:(env.PARTYBOARD_NATIVE_RECOVERY_TURNS||'').split(',').filter(Boolean).map(Number);
 assert(recoveryTurns.every(turn=>Number.isInteger(turn)&&turn>=1&&turn<=fullMatchTurns),'Recovery turns must be within the actual match');
 assert(!recoveryTurns.length||(fullMatch&&recoverFrontend&&recoveryTurns.length===recoveryRounds),'Recovery turns require a matching enabled full-match recovery plan');
 return {recoverFrontend,nativeSelfRecovery,recoveryRounds,recoveryTurns};
}
module.exports={recoveryPlan};
