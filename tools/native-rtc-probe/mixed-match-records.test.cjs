'use strict';
const assert=require('node:assert/strict');
const {matchingRecords}=require('./drive-mixed-full-match.cjs');
const sample={frame:10,checkpoint:123,frand:4,rand8:5,pads:Array(44).fill(0)};
const peers=Array.from({length:4},(_,index)=>[{...sample,frame:index%2?'10':10}]);
assert.equal(matchingRecords(peers).length,1);
assert.equal(matchingRecords([...peers.slice(0,3),[]]).length,0);
for(const field of ['checkpoint','frand','rand8']){
 const bad=peers.map(records=>records.map(record=>({...record})));bad[3][0][field]++;
 assert.throws(()=>matchingRecords(bad),/disagreement/);
}
const bad=peers.map(records=>records.map(record=>({...record,pads:[...record.pads]})));bad[2][0].pads[11]=1;
assert.throws(()=>matchingRecords(bad),/disagreement/);
console.log('PASS mixed records: numeric/string frame alignment, missing peers, checkpoint/RNG/pad divergence');
