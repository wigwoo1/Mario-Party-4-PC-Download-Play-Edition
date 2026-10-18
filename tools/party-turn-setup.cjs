'use strict';
const turnLengths=Array.from({length:10},(_,index)=>(index+1)*5);
function validateLength(turns){
 if(!turnLengths.includes(turns))throw new RangeError('Party length must be 5 through 50 in five-turn steps');
}
function cursorKey(row,target=1){
 if(!Number.isInteger(row)||row<0||row>4||!Number.isInteger(target)||target<0||target>4)
  throw new RangeError('Party setup row must be between zero and four');
 // This menu advances its row with A and retreats with B; the directional
 // stick/pad changes the value on the selected row instead.
 if(row<target)return 'z';
 if(row>target)return 'x';
 return null;
}
function turnAdjustmentKey(current,target){
 validateLength(current);validateLength(target);
 const from=turnLengths.indexOf(current),to=turnLengths.indexOf(target);
 const right=(to-from+turnLengths.length)%turnLengths.length;
 const left=(from-to+turnLengths.length)%turnLengths.length;
 if(right===0)return null;
 // The browser's default keyboard maps A/D to horizontal stick movement, which
 // this game menu reads alongside its digital D-pad input.
 return right<left?'d':'a';
}
function setupMenuOwnerIndex(states,port){
 const owners=states.flatMap((state,index)=>{
  const binding=state.input?.localPadSlots?.[port];
  return Number.isInteger(binding)&&binding>=0&&binding<4?[index]:[];
 });
 if(owners.length!==1)throw Error('Party setup input port must have exactly one local owner');
 return owners[0];
}
function turnSetupAction(states,target){
 validateLength(target);
 if(!Array.isArray(states)||states.length<2||states.length>4)throw new TypeError('Two to four peer states are required');
 const values=states.map(state=>state?.progress);
 if(values.some(progress=>!Number.isInteger(progress?.partySetupRow)||progress.partySetupRow<0||progress.partySetupRow>4))return null;
 const {partySetupRow:row,partySetupTurns:turns,partySetupPort:port}=values[0];
 validateLength(turns);
 if(!Number.isInteger(port)||port<0||port>=4)throw Error('Party setup telemetry must report its active controller port');
 if(!values.every(progress=>progress.partySetupRow===row&&progress.partySetupTurns===turns&&progress.partySetupPort===port))
  throw Error('Party turn selector disagrees across peers');
 const owner=setupMenuOwnerIndex(states,port);
 const kind=row!==1?'cursor':'turns';
 const key=row!==1?cursorKey(row,1):turns!==target?turnAdjustmentKey(turns,target):null;
 return {owner,key,kind,selected:key===null,row,turns,port};
}
function setupActionObserved(decision,progressStates){
 if(!decision||decision.selected||!Number.isInteger(decision.row)||!Number.isInteger(decision.turns)||!Number.isInteger(decision.port)||!Array.isArray(progressStates)||progressStates.length<2||progressStates.length>4)
  throw new TypeError('A pending Party setup action and two to four peer progress states are required');
 const {key,row,turns,port}=decision,kind=decision.kind??(row===1?'turns':'cursor');
 const values=progressStates.map(state=>state?.progress||state);
 if(values.some(value=>!Number.isInteger(value?.partySetupRow)||!Number.isInteger(value?.partySetupTurns)||value.partySetupPort!==port))return false;
 if(!values.every(value=>value.partySetupRow===values[0].partySetupRow&&value.partySetupTurns===values[0].partySetupTurns))return false;
 const nextRow=values[0].partySetupRow,nextTurns=values[0].partySetupTurns;
 if(kind==='turns'){
  if(row!==1)throw new RangeError('Turn-count actions require the turn row');
  validateLength(turns);validateLength(nextTurns);
  if(key!=='a'&&key!=='d')throw new RangeError('Turn-count actions must move horizontally');
  if(nextRow!==1||nextTurns===turns)return false;
  const from=turnLengths.indexOf(turns),to=turnLengths.indexOf(nextTurns);
  const distance=key==='d'?(to-from+turnLengths.length)%turnLengths.length:(from-to+turnLengths.length)%turnLengths.length;
  // Accept auto-repeat along the requested direction, but reject a move that
  // is only explainable by wrapping the long way around the selector.
  return distance>0&&distance<=turnLengths.length/2;
 }
 if(kind!=='cursor')throw new RangeError('Party setup action kind must be cursor or turns');
 if(key!=='z'&&key!=='x')throw new RangeError('Party setup cursor actions must move vertically');
 if(nextTurns!==turns)return false;
 return key==='z'?nextRow>row&&nextRow<=4:nextRow<row&&nextRow>=0;
}
function setupActionUnchanged(decision,progressStates){
 if(!decision||decision.selected||!Array.isArray(progressStates)||progressStates.length<2||progressStates.length>4)return false;
 return progressStates.map(state=>state?.progress||state).every(value=>
  value?.partySetupRow===decision.row&&value?.partySetupTurns===decision.turns&&value?.partySetupPort===decision.port);
}
async function retryObservedSetupAction(decision,{press,waitForChange,readStates,waitFrames,onAttempt=()=>{},maxAttempts=3}={}){
 if(!decision?.key||typeof press!=='function'||typeof waitForChange!=='function'||typeof readStates!=='function'||typeof waitFrames!=='function'||
  !Number.isInteger(maxAttempts)||maxAttempts<1||maxAttempts>5)throw new TypeError('A bounded observed Party setup action is required');
 for(let attempt=1;attempt<=maxAttempts;attempt++){
  await press(decision.owner,decision.key);
  const changed=await waitForChange(decision);
  const states=await readStates(),progress=states.map(state=>state?.progress||state);
  onAttempt({attempt,key:decision.key,row:decision.row,turns:decision.turns,port:decision.port,changed,
   observed:progress.map(value=>({row:value?.partySetupRow,turns:value?.partySetupTurns,port:value?.partySetupPort}))});
  if(changed){
   if(!setupActionObserved(decision,progress))throw Error('Party setup action changed peer state inconsistently or in the wrong direction');
   await waitFrames(states);return states;
  }
  if(!setupActionUnchanged(decision,progress))throw Error('Party setup action left peers in inconsistent state; refusing to retry');
  await waitFrames(states);
  // A key can be accepted after a slow poll missed the first observation.
  // Re-read after settling before sending another edge, so a late success is
  // never turned into a duplicate cursor movement.
  const settled=await readStates(),settledProgress=settled.map(state=>state?.progress||state);
  if(setupActionObserved(decision,settledProgress)){
   onAttempt({attempt,key:decision.key,row:decision.row,turns:decision.turns,port:decision.port,changed:true,lateObserved:true,
    observed:settledProgress.map(value=>({row:value?.partySetupRow,turns:value?.partySetupTurns,port:value?.partySetupPort}))});
   return settled;
  }
  if(!setupActionUnchanged(decision,settledProgress))throw Error('Party setup action left peers in inconsistent state; refusing to retry');
  if(attempt===maxAttempts)throw Error('Party setup action was not observed after '+maxAttempts+' safe attempts');
 }
 throw Error('Party setup action retry limit exceeded');
}
function partySetupKeys(current,target,initialRow=0){
 validateLength(current);validateLength(target);
 if(!Number.isInteger(initialRow)||initialRow<0||initialRow>4)throw new RangeError('Party setup row must be between zero and four');
 const keys=[];let row=initialRow,turns=current;
 const moveTo=(destination)=>{
  while(row!==destination){const key=cursorKey(row,destination);keys.push(key);row+=key==='z'?1:-1;}
 };
 moveTo(1);
 while(turns!==target){
  const key=turnAdjustmentKey(turns,target);keys.push(key);
  const index=turnLengths.indexOf(turns);
  turns=turnLengths[(index+(key==='d'?1:-1)+turnLengths.length)%turnLengths.length];
 }
 moveTo(4);
 keys.push('z'); // The game's A button on the last row accepts the settings.
 return keys;
}
module.exports={cursorKey,turnAdjustmentKey,turnLengths,setupMenuOwnerIndex,turnSetupAction,setupActionObserved,setupActionUnchanged,retryObservedSetupAction,partySetupKeys};
