'use strict';
// Consume only the existing read-only engine diagnostics. Never retain arbitrary
// console text: it can contain lobby capabilities, URLs or user file names.
const STOP_SAMPLE=0xfffffffd;
const u32=value=>Number.isInteger(value)&&value>=0&&value<=0xffffffff;
const word=value=>Number.isInteger(value)&&value>=-0x80000000&&value<=0xffffffff;
const words=(value,count)=>Array.isArray(value)&&value.length===count&&value.every(word);
function parseStateSnapshot(line){
 if(typeof line!=='string'||line.length>131072)return null;
 for(const [prefix,kind]of [['State snapshot: ','state'],['Global state snapshot: ','globals']]){
  // The longer prefix contains the shorter one with different capitalization.
  const index=line.indexOf(prefix);if(index<0)continue;
  let data;try{data=JSON.parse(line.slice(index+prefix.length));}catch{return null;}
  if(!data||!u32(data.sample)||!u32(data.frame))return null;
  if(kind==='state'){
   if(!word(data.vCounter)||!u32(data.boardSeed)||!word(data.overlay)||!word(data.event)||!word(data.status)||
      !words(data.system,6)||!Array.isArray(data.players)||data.players.length!==4||!data.players.every(row=>words(row,13)))return null;
   return {kind,data:{sample:data.sample,frame:data.frame,vCounter:data.vCounter,boardSeed:data.boardSeed,
    overlay:data.overlay,event:data.event,status:data.status,system:data.system,players:data.players}};
  }
  if(!Array.isArray(data.fields)||!data.fields.length||data.fields.length>1024||!data.fields.every(row=>
   Array.isArray(row)&&row.length===2&&typeof row[0]==='string'&&/^[A-Za-z0-9_>\-\[\].]{1,96}$/.test(row[0])&&u32(row[1])))return null;
  return {kind,data:{sample:data.sample,frame:data.frame,fields:data.fields}};
 }
 return null;
}
function hashWords(values,initial=2166136261){
 let hash=initial>>>0;
 for(let value of values){value>>>=0;for(let byte=0;byte<4;byte++){hash=Math.imul(hash^(value&255),16777619)>>>0;value>>>=8;}}
 return hash;
}
function snapshotHashes(state,globals){
 if(state.frame!==globals.frame||state.sample!==globals.sample)throw Error('One snapshot frame/sample required');
 const legacy=hashWords([state.frame,state.vCounter,state.boardSeed,state.overlay,state.event,state.status,...state.system,...state.players.flat()]);
 return {legacy,globals:hashWords(globals.fields.map(row=>row[1]),legacy)};
}
function compareStopSnapshots(records,peers){
 const selected=peers.map(peer=>{
  const rows=records.filter(row=>row.peer===peer&&row.data.sample===STOP_SAMPLE);
  const state=rows.findLast(row=>row.kind==='state')?.data;
  const globals=rows.findLast(row=>row.kind==='globals'&&row.data.frame===state?.frame)?.data;
  return {peer,state,globals};
 });
 const missing=selected.filter(row=>!row.state||!row.globals).map(row=>row.peer);
 if(missing.length)return {status:'incomplete',missing,scope:'Stop snapshots only; no synchronization acceptance'};
 const frames=selected.map(row=>row.state.frame);
 const hashes=selected.map(row=>({peer:row.peer,frame:row.state.frame,...snapshotHashes(row.state,row.globals)}));
 if(!frames.every(frame=>frame===frames[0])){
  const comparableGroups=[...new Set(frames)].map(frame=>selected.filter(row=>row.state.frame===frame).map(row=>row.peer)).filter(group=>group.length>=2);
  return {status:'different-frames',frames,hashes,comparisons:comparableGroups.map(group=>({peers:group,...compareStopSnapshots(records,group)})),scope:'Only peers at the identical simulation frame are compared; this is not a four-peer synchronization pass'};
 }
 const baseline=selected[0],differences=[];
 for(const row of selected.slice(1)){
  for(const key of ['vCounter','boardSeed','overlay','event','status','system','players']){
   if(JSON.stringify(row.state[key])!==JSON.stringify(baseline.state[key]))differences.push({peer:row.peer,kind:'state',field:key,baseline:baseline.state[key],actual:row.state[key]});
  }
  const a=baseline.globals.fields,b=row.globals.fields;
  if(a.length!==b.length)differences.push({peer:row.peer,kind:'globals',field:'fieldCount',baseline:a.length,actual:b.length});
  for(let index=0;index<Math.max(a.length,b.length);index++){
   if(JSON.stringify(a[index])!==JSON.stringify(b[index]))differences.push({peer:row.peer,kind:'globals',index,baseline:a[index],actual:b[index]});
  }
 }
 return {status:differences.length?'different':'equal-covered-fields',frame:frames[0],hashes,differences,
  scope:'Legacy and explicit global fields only. GameStat, RNG, overlay objects and heap are not captured by these snapshots.'};
}
function createStateSnapshotRecorder({write}){
 if(typeof write!=='function')throw Error('Snapshot sink required');
 const records=[],counts=new Map();let discarded=0;
 return {
  receive(peer,line){
   if(!/^(native|browser-[0-2])$/.test(peer))throw Error('Known test peer required');
   const parsed=parseStateSnapshot(line);if(!parsed)return false;
   const count=counts.get(peer)||0;if(count>=64){discarded++;return false;}
   const record=JSON.parse(JSON.stringify({version:1,peer,...parsed}));
   write(JSON.stringify(record)+'\n');records.push(record);counts.set(peer,count+1);return true;
  },
  summary(peers){return {counts:Object.fromEntries(counts),discarded,comparison:compareStopSnapshots(records,peers)};}
 };
}
async function waitForStopSnapshots(recorder,peers,{timeoutMs=5000,pause=ms=>new Promise(resolve=>setTimeout(resolve,ms)),now=Date.now}={}){
 if(!Number.isInteger(timeoutMs)||timeoutMs<1||timeoutMs>10000)throw Error('Bounded snapshot drain required');
 const deadline=now()+timeoutMs;
 for(;;){
  const result=recorder.summary(peers);
  if(result.comparison.status!=='incomplete')return {complete:true,...result};
  if(now()>=deadline)return {complete:false,...result};
  await pause(25);
 }
}
module.exports={STOP_SAMPLE,parseStateSnapshot,hashWords,snapshotHashes,compareStopSnapshots,createStateSnapshotRecorder,waitForStopSnapshots};
