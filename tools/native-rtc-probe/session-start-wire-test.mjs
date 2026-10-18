import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {StartBarrier} from '../../web/start-barrier.mjs';
import {prepareStartAcknowledgment,resumeCommittedClientRuntime} from '../../web/start-client.mjs';
const executable=process.argv[2];assert.ok(executable);
const snapshot={assetId:'a'.repeat(64),total:50000,revision:5,membership:3,saveRequired:false,players:[{peer:'guest3',slot:3},{peer:'host',slot:0},{peer:'guest1',slot:1},{peer:'guest2',slot:2}].map(p=>({...p,ready:true,verified:true,shaders:true,bytes:50000}))};
const expected=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(0x11),seed:7,buildId:'b'.repeat(64),checkpoint:123}).proposal;
const invoke=(mode,input)=>{const r=spawnSync(executable,[mode],{input,encoding:'utf8',timeout:10000});assert.ifError(r.error);return r;};
const original=invoke('--proposal');assert.equal(original.status,0);assert.deepEqual(JSON.parse(original.stdout),expected);
const acks=[],preparedClients=[];for(const {peer} of expected.players){const prepared=await prepareStartAcknowledgment(expected,{peer,buildId:expected.buildId,snapshot:()=>snapshot,prepare:async()=>({checkpoint:123,sessionSeed:7,engine:{phase:'Idle',preparedCheckpoint:()=>123,resume:()=>true},dispose(){}})});acks.push(prepared.acknowledgment);preparedClients.push({peer,prepared});}
const accepted=invoke('--wire',acks.map(a=>JSON.stringify(a)).join('\n')+'\n');assert.equal(accepted.status,0,accepted.stderr);const commit=JSON.parse(accepted.stdout);assert.deepEqual(commit,{...expected,type:'session-start-commit'});for(const {peer,prepared} of preparedClients)assert.equal(resumeCommittedClientRuntime(commit,prepared,{peer,buildId:expected.buildId,snapshot:()=>snapshot}),true);
const tampered=structuredClone(acks);tampered[2].checkpoint++;assert.notEqual(invoke('--wire',tampered.map(a=>JSON.stringify(a)).join('\n')+'\n').status,0);
const membership=structuredClone(acks);membership[1].players[1].slot=3;assert.notEqual(invoke('--wire',membership.map(a=>JSON.stringify(a)).join('\n')+'\n').status,0);
console.log('PASS native start proposal / actual browser preparation acknowledgments / native all-player agreement; changed checkpoint and assignments rejected. Synthetic runtime/stdio fixture only.');

