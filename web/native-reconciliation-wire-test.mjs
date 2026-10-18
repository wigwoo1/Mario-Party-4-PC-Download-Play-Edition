import test from 'node:test';
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createInterface} from 'node:readline';
import {resolve} from 'node:path';
import {reconcilePausedPoll} from './paused-poll-reconciliation.mjs';
import {agreePausedState} from './paused-state-agreement.mjs';
const executable=resolve('work/native-reconciliation-wire.exe');
const identity={session:'01'.repeat(16),buildId:'b'.repeat(64),assetId:'a'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,players:[{peer:'host',slot:0},{peer:'guest1',slot:1}]};
class Channel extends EventTarget{
 label='partyboard-recovery-v1';readyState='open';ordered=true;maxRetransmits=null;maxPacketLifeTime=null;
 constructor(child){super();this.child=child;}
 send(data){this.child.stdin.write(data+'\n');}
}
for(const ahead of [false,true])test(`C++ and browser reconcile ${ahead?'one-frame native lag':'matching polls'} then strictly agree`,{timeout:10000},async()=>{
 const child=spawn(executable,[],{stdio:['pipe','pipe','pipe']}),channel=new Channel(child);
 let errors='',result,timer;child.stderr.on('data',chunk=>{errors+=chunk;});
 const exited=new Promise((resolve,reject)=>{child.once('error',reject);child.once('exit',code=>resolve(code));});
 const lines=createInterface({input:child.stdout});
 lines.on('line',line=>{const packet=JSON.parse(line);if(packet.type==='fixture-complete')result=packet;
  else channel.dispatchEvent(new MessageEvent('message',{data:line}));});
 const frame=ahead?1n:0n,checkpoint=ahead?43:42;
 const captured={attempt:{peer:11,epoch:1n,revision:1n,frame,checkpoint,frand:3,rand8:5},
   state:{frame:Number(frame),checkpoint,frand:3,rand8:5},input:{state:'Paused',frame,epoch:1n,revision:1n}};
 const pads=new Uint8Array(44);pads[0]=7;pads[11]=42;pads[32]=pads[43]=255;
 const options={channel,identity,peer:'guest1',remotePeer:'host',nonce:'c'.repeat(32),capture:()=>captured,current:()=>true,
  bridge:{pendingState:()=>({}),completedState:()=>ahead?{frame:0n,checkpoint:42,frand:3,rand8:5,pads}:null,
   replayCompletedPoll(){throw Error('Ahead browser must remain paused');}},deadline:performance.now()+5000};
 try{
  const reconciled=reconcilePausedPoll(options);
  timer=setInterval(()=>channel.send(JSON.stringify({type:'fixture-tick'})),20);
  assert.equal((await reconciled).inputFrame,frame.toString());
  await agreePausedState(options);
  assert.equal(await exited,0,errors);assert.ok(result?.paused);assert.equal(result.frame,Number(frame));
  assert.equal(result.inputFrame,frame.toString());assert.ok(result.remainingMs>0&&result.remainingMs<=60000);
 }finally{clearInterval(timer);lines.close();child.stdin.destroy();if(child.exitCode===null)child.kill();await exited;}
});
