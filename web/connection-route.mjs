const knownDirectTypes=new Set(['host','srflx','prflx']);
export function classifyConnectionRoute(state,stats){
 if(state==='failed')return 'unreachable';
 if(state==='closed')return 'closed';
 if(state==='disconnected')return 'interrupted';
 if(state==='new'||state==='connecting')return 'connecting';
 if(state!=='connected'||!stats?.get||!stats?.values)return 'unknown';
 const reports=[...stats.values()];
 const selectedIds=new Set(reports.filter(report=>report.type==='transport'&&report.selectedCandidatePairId).map(report=>report.selectedCandidatePairId));
 let pair;
 if(selectedIds.size===1)pair=stats.get([...selectedIds][0]);
 else if(selectedIds.size===0){
  const candidates=reports.filter(report=>report.type==='candidate-pair'&&report.state==='succeeded'&&(report.selected===true||report.nominated===true));
  if(candidates.length===1)pair=candidates[0];
 }
 if(pair?.type!=='candidate-pair'||pair.state!=='succeeded')return 'unknown';
 const local=stats.get(pair.localCandidateId)?.candidateType,remote=stats.get(pair.remoteCandidateId)?.candidateType;
 if(local==='relay'||remote==='relay')return 'relayed';
 return knownDirectTypes.has(local)&&knownDirectTypes.has(remote)?'direct':'unknown';
}
export async function readConnectionRoute(connection){
 if(connection.connectionState!=='connected')return classifyConnectionRoute(connection.connectionState);
 try{const stats=await connection.getStats();return classifyConnectionRoute(connection.connectionState,stats);}
 catch{return classifyConnectionRoute(connection.connectionState);}
}
export function connectionRouteLabel(route){
 return ({connecting:'Connecting',direct:'Direct',relayed:'Relayed',interrupted:'Interrupted',unreachable:'Unreachable',closed:'Closed'})[route]||'Unknown';
}
// Statistics are local UI observations. Never publish results for a retired
// connection, including a replacement which reuses the same player name.
export function monitorConnectionRoutes({current,changed,interval=1000,schedule=setTimeout,cancel=clearTimeout}){
 let closed=false,inflight=false,timer;
 const refresh=async()=>{
  if(closed||inflight)return;
  if(timer!==undefined){cancel(timer);timer=undefined;}
  inflight=true;
  try{
   const entries=current();
   const routes=await Promise.all(entries.map(async entry=>({peer:entry.peer,label:entry.label,route:await readConnectionRoute(entry.connection)})));
   const latest=current();
   if(!closed&&latest.length===entries.length&&entries.every((entry,index)=>entry.peer===latest[index].peer&&entry.label===latest[index].label&&entry.connection===latest[index].connection))changed(routes);
  }finally{inflight=false;if(!closed)timer=schedule(refresh,interval);}
 };
 void refresh();
 return {refresh,close(){closed=true;if(timer!==undefined)cancel(timer);}};
}
