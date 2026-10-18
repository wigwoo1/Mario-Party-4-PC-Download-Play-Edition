'use strict';
// JSON does not include Error's stack, cause, or AggregateError's errors.
function errorEvidence(error, seen=new Set()) {
 if(error===null||typeof error!=='object')return {message:String(error)};
 if(seen.has(error))return {message:'[Repeated error reference]'};
 seen.add(error);
 const evidence={name:String(error.name||'Error'),message:String(error.message||error)};
 if(typeof error.stack==='string')evidence.stack=error.stack;
 if(error.cause!==undefined)evidence.cause=errorEvidence(error.cause,seen);
 if(Array.isArray(error.errors))evidence.errors=error.errors.map(item=>errorEvidence(item,seen));
 return evidence;
}
module.exports={errorEvidence};
