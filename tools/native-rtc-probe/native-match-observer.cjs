'use strict';
function createNativeMatchObserver({send,timeoutMs=30000}){
 if(typeof send!=='function'||!Number.isInteger(timeoutMs)||timeoutMs<1||timeoutMs>120000)throw Error('Bounded native observer required');
 const pending=new Map();let sequence=0,closed=null;
 function fail(error){closed=error;for(const waiter of pending.values()){clearTimeout(waiter.timer);waiter.reject(error);}pending.clear();}
 return {
  observe(){
   if(closed)return Promise.reject(closed);
   const requestId='match-'+(++sequence);
   return new Promise((resolve,reject)=>{
    const waiter={resolve,reject,timer:setTimeout(()=>{pending.delete(requestId);reject(Error('Native match observation timed out: '+requestId));},timeoutMs)};
    pending.set(requestId,waiter);
    Promise.resolve().then(()=>send({type:'diagnostic-observe',requestId})).catch(error=>{
     if(pending.delete(requestId)){clearTimeout(waiter.timer);reject(error);}
    });
   });
  },receive(event){
   if(event?.type!=='full-match-observation')return false;
   const waiter=pending.get(event.requestId);if(!waiter)return false;
   pending.delete(event.requestId);clearTimeout(waiter.timer);
   if(!Number.isSafeInteger(event.frame)||event.frame<0||!Array.isArray(event.observations)||
      !['turn','maxTurn','board','player','overlay','event'].every(key=>Number.isInteger(event.progress?.[key])))
    waiter.reject(Error('Invalid native match observation'));
   else waiter.resolve(event);
   return true;
  },close(error=Error('Native match observer closed')){fail(error);}
 };
}
module.exports={createNativeMatchObserver};
