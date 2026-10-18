'use strict';
// Review retained records independently of the live match driver.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const directory=process.argv[2];
assert.ok(directory,'Retained match directory required');
const checks=[];
for(const name of fs.readdirSync(directory).filter(name=>/^browser-full-match-turn-inputs-\d+-\d+-\d+-\d+\.json$/.test(name))){
 const bytes=fs.readFileSync(path.join(directory,name)),saved=JSON.parse(bytes);
 assert.equal(saved.peers.length,4,name+': four peers required');
 const maps=saved.peers.map(records=>{
  assert.equal(records.length,30,name+': exactly thirty retained records required');
  const map=new Map(records.map(record=>[record.frame,record]));
  assert.equal(map.size,30,name+': duplicate frames rejected');
  for(const record of records){
   assert.match(record.frame,/^\d+$/);
   for(const key of ['checkpoint','frand','rand8'])assert.ok(Number.isInteger(record[key]),name+': missing '+key);
   assert.ok(Array.isArray(record.pads)&&record.pads.length===44,name+': complete pad bytes required');
  }
  return map;
 });
 for(const map of maps.slice(1))assert.deepEqual(map,maps[0],name+': complete peer records differ');
 const frames=[...maps[0].keys()].sort((a,b)=>Number(a)-Number(b));
 checks.push({turns:saved.turns,source:name,sha256:crypto.createHash('sha256').update(bytes).digest('hex'),matchingRecords:30,firstPoll:frames[0],lastPoll:frames.at(-1)});
}
assert.ok(checks.length,'No retained ordinary turn records');
checks.sort((a,b)=>a.turns[0]-b.turns[0]);
fs.writeFileSync(path.join(directory,'browser-independent-turn-record-checks.json'),JSON.stringify({scope:'Independent comparison of saved complete input/checkpoint records; full match acceptance is separate',turnChecks:checks},null,2)+'\n');
console.log('PASS saved complete records across four peers; observed turns '+checks.map(check=>check.turns.join('/')).join(', '));
