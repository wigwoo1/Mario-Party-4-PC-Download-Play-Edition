'use strict';
const assert=require('node:assert/strict');
const {createNativeMatchKeyboard}=require('./native-match-keyboard.cjs');
(async()=>{
 const sent=[],keyboard=createNativeMatchKeyboard(packet=>sent.push(packet),{sleep:async()=>{}});
 await keyboard.down('w');await keyboard.down('d');await keyboard.press('z');
 assert.equal(sent.at(-2).buttons,256);assert.equal(sent.at(-2).stickX,127);assert.equal(sent.at(-2).stickY,127);
 assert.equal(sent.at(-1).buttons,0);assert.equal(sent.at(-1).stickX,127);
 await keyboard.down('a');assert.equal(sent.at(-1).stickX,0);
 await keyboard.releaseAll();assert.equal(sent.at(-1).stickX,0);assert.equal(sent.at(-1).stickY,0);
 await keyboard.down('e');await keyboard.down('q');await keyboard.down('l');await keyboard.down('i');
 assert.equal(sent.at(-1).buttons,96);assert.equal(sent.at(-1).triggerLeft,255);assert.equal(sent.at(-1).triggerRight,255);
 assert.equal(sent.at(-1).substickX,127);assert.equal(sent.at(-1).substickY,127);
 await keyboard.releaseAll();assert.equal(sent.at(-1).triggerRight,0);
 await assert.rejects(keyboard.press('unsupported'));await assert.rejects(keyboard.press('z',{delay:Infinity}));
 let failed=false;const recovery=[];
 const broken=createNativeMatchKeyboard(packet=>recovery.push(packet),{sleep:async()=>{throw Error('interrupted');}});
 try{await broken.press('Enter');}catch{failed=true;}
 assert(failed);assert.equal(recovery[0].buttons,4096);assert.equal(recovery[1].buttons,0);
 console.log('PASS native match keyboard: chords, opposite directions, button release and interrupted press cleanup');
})().catch(error=>{console.error(error);process.exitCode=1;});
