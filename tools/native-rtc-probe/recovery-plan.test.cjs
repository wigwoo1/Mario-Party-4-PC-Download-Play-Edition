'use strict';
const {test}=require('node:test'),assert=require('node:assert/strict');
const {recoveryPlan}=require('./recovery-plan.cjs');
const env={PARTYBOARD_NATIVE_FRONTEND_RECOVERY:'1',PARTYBOARD_NATIVE_GUEST_SELF_RECOVERY:'1',PARTYBOARD_NATIVE_FRONTEND_RECOVERY_ROUNDS:'1',PARTYBOARD_NATIVE_RECOVERY_TURNS:'1'};
const options={focusedPlay:false,fullMatch:true,fullMatchTurns:5};
test('focused minigame diagnostics exclude inherited reconnect actions and claims',()=>{
 assert.deepEqual(recoveryPlan(env,{...options,focusedPlay:true}),{recoverFrontend:false,nativeSelfRecovery:false,recoveryRounds:0,recoveryTurns:[]});
});
test('full-match recovery preserves requested round and turn',()=>{
 assert.deepEqual(recoveryPlan(env,options),{recoverFrontend:true,nativeSelfRecovery:true,recoveryRounds:1,recoveryTurns:[1]});
});
test('conflicting, out-of-range and fractional recovery plans fail before launch',()=>{
 for(const change of [{PARTYBOARD_NATIVE_FRONTEND_RECOVERY:'0'},{PARTYBOARD_NATIVE_RECOVERY_TURNS:'6'},{PARTYBOARD_NATIVE_FRONTEND_RECOVERY_ROUNDS:'2'},{PARTYBOARD_NATIVE_FRONTEND_RECOVERY_ROUNDS:'1.5'}])assert.throws(()=>recoveryPlan({...env,...change},options));
});
test('short recovery probe without turn gates remains supported',()=>{
 assert.deepEqual(recoveryPlan({...env,PARTYBOARD_NATIVE_RECOVERY_TURNS:''},{...options,fullMatch:false}).recoveryTurns,[]);
});
