import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {LobbyState} from '../web/lobby-state.mjs';
const asset='a'.repeat(64),total=50000,operations=[];
const join=(peer)=>operations.push({kind:'join',peer,name:peer});
const report=(peer,sequence,bytes,verified,shaders,ready,membership,extra={})=>operations.push({kind:'report',peer,report:{assetId:asset,sequence,bytes,verified,shaders,ready,membership,...extra}});
join('host');join('guest1');
report('host',1,total,true,true,true,2);
report('guest1',1,100,false,false,false,2);
report('guest1',2,total,true,true,true,2);
join('guest2');
report('guest1',3,total,true,true,true,2); // crossed membership: retain progress, invalidate ready
report('host',2,total,true,true,true,3);
report('guest1',4,total,true,true,true,3);
report('guest2',1,total,true,true,true,3);
join('guest3');join('guest4'); // fifth player rejects without mutation
report('guest3',1,total,true,true,true,4);
report('guest3',1,total,true,true,true,4); // duplicate sequence
report('guest2',2,total,true,true,true,5); // future membership
report('guest2',2,total-1,false,false,false,4); // backwards progress
report('intruder',1,total,true,true,true,4);
report('host',3,total,true,true,true,4,{verified:1});
report('host',3,total,true,true,true,4,{sequence:1.5});
operations.push({kind:'leave',peer:'guest1'});join('replacement');
const lobby=new LobbyState(asset,total),expected=[];
for(const op of operations){let accepted=true;try{if(op.kind==='join')lobby.join(op.peer,op.name);else if(op.kind==='leave')lobby.leave(op.peer);else lobby.report(op.peer,op.report);}catch{accepted=false;}expected.push({accepted,snapshot:lobby.snapshot()});}
const run=spawnSync(process.argv[2],[],{input:JSON.stringify({asset,total,operations}),encoding:'utf8'});
assert.equal(run.status,0,run.stderr);assert.deepEqual(JSON.parse(run.stdout),expected);
console.log(`PASS native/browser lobby parity: ${operations.length} membership/progress/readiness/invalid-report transitions`);
