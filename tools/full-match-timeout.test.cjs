'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const {DEFAULT_FULL_MATCH_TIMEOUT_MS,fullMatchDeadline,fullMatchHarnessTimeout,fullMatchTimeoutFromEnvironment}=require('./drive-production-full-match.cjs');

test('full-match runner default allows six hours for slow real-time minigames and final rankings',()=>{
 assert.equal(DEFAULT_FULL_MATCH_TIMEOUT_MS,6*60*60*1000);
 assert.equal(fullMatchDeadline(1000),1000+6*60*60*1000);
});

test('full-match deadline accepts a positive explicit timeout',()=>{
 assert.equal(fullMatchDeadline(1200,45_000),46_200);
});

test('browser harness timeout override is optional and validated',()=>{
 assert.equal(fullMatchTimeoutFromEnvironment({}),undefined);
 assert.equal(fullMatchTimeoutFromEnvironment({PARTYBOARD_TEST_GAME_FULL_MATCH_TIMEOUT_MS:'2700000'}),2_700_000);
 for(const value of ['0','-1','1.5','NaN','Infinity','9007199254740992'])assert.throws(()=>fullMatchTimeoutFromEnvironment({PARTYBOARD_TEST_GAME_FULL_MATCH_TIMEOUT_MS:value}));
});

test('outer harness guard follows the match timeout with a cleanup margin',()=>{
 assert.equal(fullMatchHarnessTimeout(),6*60*60*1000+10*60*1000);
 assert.equal(fullMatchHarnessTimeout(45*60*1000),55*60*1000);
 assert.throws(()=>fullMatchHarnessTimeout(Number.MAX_SAFE_INTEGER));
});

test('full-match deadline rejects invalid start times, timeouts, and overflow',()=>{
 for(const start of [-1,NaN,Infinity,Number.MAX_SAFE_INTEGER+1])assert.throws(()=>fullMatchDeadline(start));
 for(const timeout of [0,-1,NaN,Infinity,1.5,Number.MAX_SAFE_INTEGER])assert.throws(()=>fullMatchDeadline(1,timeout));
});
