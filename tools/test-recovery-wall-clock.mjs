import assert from 'node:assert/strict';
import {SessionInput} from '../web/session-input.mjs';
import {createSessionInputExchange} from '../web/session-input-exchange.mjs';
import {createRealtimeInputRecovery} from '../web/realtime-input-recovery.mjs';

// Real monotonic deadline adapter test. No game, RTC, or menu UI claim.
const session=new Uint8Array(16).fill(1),owners=[11,22,0,0];
const queue=new SessionInput(session,owners),events=[];
const bridge=createSessionInputExchange(queue,{session,owners,peer:11,checkpoint:()=>42,
  send:()=>{},suspendOnPause:true});
const pending=bridge.exchange(new Uint8Array(44));
const started=performance.now();queue.disconnected(22,started);
let returned;const menu=new Promise(resolve=>{returned=resolve;});
const recovery=createRealtimeInputRecovery(queue,bridge,{
  channels:{close(){events.push('channels');queue.cancel();}},
  dispose:async()=>{assert.equal(await pending,null);events.push('dispose');},
  returnToMenu(){events.push('menu');returned();},transportReady:()=>false,
  error:cause=>{console.error(cause);process.exitCode=1;},
});
const watchdog=setTimeout(()=>{console.error('FAIL: deadline did not clean up within 65 seconds');process.exit(1);},65000);
try {
  await new Promise(resolve=>setTimeout(resolve,1000));
  assert.equal(queue.status(performance.now()).state,'Paused');assert.deepEqual(events,[]);
  assert.equal(bridge.pendingState().frame,0n);
  await menu;const elapsedMs=performance.now()-started;
  assert.ok(elapsedMs>=60000&&elapsedMs<65000,`Unexpected cleanup time ${elapsedMs}`);
  assert.deepEqual(events,['channels','dispose','menu']);assert.equal(queue.status(performance.now()).frame,0n);
  await recovery.close();assert.deepEqual(events,['channels','dispose','menu']);
  console.log(JSON.stringify({passed:true,elapsedMs,events,scope:'Real wall-clock adapter timeout; no game engine, RTC or production menu.'}));
} finally {clearTimeout(watchdog);await recovery.close();}
