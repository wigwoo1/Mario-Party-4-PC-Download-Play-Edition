import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {recoveryAcknowledgment,acceptRecoveryCommit} from '../../web/recovery-barrier.mjs';
import {pausedStateReport} from '../../web/paused-state-agreement.mjs';
const executable=process.argv[2];
assert.ok(executable,'Native recovery barrier executable required');
const identity={session:'1'.repeat(32),buildId:'b'.repeat(64),assetId:'a'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,players:[{peer:'host',slot:0},{peer:'g1',slot:1},{peer:'g2',slot:2},{peer:'g3',slot:3}]};
const captured={attempt:{peer:1,epoch:1n,revision:1n,frame:7n,checkpoint:42,frand:0,rand8:5},state:{frame:10,checkpoint:42,frand:0,rand8:5},input:{state:'Paused',frame:7n,epoch:1n,revision:1n}};
const {reporter,...state}=pausedStateReport(identity,captured,{peer:'host',nonce:'c'.repeat(32)});
const expected={...state,type:'session-recovery-proposal',host:'host'};
const acknowledgments=identity.players.slice(1).map(({peer})=>recoveryAcknowledgment(expected,identity,captured,{peer,host:'host'}));
const run=spawnSync(executable,['--wire'],{input:acknowledgments.map(value=>JSON.stringify(value)).join('\n')+'\n',encoding:'utf8',timeout:10000});
assert.ifError(run.error);assert.equal(run.status,0,run.stderr);
const [proposal,commit]=run.stdout.trim().split(/\r?\n/).map(line=>JSON.parse(line));
assert.deepEqual(proposal,expected);
for(const {peer} of identity.players)assert.equal(acceptRecoveryCommit(commit,proposal,identity,captured,{peer,host:'host'}),true);
const wrong=structuredClone(commit);wrong.rand8++;
assert.throws(()=>acceptRecoveryCommit(wrong,proposal,identity,captured,{peer:'g1',host:'host'}));
for(const [message,accepted] of [[commit,true],[wrong,false]]){
 const result=spawnSync(executable,['--accept'],{input:JSON.stringify(message)+'\n',encoding:'utf8',timeout:10000});
 assert.ifError(result.error);assert.equal(result.status,accepted?0:2,result.stderr);
}
const spoofed=structuredClone(acknowledgments);spoofed[0].peer='g2';
const rejected=spawnSync(executable,['--wire'],{input:spoofed.map(value=>JSON.stringify(value)).join('\n')+'\n',encoding:'utf8',timeout:10000});
assert.ifError(rejected.error);assert.notEqual(rejected.status,0);
console.log('PASS native proposal → browser acknowledgment → native all-player commit → browser validation; spoofed acknowledgment and changed RNG rejected. Protocol fixture only; no game/reconnect claim.');
