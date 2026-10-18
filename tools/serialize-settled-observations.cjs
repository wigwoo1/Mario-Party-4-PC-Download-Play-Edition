'use strict';
// Error properties are non-enumerable; retain them explicitly in failure artifacts.
function serializeSettledObservations(results){
 return results.map(result=>result.status==='rejected'
  ?{status:result.status,reason:{name:result.reason?.name,message:String(result.reason?.message??result.reason),stack:result.reason?.stack}}
  :result);
}
module.exports={serializeSettledObservations};
