'use strict';
// Poll resolved browser results explicitly. Some installed Playwright versions
// treat an async waitForFunction predicate's Promise as immediately truthy.
async function waitForBrowserCondition(page,predicate,arg,{timeout=30000,interval=25}={}){
 if(!Number.isFinite(timeout)||timeout<1||!Number.isFinite(interval)||interval<1)throw Error('Positive condition timeout and interval required');
 const deadline=performance.now()+timeout;
 for(;;){
  const budget=deadline-performance.now();
  if(budget<=0)throw Error('Browser condition did not become true before its deadline');
  let timer,value;
  try{
   value=await Promise.race([page.evaluate(predicate,arg),new Promise((_,reject)=>{
    timer=setTimeout(()=>reject(Error('Browser condition did not become true before its deadline')),budget);
   })]);
  }finally{clearTimeout(timer);}
  if(value===true)return;
  if(value!==false&&value!==null&&value!==undefined)throw Error('Browser condition must resolve to a boolean');
  const remaining=deadline-performance.now();
  if(remaining<=0)throw Error('Browser condition did not become true before its deadline');
  await new Promise(resolve=>setTimeout(resolve,Math.min(interval,remaining)));
 }
}
module.exports={waitForBrowserCondition};
