import test from 'node:test';
import assert from 'node:assert/strict';
import {SessionInput} from './session-input.mjs';
import {createSessionInputExchange} from './session-input-exchange.mjs';
import {createSuspendedInputRecovery} from './suspended-input-recovery.mjs';
function fixture(){let time=0,disposed=0,menu=0,sent=0;const session=new Uint8Array(16).fill(1),queue=new SessionInput(session,[11,22,0,0]);
 const bridge=createSessionInputExchange(queue,{session,owners:[11,22,0,0],peer:11,checkpoint:()=>42,send:()=>sent++,suspendOnPause:true,now:()=>time});
 const wait=bridge.exchange(new Uint8Array(44));queue.disconnected(22,time);
 const recovery=createSuspendedInputRecovery(queue,bridge,{transportReady:()=>true,now:()=>time,dispose:async()=>{await wait;disposed++;},returnToMenu:async()=>menu++});
 return {queue,bridge,recovery,wait,time:value=>time=value,counts:()=>[disposed,menu],sent:()=>sent};}
test('altered RNG recovery tokens reject before verification',async()=>{
 const session=new Uint8Array(16).fill(1),queue=new SessionInput(session,[11,22,0,0]);
 const bridge=createSessionInputExchange(queue,{session,owners:[11,22,0,0],peer:11,checkpoint:()=>42,send:()=>{},suspendOnPause:true,now:()=>0});
 const wait=bridge.exchange(new Uint8Array(44),{frand:0xffffffff,rand8:7});queue.disconnected(22,0);
 const recovery=createSuspendedInputRecovery(queue,bridge,{transportReady:()=>true,now:()=>0,dispose:()=>wait,returnToMenu:()=>{}}),token=recovery.attempt(22);let verified=0;
 assert.equal(token.frand,0xffffffff);assert.equal(token.rand8,7);
 for(const field of ['frand','rand8'])assert.equal(await recovery.restore({...token,[field]:0},async()=>{verified++;return true;}),false);
 assert.equal(verified,0);await recovery.close();
});
test('only verified current suspended token permits resend; stale token cannot resume',async()=>{
 const f=fixture(),token=f.recovery.attempt(22);assert.equal(token.checkpoint,42);
 assert.equal(await f.recovery.restore(token,async()=>false),false);assert.equal(f.queue.status(0).state,'Paused');
 assert.equal(await f.recovery.restore({...token,checkpoint:43},async()=>true),false);
 assert.equal(await f.recovery.restore(token,async()=>true),true);assert.equal(f.queue.status(0).state,'Running');
 await f.recovery.close();assert.deepEqual(f.counts(),[1,1]);
});
test('verification crossing deadline releases wait and disposes once; late permission rejected',async()=>{
 const f=fixture(),token=f.recovery.attempt(22);f.time(59999);await f.recovery.tick();assert.deepEqual(f.counts(),[0,0]);
 assert.equal(await f.recovery.restore(token,async()=>{f.time(60000);return true;}),false);
 assert.equal(await f.wait,null);await f.recovery.tick();await f.recovery.close();assert.deepEqual(f.counts(),[1,1]);
});
test('a newer loss during verification invalidates the original recovery permission',async()=>{
 const f=fixture(),token=f.recovery.attempt(22);
 assert.equal(await f.recovery.restore(token,async()=>{f.queue.disconnected(22,0);return true;}),false);
 assert.equal(f.queue.status(0).state,'Paused');assert.notEqual(f.recovery.attempt(22).revision,token.revision);
 await f.recovery.close();assert.deepEqual(f.counts(),[1,1]);
});
test('verification failure crossing deadline releases the engine before propagating error',async()=>{
 const f=fixture(),token=f.recovery.attempt(22),failure=new Error('Replacement verification failed');
 f.time(59999);
 await assert.rejects(f.recovery.restore(token,async()=>{f.time(60000);throw failure;}),error=>error===failure);
 assert.equal(await f.wait,null);assert.equal(f.queue.status(60000).state,'ReturnToMenu');
 assert.deepEqual(f.counts(),[1,1]);await f.recovery.tick();await f.recovery.close();assert.deepEqual(f.counts(),[1,1]);
});
test('verification failure before deadline remains paused and permits a verified retry',async()=>{
 const f=fixture(),token=f.recovery.attempt(22),failure=new Error('Transient verification failure');
 await assert.rejects(f.recovery.restore(token,async()=>{throw failure;}),error=>error===failure);
 assert.equal(f.queue.status(0).state,'Paused');assert.deepEqual(f.counts(),[0,0]);
 assert.equal(await f.recovery.restore(token,async()=>true),true);await f.recovery.close();assert.deepEqual(f.counts(),[1,1]);
});
test('overlapping verification attempts resume and resend only once',async()=>{
 const f=fixture(),token=f.recovery.attempt(22);let release;
 const delayed=f.recovery.restore(token,()=>new Promise(resolve=>{release=resolve;}));
 assert.equal(await f.recovery.restore(token,async()=>true),true);
 release(true);assert.equal(await delayed,false);assert.equal(f.queue.status(0).state,'Running');assert.equal(f.sent(),2);
 await f.recovery.close();assert.deepEqual(f.counts(),[1,1]);
});
test('menu retry does not dispose an already disposed engine again',async()=>{
 const f=fixture();let disposed=0,menus=0;const failure=new Error('Menu temporarily unavailable');
 const recovery=createSuspendedInputRecovery(f.queue,f.bridge,{transportReady:()=>true,now:()=>60000,
  dispose:async()=>{await f.wait;disposed++;},returnToMenu:async()=>{if(++menus===1)throw failure;}});
 await assert.rejects(recovery.tick(),error=>error===failure);assert.equal(disposed,1);
 await recovery.tick();await recovery.close();assert.equal(disposed,1);assert.equal(menus,2);
});
test('replacement resend failure cancels input and completes terminal cleanup immediately',async()=>{
 const session=new Uint8Array(16).fill(1),queue=new SessionInput(session,[11,22,0,0]);
 const failure=new Error('Replacement channel send failed');let sends=0,disposed=0,menus=0;
 const bridge=createSessionInputExchange(queue,{session,owners:[11,22,0,0],peer:11,checkpoint:()=>42,
  send:()=>{if(++sends>1)throw failure;},suspendOnPause:true,now:()=>0});
 const wait=bridge.exchange(new Uint8Array(44));const rejected=assert.rejects(wait,error=>error===failure);
 queue.disconnected(22,0);
 const recovery=createSuspendedInputRecovery(queue,bridge,{transportReady:()=>true,now:()=>0,
  dispose:async()=>{await rejected;disposed++;},returnToMenu:async()=>menus++});
 await assert.rejects(recovery.restore(recovery.attempt(22),async()=>true),error=>error===failure);
 assert.equal(queue.status(0).state,'ReturnToMenu');assert.equal(queue.status(0).frame,0n);
 assert.equal(disposed,1);assert.equal(menus,1);await recovery.tick();await recovery.close();
 assert.equal(disposed,1);assert.equal(menus,1);
});
test('multiple missing peers and transport readiness gate synchronized resume',async()=>{
 const session=new Uint8Array(16).fill(1),queue=new SessionInput(session,[11,22,33,0]),ready=new Set();let sent=0;
 const bridge=createSessionInputExchange(queue,{session,owners:[11,22,33,0],peer:11,checkpoint:()=>42,send:()=>sent++,suspendOnPause:true,now:()=>0});
 const wait=bridge.exchange(new Uint8Array(44));queue.disconnected(22,0);queue.disconnected(33,0);
 const recovery=createSuspendedInputRecovery(queue,bridge,{transportReady:peer=>peer?ready.has(peer):[22,33].every(peer=>ready.has(peer)),now:()=>0,dispose:()=>wait,returnToMenu:()=>{}});
 const first=recovery.attempt(22);let verified=0;
 assert.equal(await recovery.restore(first,async()=>{verified++;return true;}),false);assert.equal(verified,0);
 ready.add(22);assert.equal(await recovery.restore(first,async()=>true),false);assert.equal(queue.status(0).state,'Paused');assert.equal(sent,1);
 ready.add(33);assert.equal(await recovery.restore(recovery.attempt(33),async()=>{ready.delete(33);return true;}),false);assert.equal(queue.status(0).state,'Paused');
 ready.add(33);assert.equal(await recovery.restore(recovery.attempt(33),async()=>true),true);assert.equal(sent,2);await recovery.close();
});
test('deadline tick settles a stalled verification and ignores its late success',{timeout:1000},async()=>{
 const f=fixture(),token=f.recovery.attempt(22);let release;
 const restored=f.recovery.restore(token,()=>new Promise(resolve=>{release=resolve;}));
 await Promise.resolve();f.time(60000);await f.recovery.tick();
 assert.equal(await restored,false);assert.equal(await f.wait,null);assert.deepEqual(f.counts(),[1,1]);
 release(true);await Promise.resolve();assert.equal(f.queue.status(60000).state,'ReturnToMenu');assert.equal(f.sent(),1);
 await f.recovery.close();assert.deepEqual(f.counts(),[1,1]);
});
test('explicit close settles stalled verification without waiting for the remote response',{timeout:1000},async()=>{
 const f=fixture(),restored=f.recovery.restore(f.recovery.attempt(22),()=>new Promise(()=>{}));
 await Promise.resolve();await f.recovery.close();assert.equal(await restored,false);assert.deepEqual(f.counts(),[1,1]);
});
test('synchronous connection loss during resend reports recovery incomplete',async()=>{
 const session=new Uint8Array(16).fill(1),queue=new SessionInput(session,[11,22,0,0]);let sends=0;
 const bridge=createSessionInputExchange(queue,{session,owners:[11,22,0,0],peer:11,checkpoint:()=>42,
  send:()=>{if(++sends===2)queue.disconnected(22,0);},suspendOnPause:true,now:()=>0});
 const wait=bridge.exchange(new Uint8Array(44));queue.disconnected(22,0);
 const recovery=createSuspendedInputRecovery(queue,bridge,{transportReady:()=>true,now:()=>0,dispose:()=>wait,returnToMenu:()=>{}});
 assert.equal(await recovery.restore(recovery.attempt(22),async()=>true),false);
 assert.equal(queue.status(0).state,'Paused');assert.equal(queue.status(0).frame,0n);assert.ok(bridge.pendingState());await recovery.close();
});
test('terminal cleanup aborts verifier network work while preserving expiry result',{timeout:1000},async()=>{
 for(const expired of [false,true]){
  const f=fixture();let signal,aborts=0;
  const restored=f.recovery.restore(f.recovery.attempt(22),(_token,options)=>{
   signal=options.signal;return new Promise((_resolve,reject)=>signal.addEventListener('abort',()=>{aborts++;reject(new Error('Verification aborted'));},{once:true}));
  });
  await Promise.resolve();assert.equal(signal.aborted,false);
  if(expired){f.time(60000);await f.recovery.tick();}else await f.recovery.close();
  assert.equal(await restored,false);assert.equal(signal.aborted,true);assert.equal(aborts,1);assert.deepEqual(f.counts(),[1,1]);
  await f.recovery.close();assert.equal(aborts,1);
 }
});
test('queued verification never starts after immediate close or a newer loss',async()=>{
 for(const closed of [false,true]){
  const f=fixture();let verified=0;
  const restored=f.recovery.restore(f.recovery.attempt(22),async()=>{verified++;return true;});
  if(closed)await f.recovery.close();else f.queue.disconnected(22,0);
  assert.equal(await restored,false);assert.equal(verified,0);
  if(!closed)assert.equal(f.queue.status(0).state,'Paused');await f.recovery.close();assert.deepEqual(f.counts(),[1,1]);
 }
});
test('restore called with expired token performs terminal cleanup without verification',async()=>{
 const f=fixture(),token=f.recovery.attempt(22);let verified=0;f.time(60000);
 assert.equal(await f.recovery.restore(token,async()=>{verified++;return true;}),false);
 assert.equal(verified,0);assert.equal(await f.wait,null);assert.deepEqual(f.counts(),[1,1]);await f.recovery.close();
});
test('deadline crossed during final transport gate completes cleanup immediately',async()=>{
 const session=new Uint8Array(16).fill(1),queue=new SessionInput(session,[11,22,0,0]);let time=0,disposed=0,menus=0;
 const bridge=createSessionInputExchange(queue,{session,owners:[11,22,0,0],peer:11,checkpoint:()=>42,send:()=>{},suspendOnPause:true,now:()=>time});
 const wait=bridge.exchange(new Uint8Array(44));queue.disconnected(22,0);
 const recovery=createSuspendedInputRecovery(queue,bridge,{now:()=>time,transportReady:peer=>{if(peer===undefined){time=60000;return false;}return true;},
  dispose:async()=>{await wait;disposed++;},returnToMenu:async()=>menus++});
 assert.equal(await recovery.restore(recovery.attempt(22),async()=>true),false);
 assert.equal(await wait,null);assert.equal(disposed,1);assert.equal(menus,1);await recovery.close();assert.equal(disposed,1);
});
