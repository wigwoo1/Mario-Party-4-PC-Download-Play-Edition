import {test} from 'node:test';
import assert from 'node:assert/strict';
import {createRecoveryCommit,matchesRecoveryCommit} from './recovery-commit.mjs';
const identity={session:'01'.repeat(16),buildId:'b'.repeat(64),assetId:'a'.repeat(64),profile:'unlocked-no-save-v1',
  seed:0,revision:8,membership:4,players:[{peer:'host',slot:0},{peer:'guest',slot:1}]};
const attempt={peer:1,epoch:1n,revision:1n},state={frame:10,checkpoint:0xffffffff,frand:0,rand8:42};
const input={state:'Paused',frame:7n,epoch:1n,revision:1n};
test('recovery commit uses exact session, attempt and paused state; wire values are JSON-safe',()=>{
  const commit=createRecoveryCommit(identity,attempt,state,input);assert.ok(matchesRecoveryCommit(JSON.parse(JSON.stringify(commit)),identity,attempt,state,input));
  for(const change of [{inputFrame:'8'},{session:'02'.repeat(16)},{epoch:'2'},{revision:'2'},{frame:11},{checkpoint:0},
    {frand:1},{rand8:43},{seed:1},{players:[]},{profile:'save-enabled'},{peer:2},{buildId:'c'.repeat(64)}])
    assert.equal(matchesRecoveryCommit({...commit,...change},identity,attempt,state,input),false);
  commit.players[0].slot=3;assert.equal(identity.players[0].slot,0);
});
test('missing paused state and invalid attempt never create recovery authorization',()=>{
  assert.throws(()=>createRecoveryCommit(identity,attempt,{...state,frand:undefined},input));
  assert.throws(()=>createRecoveryCommit(identity,{...attempt,epoch:0n},state,input));
  assert.throws(()=>createRecoveryCommit(identity,attempt,{...state,frame:-1},input));
});
test('input queue position must be paused and share recovery epoch independently of engine frame',()=>{
  const commit=createRecoveryCommit(identity,attempt,state,input);assert.equal(commit.frame,10);assert.equal(commit.inputFrame,'7');
  for(const change of [{frame:8n},{epoch:2n},{revision:2n},{state:'Running'}]) {
    const changed={...input,...change};
    if(change.frame!==undefined)assert.equal(matchesRecoveryCommit(commit,identity,attempt,state,changed),false);
    else assert.throws(()=>createRecoveryCommit(identity,attempt,state,changed));
  }
  assert.throws(()=>createRecoveryCommit(identity,attempt,state));
});
