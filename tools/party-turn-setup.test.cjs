'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const {cursorKey,turnAdjustmentKey,turnLengths,setupMenuOwnerIndex,turnSetupAction,setupActionObserved,setupActionUnchanged,retryObservedSetupAction,partySetupKeys}=require('./party-turn-setup.cjs');
test('actual game source exposes five-turn mode and forwards the selected length',()=>{
 const source=fs.readFileSync(path.join(__dirname,'../src/REL/mentDll/main.c'),'utf8');
 assert.match(source,/#define PARTY_TURN_MIN 5\b/);
 assert.match(source,/#define PARTY_TURN_MAX 50\b/);
 assert.match(source,/#define PARTY_TURN_STEP 5\b/);
 assert.match(source,/#define PARTY_TURN_DEFAULT 20\b/);
 assert.match(source,/var_r19\s*=\s*lbl_1_bss_35BC\[1\]\.unk_1C\[1\]/);
 assert.match(source,/BoardPartyConfigSet\([^;]*var_r19/s);
});
test('browser gameplay driver confirms the Party board instead of returning to Party Mode',()=>{
 const source=fs.readFileSync(path.join(__dirname,'../src/REL/mentDll/main.c'),'utf8');
 const picker=source.slice(source.indexOf('Party board selection ready:'),source.indexOf('Party board selection ready:')+1500);
 assert.match(picker,/HuPadBtnDown\[lbl_1_bss_3114->unk_6C\]\s*&\s*0x200[\s\S]*?var_r29\s*=\s*0;/);
 assert.match(picker,/HuPadBtnDown\[lbl_1_bss_3114->unk_6C\]\s*&\s*0x100[\s\S]*?var_r29\s*=\s*2;/);
 const driver=fs.readFileSync(path.join(__dirname,'../web/lobby-runtime-peers-browser-test.cjs'),'utf8');
 assert.match(driver,/pressKeyboard\(pages\[owner\],'z',\{delay:120\}\);\s*\/\/ Board selector accepts A; B returns to Party Mode\./);
 assert.match(driver,/Before setup telemetry exists, connected players must confirm[\s\S]*?await Promise\.all\(pages\.map\(page=>pressKeyboard\(page,'z',\{delay:120\}\)\)\);/);
 assert.match(driver,/for\(const page of pages\)await page\.locator\('#canvas'\)\.click\(\);\s*\/\/ Click the player-1 page last[\s\S]*?await host\.locator\('#canvas'\)\.click\(\);/);
 assert.match(driver,/retryObservedSetupAction\(\{owner:decision\.owner,row,turns,port:decision\.port,key,kind:'cursor'\}/);
 assert.match(driver,/text\.includes\('Party flow '\)[\s\S]*?text\.includes\('Party setup confirmation '\)/);
});
test('turn selector navigation reaches each legal party length through the shortest direction',()=>{
 assert.deepEqual(turnLengths,[5,10,15,20,25,30,35,40,45,50]);
 assert.equal(turnAdjustmentKey(20,5),'a');
 assert.equal(turnAdjustmentKey(20,25),'d');
 assert.equal(turnAdjustmentKey(50,5),'d');
 assert.equal(turnAdjustmentKey(5,50),'a');
 assert.equal(turnAdjustmentKey(20,20),null);
});
test('party settings cursor advances and retreats with A/B while directions change the row value',()=>{
 assert.equal(cursorKey(0),'z');assert.equal(cursorKey(2),'x');assert.equal(cursorKey(4),'x');assert.equal(cursorKey(1),null);
});
test('turn selector navigation rejects values the actual game does not expose',()=>{
 for(const length of [0,4,21,55])assert.throws(()=>turnAdjustmentKey(20,length),RangeError);
 for(const row of [-1,5])assert.throws(()=>cursorKey(row),RangeError);
});
test('party setup presses input only on the peer that owns the reported port',()=>{
 const states=[0,1,2,3].map(peer=>({input:{localPadSlots:[0,1,2,3].map(port=>port===peer?peer:null)}}));
 for(let port=0;port<4;port++)assert.equal(setupMenuOwnerIndex(states,port),port);
 assert.throws(()=>setupMenuOwnerIndex(states,4),/exactly one local owner/);
});
test('actual setup action selects five- and ten-turn modes on the peer owning the menu controller',()=>{
 for(const target of [5,10]){
  const states=[0,1,2,3].map(peer=>({progress:{partySetupRow:0,partySetupTurns:20,partySetupPort:2},input:{localPadSlots:[0,1,2,3].map(port=>port===peer?peer:null)}}));
  assert.deepEqual(turnSetupAction(states,target),{owner:2,key:'z',kind:'cursor',selected:false,row:0,turns:20,port:2});
  for(const state of states)state.progress.partySetupRow=1;
  assert.deepEqual(turnSetupAction(states,target),{owner:2,key:'a',kind:'turns',selected:false,row:1,turns:20,port:2});
  for(const state of states)state.progress.partySetupTurns=target;
  assert.deepEqual(turnSetupAction(states,target),{owner:2,key:null,kind:'turns',selected:true,row:1,turns:target,port:2});
 }
});
test('Party setup action observes synchronized movement even when a held key repeats',()=>{
 const state=(row,turns,port=2)=>({partySetupRow:row,partySetupTurns:turns,partySetupPort:port});
 assert.equal(setupActionObserved({row:0,turns:20,port:2,key:'z'},[state(1,20),state(2,20),state(1,20),state(1,20)]),false);
 assert.equal(setupActionObserved({row:0,turns:20,port:2,key:'z'},[state(1,20),state(1,20),state(1,20),state(1,20)]),true);
 assert.equal(setupActionObserved({row:1,turns:20,port:2,key:'a'},[state(1,15),state(1,10),state(1,10),state(1,10)]),false);
 assert.equal(setupActionObserved({row:1,turns:20,port:2,key:'a'},[state(1,10),state(1,10),state(1,10),state(1,10)]),true);
 assert.equal(setupActionObserved({row:1,turns:20,port:2,key:'d'},[state(1,25),state(1,25),state(1,25),state(1,25)]),true);
 assert.equal(setupActionObserved({row:1,turns:20,port:2,key:'a'},[state(1,25),state(1,25),state(1,25),state(1,25)]),false);
 assert.equal(setupActionObserved({row:4,turns:5,port:0,key:'x'},[state(3,5,0),state(2,5,0),state(2,5,0),state(2,5,0)]),false);
 assert.equal(setupActionObserved({row:4,turns:5,port:0,key:'x'},[state(2,5,0),state(2,5,0),state(2,5,0),state(2,5,0)]),true);
 // Row 1 is also the starting position for vertical cursor navigation after
 // confirming the turn count, so action kind must disambiguate it explicitly.
 assert.equal(setupActionObserved({row:1,turns:5,port:0,key:'z',kind:'cursor'},[state(2,5,0),state(2,5,0)]),true);
 assert.throws(()=>setupActionObserved({row:1,turns:20,port:2,key:'z'},[state(1,20),state(1,20)]),/horizontally/);
});
test('a lost Party setup key retries only while every peer still reports the original state',async()=>{
 const decision={owner:2,key:'z',selected:false,row:2,turns:5,port:0},states=[0,1,2,3].map(()=>({partySetupRow:2,partySetupTurns:5,partySetupPort:0}));
 let presses=0,waits=0;const attempts=[];
 const result=await retryObservedSetupAction(decision,{press:async(owner,key)=>{assert.equal(owner,2);assert.equal(key,'z');presses++;},
  waitForChange:async()=>{if(presses===2){states.forEach(state=>state.partySetupRow=3);return true;}return false;},
  readStates:async()=>states,waitFrames:async observed=>{assert.equal(observed,states);waits++;},onAttempt:entry=>attempts.push(entry)});
 assert.equal(result,states);assert.equal(presses,2);assert.equal(waits,2);assert.deepEqual(attempts.map(entry=>entry.changed),[false,true]);
 assert.equal(setupActionUnchanged(decision,states),false);
});
test('a late observed Party setup key is not repeated after the post-input settle window',async()=>{
 const decision={owner:0,key:'z',selected:false,row:2,turns:5,port:0},states=[0,1].map(()=>({partySetupRow:2,partySetupTurns:5,partySetupPort:0}));
 let presses=0,waits=0;
 const result=await retryObservedSetupAction(decision,{press:async()=>{presses++;},waitForChange:async()=>false,readStates:async()=>states,
  waitFrames:async()=>{if(++waits===1)states.forEach(state=>state.partySetupRow=3);}});
 assert.equal(result,states);assert.equal(presses,1);assert.equal(waits,1);
});
test('Party setup retry stops on peer disagreement and after the bounded attempt count',async()=>{
 const decision={owner:0,key:'x',selected:false,row:2,turns:5,port:0},states=[0,1].map(()=>({partySetupRow:2,partySetupTurns:5,partySetupPort:0}));
 await assert.rejects(retryObservedSetupAction(decision,{press:async()=>{},waitForChange:async()=>false,
  readStates:async()=>[states[0],{...states[1],partySetupRow:1}],waitFrames:async()=>{}}),/inconsistent state/);
 let presses=0;await assert.rejects(retryObservedSetupAction(decision,{press:async()=>{presses++;},waitForChange:async()=>false,
  readStates:async()=>states,waitFrames:async()=>{},maxAttempts:2}),/not observed after 2 safe attempts/);assert.equal(presses,2);
});
test('turn setup telemetry supports two-peer diagnostic runs as well as four-peer matches',()=>{
 const states=[0,1].map(peer=>({progress:{partySetupRow:1,partySetupTurns:20,partySetupPort:0},input:{localPadSlots:[peer===0?0:null,null,null,null]}}));
 assert.deepEqual(turnSetupAction(states,5),{owner:0,key:'a',kind:'turns',selected:false,row:1,turns:20,port:0});
});
test('setup action waits until all peers expose matching live selector telemetry',()=>{
 const states=Array.from({length:4},()=>({progress:{partySetupRow:-1,partySetupTurns:-1,partySetupPort:-1}}));
 assert.equal(turnSetupAction(states,5),null);
 states.forEach(state=>Object.assign(state.progress,{partySetupRow:1,partySetupTurns:10,partySetupPort:0}));
 states[3].progress.partySetupTurns=15;
 assert.throws(()=>turnSetupAction(states,5),/disagrees across peers/);
});
test('actual game setup sequence selects five- and ten-turn modes and preserves the other settings',()=>{
 assert.deepEqual(partySetupKeys(20,5),['z','a','a','a','z','z','z','z']);
 assert.deepEqual(partySetupKeys(20,10),['z','a','a','z','z','z','z']);
 assert.deepEqual(partySetupKeys(20,25),['z','d','z','z','z','z']);
 assert.deepEqual(partySetupKeys(20,50),['z','a','a','a','a','z','z','z','z']);
 assert.deepEqual(partySetupKeys(20,20),['z','z','z','z','z']);
});
