'use strict';
const {test}=require('node:test'),assert=require('node:assert/strict');
const {STOP_SAMPLE,parseStateSnapshot,hashWords,snapshotHashes,compareStopSnapshots,createStateSnapshotRecorder,waitForStopSnapshots}=require('./state-snapshot-recorder.cjs');
const state=()=>({sample:STOP_SAMPLE,frame:90108,vCounter:90108,boardSeed:42,overlay:-1,event:1,status:0,system:[5,5,0,3,9,0],players:Array.from({length:4},(_,i)=>[i,i,1,0,i,2,3,42,2,0,-1,-1,-1])});
const globals=()=>({sample:STOP_SAMPLE,frame:90108,fields:[['0x50424701u',1346520833],['system->turn',5],['system->board_data[i]',0],['player->coins',42]]});
const line=(kind,value)=>(kind==='state'?'State snapshot: ':'Global state snapshot: ')+JSON.stringify(value);
const rows=(peer,s=state(),g=globals())=>[{peer,kind:'state',data:s},{peer,kind:'globals',data:g}];
test('complete snapshots survive long console lines with explicit semantic fields and signed values',()=>{
 const g=globals();g.fields=Array.from({length:308},(_,i)=>['system->flag[i][j]',i]);
 assert.deepEqual(parseStateSnapshot(line('globals',g)),{kind:'globals',data:g});
 assert.deepEqual(parseStateSnapshot(line('state',state())),{kind:'state',data:state()});
});
test('arbitrary console data, malformed snapshots and nonnumeric payloads are excluded',()=>{
 for(const value of ['https://host/#room=secret','Lobby link: secret','State snapshot: {bad',line('state',{...state(),frame:NaN}),line('state',{...state(),players:[]}),line('globals',{...globals(),fields:[['https://secret',1]]}),line('globals',{...globals(),fields:[['system->turn','secret']]})])assert.equal(parseStateSnapshot(value),null);
 const s={...state(),capability:'secret'},g={...globals(),url:'private'};
 assert.equal(JSON.stringify(parseStateSnapshot(line('state',s))).includes('secret'),false);
 assert.equal(JSON.stringify(parseStateSnapshot(line('globals',g))).includes('private'),false);
});
test('FNV words use little-endian modulo-32 arithmetic including negative sentinels',()=>{
 const bytes=Buffer.alloc(12);[0xffffffff,42,90108].forEach((v,i)=>bytes.writeUInt32LE(v,i*4));
 let hash=2166136261n;for(const byte of bytes)hash=((hash^BigInt(byte))*16777619n)&0xffffffffn;
 assert.equal(hashWords([-1,42,90108]),Number(hash));
 const s=state(),g=globals(),actual=snapshotHashes(s,g);
 assert.equal(actual.globals,hashWords(g.fields.map(row=>row[1]),actual.legacy));
 assert.throws(()=>snapshotHashes(s,{...g,frame:90109}),/One snapshot/);
});
test('recorder retains every peer before teardown and remains independent of caller mutation',()=>{
 const journal=[],recorder=createStateSnapshotRecorder({write:text=>journal.push(text)});
 for(const peer of ['native','browser-0','browser-1','browser-2']){
  const s=state();assert.equal(recorder.receive(peer,line('state',s)),true);s.frame++;
  assert.equal(recorder.receive(peer,line('globals',globals())),true);
 }
 assert.equal(journal.length,8);assert.equal(recorder.summary(['native','browser-0','browser-1','browser-2']).comparison.status,'equal-covered-fields');
 assert.throws(()=>recorder.receive('https://private',line('state',state())),/Known test peer/);
});
test('comparison finds exact changed field even when repeated diagnostic names are identical',()=>{
 const g=globals();g.fields.push(['system->board_data[i]',1]);const other=structuredClone(g);other.fields[4][1]=2;
 const result=compareStopSnapshots([...rows('native',state(),g),...rows('browser-0',state(),other)],['native','browser-0']);
 assert.equal(result.status,'different');assert.equal(result.differences.length,1);assert.equal(result.differences[0].index,4);
 assert.equal(result.hashes[0].legacy,result.hashes[1].legacy);assert.notEqual(result.hashes[0].globals,result.hashes[1].globals);
});
test('missing or unequal-frame snapshots never produce an equality claim',()=>{
 assert.equal(compareStopSnapshots(rows('native'),['native','browser-0']).status,'incomplete');
 const later=state();later.frame++;const g=globals();g.frame++;
 assert.equal(compareStopSnapshots([...rows('native'),...rows('browser-0',later,g)],['native','browser-0']).status,'different-frames');
 const schema=globals();schema.fields.pop();assert.equal(compareStopSnapshots([...rows('native'),...rows('browser-0',state(),schema)],['native','browser-0']).status,'different');
});
test('latest paired stop is selected and prepared snapshots do not masquerade as stop evidence',()=>{
 const prepared=state();prepared.sample=0xfffffffe;const initial=globals();initial.sample=prepared.sample;
 const records=[...rows('native'),...rows('browser-0',prepared,initial)];
 assert.equal(compareStopSnapshots(records,['native','browser-0']).status,'incomplete');
 const later=state();later.frame++;const laterGlobals=globals();laterGlobals.frame++;
 records.push(...rows('native',later,laterGlobals),...rows('browser-0',later,laterGlobals));
 assert.equal(compareStopSnapshots(records,['native','browser-0']).frame,90109);
});
test('a peer stopping one frame earlier does not hide a comparable failing pair',()=>{
 const early=state();early.frame--;const eg=globals();eg.frame--;
 const changed=state();changed.overlay=78;
 const result=compareStopSnapshots([...rows('native'),...rows('browser-0',early,eg),...rows('browser-1',changed)],['native','browser-0','browser-1']);
 assert.equal(result.status,'different-frames');assert.equal(result.comparisons.length,1);
 assert.deepEqual(result.comparisons[0].peers,['native','browser-1']);assert.equal(result.comparisons[0].status,'different');
 assert.equal(result.comparisons[0].differences[0].field,'overlay');
});
test('diagnostic storage is bounded without truncating an accepted snapshot',()=>{
 const journal=[],recorder=createStateSnapshotRecorder({write:text=>journal.push(text)});
 for(let i=0;i<70;i++)recorder.receive('native',line('state',state()));
 assert.equal(journal.length,64);assert.equal(recorder.summary(['native']).discarded,6);
 assert.equal(parseStateSnapshot('State snapshot: '+' '.repeat(131073)),null);
});
test('context teardown waits for the delayed second console message, not just the Stop click',async()=>{
 const recorder=createStateSnapshotRecorder({write:()=>{}});let time=0;
 recorder.receive('browser-0',line('state',state()));
 const drained=await waitForStopSnapshots(recorder,['browser-0'],{now:()=>time,timeoutMs:100,pause:async ms=>{time+=ms;recorder.receive('browser-0',line('globals',globals()));}});
 assert.equal(drained.complete,true);assert.equal(drained.comparison.status,'equal-covered-fields');assert.equal(time,25);
});
test('a missing stop does not hang cleanup or count as captured evidence',async()=>{
 const recorder=createStateSnapshotRecorder({write:()=>{}});let time=0;
 const drained=await waitForStopSnapshots(recorder,['native'],{now:()=>time,timeoutMs:50,pause:async ms=>{time+=ms;}});
 assert.equal(drained.complete,false);assert.equal(drained.comparison.status,'incomplete');assert.equal(time,50);
 await assert.rejects(waitForStopSnapshots(recorder,['native'],{timeoutMs:Infinity}),/Bounded snapshot drain/);
});
