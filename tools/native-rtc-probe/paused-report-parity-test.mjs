import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {pausedStateReport} from '../../web/paused-state-agreement.mjs';
const native=JSON.parse(execFileSync(process.argv[2],['--paused-wire'],{encoding:'utf8'}));
const identity={session:'1'.repeat(32),buildId:'b'.repeat(64),assetId:'a'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,
 players:[{peer:'host',slot:0},{peer:'g1',slot:1},{peer:'g2',slot:2},{peer:'g3',slot:3}]};
const browser=pausedStateReport(identity,{attempt:{peer:1,epoch:1n,revision:1n,frame:7n,checkpoint:42,frand:0,rand8:5},
 state:{frame:10,checkpoint:42,frand:0,rand8:5},input:{state:'Paused',frame:7n,epoch:1n,revision:1n}},
 {peer:'host',nonce:'c'.repeat(32)});
assert.deepEqual(native,browser);
console.log('PASS compiled native/browser paused-state report parity; fixture state only');
