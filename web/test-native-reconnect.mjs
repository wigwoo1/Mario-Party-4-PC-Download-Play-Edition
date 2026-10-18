import {execFileSync} from 'node:child_process';
import assert from 'node:assert/strict';
import {ReconnectPolicy} from './reconnect-policy.mjs';
if(!process.argv[2])throw Error('Pass compiled reconnect-vectors executable');
const native=execFileSync(process.argv[2],[],{encoding:'utf8',timeout:10000}).trim().split(/\r?\n/);
const policy=new ReconnectPolicy([11,22,22,33]), observed=[];
function emit(accepted,now){
  const remaining=policy.remainingMs(now);
  const missing=policy.missingSlots.reduce((mask,value,i)=>mask|(value?1<<i:0),0);
  observed.push(`${Number(accepted)} ${['Running','Paused','ReturnToMenu'].indexOf(policy.state)} ${policy.epoch} ${policy.revision} ${remaining} ${missing}`);
}
emit(policy.disconnected(22,100),100);
emit(policy.disconnected(33,1000),1000);
emit(policy.markRecovered(22,1n,1n,1100),1100);
emit(policy.markRecovered(22,1n,2n,1200),1200);
emit(policy.markRecovered(33,1n,2n,1300),1300);
emit(policy.resume(1n,1n,1400),1400);
emit(policy.disconnected(22,1500),1500);
emit(policy.markRecovered(22,1n,2n,1600),1600);
emit(policy.markRecovered(22,1n,3n,60099),60099);
emit(policy.resume(1n,3n,60099),60099);
emit(policy.disconnected(11,61000),61000);
emit(policy.markRecovered(11,2n,4n,121000),121000);
emit(policy.resume(2n,4n,121001),121001);
assert.deepEqual(observed,native);
console.log('C++/JavaScript reconnect conformance passed: 13 transitions and deadlines');
