import {automaticRoom} from './automatic-room.mjs';
import {automaticPreparation} from './automatic-preparation.mjs';
import {loadBundledPipelineSeed} from './bundled-pipeline-seed.mjs';
import {sendPipelineSeed,receivePipelineSeed} from './pipeline-seed-channel.mjs';
import {captureClosedPipelineSeed} from './pipeline-seed.mjs';
import {StartBarrier} from './start-barrier.mjs';
import {StartChannels} from './start-channels.mjs';
import {prepareStartAcknowledgment,resumeCommittedClientRuntime} from './start-client.mjs';
import {prepareLobbyGameInput} from './lobby-game-input.mjs';
import {createRealtimeInputRecovery} from './realtime-input-recovery.mjs';
import {receiveSessionInputLoss,sessionInputLossNotice} from './session-input-loss.mjs';
import {createRecoveryCredential,receiveRecoveryCredential} from './recovery-identity.mjs';
import {createReplacementTransport} from './replacement-transport.mjs';
import {agreePausedState} from './paused-state-agreement.mjs';
import {reconcilePausedPoll} from './paused-poll-reconciliation.mjs';
import {alignRecoveryRoutes} from './recovery-route-alignment.mjs';
import {adoptVerifiedInput} from './verified-input-adoption.mjs';
import {RecoveryBarrier,recoveryAcknowledgment} from './recovery-barrier.mjs';
import {resumeCommittedInput} from './committed-input-recovery.mjs';
import {knownShaderReadiness} from './shader-readiness.mjs';
import {createLobbyInputRouter} from './lobby-input-router.mjs';
import {lobbyInputOwnership} from './lobby-input-ownership.mjs';
import {preparedRuntimeReport,receivePreparedRuntimeReport,comparePreparedRuntimeReports} from './prepared-runtime-report.mjs';
import {lobbyMusic} from './lobby-music.mjs';
import {audioLayout} from './audio-layout.mjs';
import {gameAudioControls} from './game-audio-controls.mjs';
import {lobbyControlEditor} from './lobby-control-editor.mjs';
import {displayControls} from './display-controls.mjs';
import {touchControls} from './touch-controls.mjs';
import {controlBindingGlyphs} from './control-bindings.mjs';
import {iceServerConfiguration} from './ice-servers.mjs';
import {applyRuntimeKeyboardBindings,applyRuntimeControllerBindings} from './runtime-control-bindings.mjs';
import {prepare,host,guest,browserStore} from './transfer.mjs';
import {LobbyState} from './lobby-state.mjs';
import {LobbyHost} from './lobby-host.mjs';
import {LobbyClient} from './lobby-client.mjs';
import {cachedDownloadControls} from './cached-download-controls.mjs';
import {downloadProgressReporter} from './download-progress-reporter.mjs';
import {monitorConnectionRoutes,connectionRouteLabel} from './connection-route.mjs';
import {renderLobby} from './lobby-view.mjs';
import {verifyStoredAsset} from './runtime-asset.mjs';
import {createSessionConfiguration,receiveSessionConfiguration,validateSessionConfiguration} from './session-config.mjs';
let downloadCacheControls;
const $=id=>document.getElementById(id);
const deviceControls=$('guest-resolution').parentElement.parentElement;
deviceControls.querySelector('h2').textContent='Display and controls for this device';
deviceControls.querySelector('small').textContent='Choose a local output resolution and aspect ratio before preparing a game. Each host and guest uses their own settings; the original 4:3 mode is available.';
const display=displayControls({container:deviceControls,select:$('guest-resolution'),gameView:$('game-view'),status:message=>status(message)});
const controlEditor=lobbyControlEditor(deviceControls);
const music=lobbyMusic($('lobby-audio'),$('music-toggle'),$('music-title'),$('music-volume'),{
  getItem:key=>localStorage.getItem(key),setItem:(key,value)=>localStorage.setItem(key,value),
});
const speakers=audioLayout($('speaker-layout'),$('speaker-status'),{
  getItem:key=>localStorage.getItem(key),setItem:(key,value)=>localStorage.setItem(key,value),
});
const gameAudio=gameAudioControls($('game-volume'),$('game-mute'),$('game-audio-status'),{
  getItem:key=>localStorage.getItem(key),setItem:(key,value)=>localStorage.setItem(key,value),
});
const status=text=>{$('status').textContent=text;downloadCacheControls?.refresh();};
let pc, receiver, store, asset, aborter, offer, generation=0;
let runtimeAsset;
let sessionConfiguration;
const recoveryCredentials=new Map();
export function lobbyRecoveryCredentialState(){return [...recoveryCredentials.values()].map(({session,peer})=>({session,peer,available:true}));}
export function lobbySessionConfiguration(){return sessionConfiguration?{...sessionConfiguration}:null;}
let lobbyRuntime,lobbyPreparing,lobbyCleanup=Promise.resolve();
let lobbyPipelineSeed,guestPipelineChannel;
export function setLobbyPipelineSeed(seed){
  if(lobbyRuntime||lobbyPreparing)throw Error('Pipeline seed must precede runtime preparation.');
  if(!(seed?.blob instanceof Blob)||seed.blob.size<100||seed.blob.size>32*1024**2||! /^[a-f0-9]{64}$/.test(seed.sha256))throw Error('Invalid pipeline seed.');
  lobbyPipelineSeed=Object.freeze({blob:new Blob([seed.blob]),sha256:seed.sha256});
}
$('pipeline-file').onchange=async()=>{
  const selected=$('pipeline-file').files[0],originalGeneration=generation;
  try{
    if(!selected)return;
    if(lobbyHost||pc||lobbyRuntime||lobbyPreparing)throw Error('Select shader definitions before creating invitations.');
    if(selected.size<100||selected.size>32*1024**2)throw Error('Shader definitions must be between 100 bytes and 32 MiB.');
    const bytes=new Uint8Array(await selected.arrayBuffer());
    if(!new TextEncoder().encode('SQLite format 3\0').every((byte,index)=>bytes[index]===byte))throw Error('Select an Aurora shader definition database.');
    const sha256=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),byte=>byte.toString(16).padStart(2,'0')).join('');
    if(generation!==originalGeneration||pc||lobbyHost)throw Error('Lobby changed while reading shader definitions.');
    setLobbyPipelineSeed({blob:new Blob([bytes]),sha256});status('Known shader definitions selected. Each player will compile them locally before Ready.');
  }catch(error){if(generation===originalGeneration)status(error.message||String(error));}
};
let guestPreparationChannel,lobbyInputRouter,lobbyGameInput,lobbyInputMembership;
let lobbyRecovery,lobbyRecoveryPlan;
let lobbyCommittedIdentity;
let lobbyRecoveryRound;
let recoveryUIOwner,recoveryUIBusy=false,recoveryUITimer;
function showRecoveryControls(){
  $('recovery-panel').hidden=false;
  $('recovery-host').hidden=$('recovery-answer').hidden=$('recovery-resume').hidden=!!automaticLobby||!lobbyHost;
  $('recovery-join').hidden=!!automaticLobby||!!lobbyHost;
  for(const element of $('recovery-panel').querySelectorAll('[data-recovery-manual]'))element.hidden=!!automaticLobby;
  $('recovery-explanation').textContent=automaticLobby?'Reconnecting automatically. The game waits up to 60 seconds and resumes only after every player confirms the same paused state.':'The game waits up to 60 seconds. Exchange these codes privately. Resume requires every player to confirm the same paused state.';
  clearInterval(recoveryUITimer);
  const originalInput=lobbyGameInput;
  const update=()=>{
    if(lobbyGameInput!==originalInput||!lobbyRecovery){clearInterval(recoveryUITimer);return;}
    const state=originalInput.queue.status(performance.now());
    $('recovery-countdown').textContent=state.state==='Paused'?Math.ceil(state.remainingMs/1000)+' seconds remaining.':'Recovery ended.';
    if(state.state!=='Paused'){clearInterval(recoveryUITimer);return;}
    $('recovery-resume').disabled=!!lobbyRecoveryRound||recoveryUIBusy;
  };
  update();recoveryUITimer=setInterval(update,250);
  if(!recoveryUIOwner){
    const state=lobbyGameInput.queue.status(performance.now()),select=$('recovery-peer');select.replaceChildren();
    for(const binding of lobbyRecoveryPlan.bindings)if(binding.peer!==lobbyRecoveryPlan.peer&&state.missingSlots[binding.slot]){
      const option=document.createElement('option');option.value=binding.peer;option.textContent=binding.peer;select.append(option);
    }
  }
}
function resetRecoveryControls(){
  clearInterval(recoveryUITimer);recoveryUITimer=undefined;$('recovery-countdown').textContent='';
  recoveryUIOwner?.close();recoveryUIOwner=undefined;recoveryUIBusy=false;
  $('recovery-panel').hidden=true;$('recovery-local').value=$('recovery-remote').value='';
  $('recovery-answer').disabled=$('recovery-copy').disabled=true;
}
const recoveryCode=()=>{const text=$('recovery-remote').value.trim();if(!text||text.length>110000)throw Error('Paste a valid reconnect code.');return JSON.parse(text);};
async function finishUIReplacement(owner){
  await owner.ready();const transferred=owner.handoff();
  if(recoveryUIOwner===owner)recoveryUIOwner=undefined;
  const receipt=await verifyLobbyReplacementState(transferred);adoptLobbyReplacementState(receipt);
  $('recovery-status').textContent='Connection and paused state verified. The host can ask all players to resume.';
  $('recovery-answer').disabled=true;
}
async function recoveryUIAction(action){
  if(recoveryUIBusy)return;recoveryUIBusy=true;
  const originalGeneration=generation;
  try{await action();}catch(error){if(generation!==originalGeneration)return;recoveryUIOwner?.close();recoveryUIOwner=undefined;
    if(lobbyRecovery)$('recovery-status').textContent=error.message||String(error);
  }finally{if(generation===originalGeneration)recoveryUIBusy=false;}
}
$('recovery-host').onclick=()=>recoveryUIAction(async()=>{
  recoveryUIOwner?.close();const owner=createLobbyReplacementTransport($('recovery-peer').value);recoveryUIOwner=owner;
  const code=await owner.createOffer();$('recovery-local').value=JSON.stringify(code);$('recovery-copy').disabled=false;$('recovery-answer').disabled=false;
  $('recovery-status').textContent='Send this reconnect invitation to the selected player, then paste their reply.';
});
$('recovery-join').onclick=()=>recoveryUIAction(async()=>{
  const code=recoveryCode();recoveryUIOwner?.close();const owner=createLobbyReplacementTransport('host');recoveryUIOwner=owner;
  const reply=await owner.acceptOffer(code);$('recovery-local').value=JSON.stringify(reply);$('recovery-copy').disabled=false;
  $('recovery-status').textContent='Send this reply to the host. Waiting for the replacement connection.';await finishUIReplacement(owner);
});
$('recovery-answer').onclick=()=>recoveryUIAction(async()=>{
  if(!recoveryUIOwner)throw Error('Create a reconnect invitation first.');const owner=recoveryUIOwner;
  await owner.acceptAnswer(recoveryCode());await finishUIReplacement(owner);
});
$('recovery-copy').onclick=async()=>{try{await navigator.clipboard.writeText($('recovery-local').value);$('recovery-status').textContent='Reconnect code copied.';}catch(error){$('recovery-status').textContent=error.message;}};
$('recovery-resume').onclick=()=>recoveryUIAction(async()=>{const round=await beginLobbyRecoveryAgreement();$('recovery-status').textContent='Waiting for '+round.pending.length+' player confirmations.';});
function recoveryContext(){
  if(!lobbyRecovery||!lobbyCommittedIdentity||!lobbyGameInput)throw Error('Running paused recovery required.');
  const runtime=lobbyRuntime,inputOwner=lobbyGameInput,identity=lobbyCommittedIdentity,configuration=sessionConfiguration;
  const input=inputOwner.queue.status(performance.now());
  const binding=lobbyRecoveryPlan.bindings.find(binding=>input.missingSlots[binding.slot]);
  const token=binding&&lobbyRecovery.attempt(binding.owner);
  if(!token)throw Error('Suspended assigned input required.');
  const current=()=>{const actual=lobbyRecovery?.attempt(token.peer);return lobbyRuntime===runtime&&lobbyGameInput===inputOwner&&
    lobbyCommittedIdentity===identity&&sessionConfiguration===configuration&&runtime.buildId===identity.buildId&&
    configuration?.session===identity.session&&configuration.assetId===identity.assetId&&configuration.seed===identity.seed&&!!actual&&
    ['peer','epoch','revision','frame','checkpoint','frand','rand8'].every(key=>actual[key]===token[key]);};
  const capture=()=>{if(!current())throw Error('Recovery round became stale.');return {attempt:token,input:inputOwner.queue.status(performance.now()),
    state:{frame:runtime.module._PartyBoard_GameFrameCount(),checkpoint:token.checkpoint,frand:token.frand,rand8:token.rand8}};};
  const transportReady=()=>{
    const peers=lobbyHost?identity.players.filter(player=>player.peer!=='host').map(player=>player.peer):['host'];
    return peers.every(peer=>lobbyInputChannels.get(peer)?.readyState==='open'&&lobbyStartChannels.get(peer)?.readyState==='open');
  };
  return {identity,capture,current,transportReady,queue:inputOwner.queue,bridge:inputOwner.bridge,peer:lobbyRecoveryPlan.peer,host:'host'};
}
const activeRouteAlignments=new WeakMap(),finishedRouteAlignments=new WeakMap();
function routeAlignmentContext(){
  const runtime=lobbyRuntime,inputOwner=lobbyGameInput,identity=lobbyCommittedIdentity,policy=inputOwner?.queue.status(performance.now());
  const binding=lobbyRecoveryPlan?.bindings.find(binding=>policy?.missingSlots[binding.slot]),initial=binding&&lobbyRecovery?.attempt(binding.owner);
  if(!initial||policy.state!=='Paused'||!identity)throw Error('Original paused route alignment required.');
  const current=()=>{const input=inputOwner.queue.status(performance.now());return lobbyRuntime===runtime&&lobbyGameInput===inputOwner&&lobbyCommittedIdentity===identity&&
    input.state==='Paused'&&input.epoch===policy.epoch&&input.revision===policy.revision&&
    (input.frame===policy.frame||input.frame===policy.frame+1n);};
  const capture=()=>{const attempt=lobbyRecovery?.attempt(initial.peer);if(!current()||!attempt)throw Error('Original route alignment became stale.');
    return{attempt,input:inputOwner.queue.status(performance.now()),state:{frame:runtime.module._PartyBoard_GameFrameCount(),checkpoint:attempt.checkpoint,frand:attempt.frand,rand8:attempt.rand8}};};
  return{identity,capture,current,bridge:inputOwner.bridge,peer:lobbyRecoveryPlan.peer,epoch:policy.epoch,signal:aborter.signal,deadline:performance.now()+policy.remainingMs};
}
async function alignAssignedRoute(peer,channel,nonce){
  if(activeRouteAlignments.has(channel))throw Error('Route alignment already pending.');
  const context=routeAlignmentContext(),previous=finishedRouteAlignments.get(channel);
  const finished=previous?.epoch===context.epoch&&previous.session===context.identity.session?previous.nonces:new Set();
  if(finished.has(nonce)||finished.size>=16)throw Error('Route alignment nonce unavailable.');
  activeRouteAlignments.set(channel,{peer,nonce});
  try{await reconcilePausedPoll({...context,channel,remotePeer:peer,nonce,previousNonces:finished,allowStartChannel:true});finished.add(nonce);finishedRouteAlignments.set(channel,{session:context.identity.session,epoch:context.epoch,nonces:finished});}
  finally{activeRouteAlignments.delete(channel);}
}
export async function beginLobbyRecoveryAgreement(){
  if(!lobbyHost||lobbyRecoveryRound)throw Error('Host recovery round already pending or unavailable.');
  const initial=recoveryContext();if(!initial.transportReady())throw Error('Every assigned recovery route must be open.');
  const runtime=lobbyRuntime,inputOwner=lobbyGameInput,policy=inputOwner.queue.status(performance.now()),aligning={aligning:true};lobbyRecoveryRound=aligning;
  try{
    await alignRecoveryRoutes({peers:initial.identity.players.filter(player=>player.peer!=='host').map(player=>player.peer),
      current:()=>{const state=inputOwner.queue.status(performance.now());return lobbyRuntime===runtime&&lobbyGameInput===inputOwner&&lobbyRecoveryRound===aligning&&
        state.state==='Paused'&&state.epoch===policy.epoch&&state.revision===policy.revision&&initial.transportReady();},
      reconcile:async peer=>{const channel=lobbyStartChannels.get(peer),nonce=Array.from(crypto.getRandomValues(new Uint8Array(16)),byte=>byte.toString(16).padStart(2,'0')).join('');
        channel.send(JSON.stringify({type:'session-poll-align',version:1,nonce,session:initial.identity.session,buildId:initial.identity.buildId,assetId:initial.identity.assetId}));
        await alignAssignedRoute(peer,channel,nonce);}});
  }catch(error){if(lobbyRecoveryRound===aligning)lobbyRecoveryRound=undefined;throw error;}
  lobbyRecoveryRound=undefined;
  const context=recoveryContext();
  const time=performance.now(),nonce=Array.from(crypto.getRandomValues(new Uint8Array(16)),byte=>byte.toString(16).padStart(2,'0')).join('');
  const barrier=new RecoveryBarrier({...context,nonce,deadline:time+context.queue.status(time).remainingMs});
  lobbyRecoveryRound={context,barrier,acknowledged:barrier.proposal};
  try{for(const player of context.identity.players)if(player.peer!=='host')lobbyStartChannels.get(player.peer).send(JSON.stringify(barrier.proposal));}
  catch(error){barrier.cancel();lobbyRecoveryRound=undefined;throw error;}
  return {pending:barrier.pending};
}
function receiveRunningRecovery(peer,channel,message){
  if(!lobbyRecovery||(lobbyStartChannels.get(peer)!==channel&&originalRecoveryChannels.get(peer)!==channel))return false;
  if(['paused-poll-report','paused-poll-ack'].includes(message.type)){
    const active=activeRouteAlignments.get(channel);
    return message.peer===peer&&(active?.nonce===message.nonce||finishedRouteAlignments.get(channel)?.nonces.has(message.nonce));
  }
  if(message.type==='session-poll-align'&&!lobbyHost&&peer==='host'){
    if(lobbyRecoveryRound||Object.keys(message).length!==6||message.version!==1||! /^[a-f0-9]{32}$/.test(message.nonce)||
      !['session','buildId','assetId'].every(key=>message[key]===lobbyCommittedIdentity?.[key]))throw Error('Invalid assigned route alignment.');
    alignAssignedRoute(peer,channel,message.nonce).catch(error=>fail(error));return true;
  }
  if(!lobbyHost&&peer==='host'&&message.type==='session-input-loss'){
    suspendLobbyConnection(receiveSessionInputLoss(message,{session:sessionConfiguration.session,plan:lobbyRecoveryPlan}),false);return true;
  }
  if(message.type==='session-recovery-proposal'&&!lobbyHost&&peer==='host'){
    if(lobbyRecoveryRound)throw Error('Recovery round already pending.');
    const context=recoveryContext();if(!context.transportReady())throw Error('Recovery routes unavailable.');
    const ack=recoveryAcknowledgment(message,context.identity,context.capture(),context);
    lobbyRecoveryRound={context,acknowledged:structuredClone(message)};channel.send(JSON.stringify(ack));return true;
  }
  if(message.type==='session-recovery-ack'&&lobbyHost){
    const round=lobbyRecoveryRound;if(!round?.barrier)throw Error('No host recovery proposal.');
    if(round.barrier.acknowledge(peer,message)){
      if(!round.context.transportReady())throw Error('Recovery route lost before commit.');
      const commit=round.barrier.commit();
      for(const player of round.context.identity.players)if(player.peer!=='host')lobbyStartChannels.get(player.peer).send(JSON.stringify(commit));
      resumeCommittedInput({...round.context,message:commit,acknowledged:round.acknowledged});lobbyRecoveryRound=undefined;announcedInputLoss.clear();
      resetRecoveryControls();status('All players confirmed recovery. Game resumed.');
    }return true;
  }
  if(message.type==='session-recovery-commit'&&!lobbyHost&&peer==='host'){
    const round=lobbyRecoveryRound;if(!round)throw Error('Unacknowledged recovery commit.');
    resumeCommittedInput({...round.context,message,acknowledged:round.acknowledged});lobbyRecoveryRound=undefined;announcedInputLoss.clear();resetRecoveryControls();status('All players confirmed recovery. Game resumed.');return true;
  }
  return false;
}
const lobbyReplacementTransfers=new WeakMap(),verifiedReplacementStates=new WeakMap();
export async function verifyLobbyReplacementState(transferred){
  const owned=lobbyReplacementTransfers.get(transferred);
  if(!owned||owned.state!=='Transport'||!lobbyRecovery||!lobbyGameInput||!lobbyCommittedIdentity)throw Error('Owned paused lobby replacement required.');
  owned.state='Verifying';
  const identity=lobbyCommittedIdentity,channel=owned.control;
  const originalAttempt=owned.attempt;
  owned.reconciling=true;
  const current=()=>{
    const input=lobbyGameInput?.queue.status(performance.now());
    if(owned.reconciling){
      if(lobbyRuntime!==owned.runtime||sessionConfiguration?.session!==transferred.session||input?.state!=='Paused'||
         input.epoch!==originalAttempt.epoch||input.revision!==originalAttempt.revision||
         (input.frame!==originalAttempt.frame&&input.frame!==originalAttempt.frame+1n))return false;
      const pending=lobbyGameInput.bridge.pendingState();
      if(input.frame===originalAttempt.frame)return !!pending&&['frame','checkpoint','frand','rand8'].every(key=>pending[key]===originalAttempt[key]);
      return true; // one original callback is executing; the protocol checks its resulting state
    }
    const actual=lobbyRecovery?.attempt(owned.attempt.peer);
    return lobbyRuntime===owned.runtime&&sessionConfiguration?.session===transferred.session&&!!actual&&
      ['peer','epoch','revision','frame','checkpoint','frand','rand8'].every(key=>actual[key]===owned.attempt[key]);
  };
  const capture=()=>{if(!current())throw Error('Replacement paused state changed.');
    const attempt=lobbyRecovery.attempt(owned.attempt.peer),input=lobbyGameInput.queue.status(performance.now());
    return {attempt,input,state:{frame:lobbyRuntime.module._PartyBoard_GameFrameCount(),checkpoint:attempt.checkpoint,frand:attempt.frand,rand8:attempt.rand8}};
  };
  const time=performance.now(),remaining=lobbyGameInput.queue.status(time).remainingMs;
  try{
    await reconcilePausedPoll({channel,identity,peer:lobbyRecoveryPlan.peer,remotePeer:owned.peer,nonce:transferred.nonce,
      capture,current,bridge:lobbyGameInput.bridge,signal:aborter.signal,deadline:time+remaining});
    const reconciled=lobbyRecovery.attempt(originalAttempt.peer);
    if(!current()||!reconciled)throw Error('Reconciled original pending poll unavailable.');
    owned.attempt=reconciled;owned.reconciling=false;
    const receipt=await agreePausedState({channel,identity,peer:lobbyRecoveryPlan.peer,remotePeer:owned.peer,nonce:transferred.nonce,
      capture,current,signal:aborter.signal,deadline:time+remaining});
    if(!current())throw Error('Replacement attempt changed after state agreement.');
    owned.state='Verified';verifiedReplacementStates.set(receipt,{transferred,owned,identity,capture,current});return receipt;
  }catch(error){owned.state='Failed';transferred.connection.close();connections.delete(transferred.connection);throw error;}
}
export function adoptLobbyReplacementState(receipt){
  const verified=verifiedReplacementStates.get(receipt);
  if(!verified||verified.owned.state!=='Verified'||!verified.current()||!lobbyInputRouter)throw Error('Verified current lobby replacement required.');
  const {owned,transferred,identity,capture,current}=verified,peer=owned.peer;
  const adopted=adoptVerifiedInput({receipt,identity,peer:lobbyRecoveryPlan.peer,remotePeer:peer,nonce:transferred.nonce,
    input:owned.input,control:owned.control,router:lobbyInputRouter,capture,current});
  owned.state='Adopted';verifiedReplacementStates.delete(receipt);
  finishedRouteAlignments.set(adopted.control,{session:identity.session,
    epoch:lobbyGameInput.queue.status(performance.now()).epoch,nonces:new Set([adopted.nonce])});
  const old=lobbyHost?guestConnections.get(peer):pc,connection=owned.connection,runtime=owned.runtime;
  lobbyInputChannels.set(peer,adopted.input);lobbyStartChannels.set(peer,adopted.control);
  if(lobbyHost)guestConnections.set(peer,connection);else pc=connection;
  const active=()=>lobbyRuntime===runtime&&connections.has(connection)&&(lobbyHost?guestConnections.get(peer)===connection:pc===connection);
  connection.addEventListener('datachannel',event=>event.channel.close());
  connection.addEventListener('connectionstatechange',()=>{if(active()&&['failed','disconnected'].includes(connection.connectionState))suspendLobbyConnection(peer);});
  adopted.control.addEventListener('close',()=>{if(active()&&lobbyStartChannels.get(peer)===adopted.control)suspendLobbyConnection(peer);});
  adopted.control.addEventListener('message',event=>{if(!active())return;try{
    if(typeof event.data!=='string'||event.data.length>4096)throw Error('Invalid running recovery control.');
    const message=JSON.parse(event.data);
    if(['paused-state-report','paused-state-ack','paused-poll-report','paused-poll-ack'].includes(message.type)&&message.nonce===adopted.nonce)return;
    if(receiveRunningRecovery(peer,adopted.control,message))return;
    if(!lobbyHost&&peer==='host'&&message.type==='session-input-loss'){
      const missing=receiveSessionInputLoss(message,{session:sessionConfiguration.session,plan:lobbyRecoveryPlan});suspendLobbyConnection(missing,false);return;
    }
    throw Error('Unexpected running recovery control.');
  }catch(error){fail(error);}});
  if(old&&old!==connection){connections.delete(old);old.close();}
  status('Replacement connection verified. Game remains paused until all players agree to resume.');
  return {peer,owner:owned.attempt.peer,state:lobbyGameInput.queue.status(performance.now()).state};
}
const lobbyReplacements=new Set(),replacementNonces=new Map();
export function createLobbyReplacementTransport(peer){
  if(!lobbyRecovery||!lobbyGameInput||!lobbyRecoveryPlan)throw Error('Paused running lobby recovery required.');
  const binding=lobbyRecoveryPlan.bindings.find(binding=>binding.peer===peer&&binding.peer!==lobbyRecoveryPlan.peer);
  if(!binding||!lobbyGameInput.queue.status(performance.now()).missingSlots[binding.slot])throw Error('Missing assigned peer required for replacement.');
  const attempt=binding&&lobbyRecovery.attempt(binding.owner);
  if(!attempt)throw Error('Current suspended peer input required.');
  const credential=recoveryCredentials.get(lobbyHost?peer:localPeer);
  if(!credential)throw Error('Original peer recovery credential required.');
  const time=performance.now(),remaining=lobbyGameInput.queue.status(time).remainingMs;
  if(remaining<=0)throw Error('Recovery deadline expired.');
  const iceServers=iceConfiguration();
  if(!replacementNonces.has(credential.peer))replacementNonces.set(credential.peer,new Set());
  const owner=createReplacementTransport({credential,role:lobbyHost?'host':'guest',usedNonces:replacementNonces.get(credential.peer),
    signal:aborter.signal,deadline:time+remaining,createConnection:()=>new RTCPeerConnection({iceServers})});
  lobbyReplacements.add(owner);
  const originalRuntime=lobbyRuntime,originalSession=sessionConfiguration?.session;
  const current=()=>{
    const actual=lobbyRecovery?.attempt(binding.owner);
    if(lobbyRuntime!==originalRuntime||sessionConfiguration?.session!==originalSession||!actual||!['peer','epoch','revision','frame','checkpoint','frand','rand8'].every(key=>actual[key]===attempt[key])){
      owner.close();lobbyReplacements.delete(owner);throw Error('Replacement recovery attempt changed.');
    }
  };
  const checked=method=>async(...args)=>{current();try{const result=await owner[method](...args);current();return result;}catch(error){owner.close();lobbyReplacements.delete(owner);throw error;}};
  return {attempt,createOffer:checked('createOffer'),acceptOffer:checked('acceptOffer'),acceptAnswer:checked('acceptAnswer'),ready:checked('ready'),
    handoff(){current();const transferred=owner.handoff();connections.add(transferred.connection);lobbyReplacements.delete(owner);
      const result=Object.freeze({...transferred,attempt});lobbyReplacementTransfers.set(result,{peer,attempt,runtime:lobbyRuntime,state:'Transport',
        input:transferred.channels.get('partyboard-input-v1'),control:transferred.channels.get('partyboard-recovery-v1'),connection:transferred.connection});return result;},
    close(){owner.close();lobbyReplacements.delete(owner);}};
}
const announcedInputLoss=new Set();
const inputRouteLosses=[];
export function lobbyTransportDiagnostics(){return {inputRouteLosses:structuredClone(inputRouteLosses),
  inputChannels:[...lobbyInputChannels].map(([peer,channel])=>({peer,state:channel.readyState,bufferedAmount:channel.bufferedAmount})),
  controlChannels:[...lobbyStartChannels].map(([peer,channel])=>({peer,state:channel.readyState,bufferedAmount:channel.bufferedAmount}))};}
function suspendLobbyConnection(peer,upstream=true){
  if(!lobbyRecovery||!lobbyGameInput||!lobbyRecoveryPlan)return false;
  const bindings=lobbyHost||!upstream?lobbyRecoveryPlan.bindings.filter(binding=>binding.peer===peer):lobbyRecoveryPlan.bindings.filter(binding=>binding.peer!==lobbyRecoveryPlan.peer);
  if(!bindings.length)return false;
  lobbyRecoveryRound?.barrier?.cancel();lobbyRecoveryRound=undefined;
  for(const binding of bindings)if(!lobbyGameInput.queue.status(performance.now()).missingSlots[binding.slot])lobbyGameInput.queue.disconnected(binding.owner,performance.now());
  lobbyGameInput.bridge.notify();
  if(lobbyHost&&!announcedInputLoss.has(peer)){
    announcedInputLoss.add(peer);
    const notice=JSON.stringify(sessionInputLossNotice(peer,{session:sessionConfiguration.session,plan:lobbyRecoveryPlan}));
    for(const [other,channel] of lobbyStartChannels)if(other!==peer&&channel.readyState==='open'){
      try{channel.send(notice);}catch(error){console.error('Recovery loss notice failed',error);}
    }
  }
  status('Connection lost. Game paused for verified reconnection (up to 60 seconds).');
  showRecoveryControls();
  return true;
}
export function connectLobbyGameInput(){
  if(lobbyGameInput)throw Error('Game input already connected.');
  inputRouteLosses.length=0;
  const plan=lobbyInputPlan(),session=Uint8Array.from(sessionConfiguration.session.match(/../g),b=>parseInt(b,16));
  lobbyInputMembership=plan.membership;
  lobbyGameInput=prepareLobbyGameInput(lobbyRuntime,{plan,session,bind:bindLobbyInput});
  lobbyRuntime.inputExchangeInstalled=true;
  return {installed:true,membership:plan.membership};
}
const lobbyStartChannels=new Map();let lobbyStartOwner,clientStartPreparation,startPreparing=false;
const originalRecoveryChannels=new Map();
let lobbyStartBarrier;
export function lobbyStartState(){return lobbyStartBarrier?{state:lobbyStartBarrier.state,pending:lobbyStartBarrier.pending}:null;}
$('start-agreement').onclick=()=>{try{const state=beginLobbyStartAgreement();$('start-agreement').disabled=true;status('Start confirmation: '+state.pending.length+' players pending.');}catch(error){status(error.message||String(error));}};
$('start-game').onclick=()=>{try{commitLobbyStart();$('start-game').disabled=true;status('Agreed online game started.');}catch(error){fail(error);}};
export function lobbyReadyState(){return currentLobbySnapshot?{canStart:currentLobbySnapshot.canStart,players:currentLobbySnapshot.players.map(({peer,ready,shaders})=>({peer,ready,shaders}))}:null;}
export function lobbyInputObservations(){return lobbyGameInput?.observations()??[];}
// Wall time of completed exchanges includes peer waits and any recovery pause.
// This diagnostic snapshot does not participate in deterministic state hashing.
export function lobbyInputTiming(){return lobbyGameInput?.timing()??null;}
function gameProgress(module){
  if(typeof module?._PartyBoard_GameProgress!=='function')return null;
  const read=module._PartyBoard_GameProgress;
  return {turn:read(0),maxTurn:read(1),board:read(2),player:read(3),overlay:read(4),event:read(5),
    menuBoard:read(6),menuPort:read(7),
    partySetupTurns:read(9),partySetupRow:read(10),partySetupPort:read(11),
    mapViewPort:typeof module._PartyBoardBrowserMapViewPort==='function'?module._PartyBoardBrowserMapViewPort():null,
    mapView:typeof module._PartyBoardBrowserMapViewActive==='function'?!!module._PartyBoardBrowserMapViewActive():false};
}
export function lobbyGameState(){if(!lobbyRuntime||!lobbyGameInput)return null;const input=lobbyGameInput.queue.status(performance.now());return {phase:lobbyRuntime.engine.phase,frame:lobbyRuntime.module._PartyBoard_GameFrameCount(),progress:gameProgress(lobbyRuntime.module),input:{state:input.state,frame:input.frame.toString(),stopReason:input.stopReason,localPadSlots:lobbyRuntime.module.partyBoardGlyphLocalPadSlots},pending:lobbyGameInput.bridge.pendingState()?{...lobbyGameInput.bridge.pendingState(),frame:lobbyGameInput.bridge.pendingState().frame.toString()}:null};}
$('ready-game').onclick=()=>{try{const receipt=markLobbyReady();status('Ready: '+receipt.created+' known shader pipelines prepared. Additional scene coverage remains under development.');}catch(error){status(error.message||String(error));}};
export function markLobbyReady(){
  if(lobbyRecovery||lobbyStartOwner||clientStartPreparation||!currentLobbySnapshot)throw Error('Prepared lobby ready action required.');
  const capability=knownShaderReadiness(lobbyRuntime),configuration=sessionConfiguration;
  const actual=preparedRuntimeReport(lobbyRuntime,configuration),peer=lobbyHost?'host':localPeer;
  if(JSON.stringify(preparedReports.get(peer))!==JSON.stringify(actual))throw Error('Current prepared runtime report required.');
  const player=currentLobbySnapshot.players.find(player=>player.peer===peer);if(!player?.verified)throw Error('Verified assigned player required.');
  const report={assetId:configuration.assetId,sequence:lobbyHost?player.sequence+1:++reportSequence,
    bytes:currentLobbySnapshot.total,verified:true,shaders:true,ready:true,membership:currentLobbySnapshot.membership};
  if(lobbyHost)lobbyHost.reportLocal(peer,report);
  else {if(statusChannel?.readyState!=='open')throw Error('Lobby report connection closed.');statusChannel.send(JSON.stringify({type:'lobby-report',version:1,report}));}
  return capability;
}
export function beginLobbyStartAgreement(){
  if(!lobbyHost||!lobbyRuntime||lobbyStartOwner)throw Error('Prepared host lobby required.');
  checkPreparedAgreement();
  if(currentLobbySnapshot.players.some(player=>!preparedReports.has(player.peer)))throw Error('Every current peer must report its actual prepared runtime.');
  const configuration=sessionConfiguration;
  const session=Uint8Array.from(configuration.session.match(/../g),b=>parseInt(b,16));
  const barrier=new StartBarrier(currentLobbySnapshot,{session,seed:configuration.seed,
    buildId:lobbyRuntime.buildId,checkpoint:lobbyRuntime.checkpoint});
  if(!lobbyGameInput)connectLobbyGameInput();
  const owner=new StartChannels(barrier,{snapshot:()=>currentLobbySnapshot,requireOnlineInput:true,
    changed:state=>{$('start-game').disabled=state!=='Agreed';},
    error:(_,error)=>fail(error)});
  try{
    owner.acknowledgeLocal('host',{...barrier.proposal,type:'session-start-ack'});
    for(const binding of lobbyInputPlan().bindings)if(binding.peer!=='host'){
      const channel=lobbyStartChannels.get(binding.peer);if(channel?.readyState!=='open')throw Error('Start channel unavailable.');
      owner.attach(binding.peer,channel);
    }
    lobbyStartOwner=owner;lobbyStartBarrier=barrier;return {state:barrier.state,pending:barrier.pending};
  }catch(error){owner.close();throw error;}
}
function runCommittedLobbyGame(commit,prepared,peer){
  const runtime=lobbyRuntime,input=lobbyGameInput,plan=lobbyInputPlan();
  const run=resumeCommittedClientRuntime(commit,prepared,{peer,buildId:lobbyRuntime.buildId,
    snapshot:()=>currentLobbySnapshot,requireOnlineInput:true});
  lobbyRecoveryPlan=plan;
  lobbyCommittedIdentity=structuredClone(commit);
  lobbyHost?.freezeMembership();
  lobbyRecovery=createRealtimeInputRecovery(input.queue,input.bridge,{
    execution:run,channels:{close(){input.close();}},
    transportReady:owner=>{
      if(!lobbyHost)return lobbyInputChannels.get('host')?.readyState==='open';
      const bindings=plan.bindings.filter(binding=>binding.peer!==plan.peer&&(owner===undefined||binding.owner===owner));
      return bindings.every(binding=>lobbyInputChannels.get(binding.peer)?.readyState==='open');
    },
    dispose:async()=>{releaseLobbyRuntime();await lobbyCleanup;},
    returnToMenu:async()=>{stop();await lobbyCleanup;$('game-view').hidden=true;status('Online session ended. Return to the lobby to start again.');},
    error:error=>console.error('Online session recovery',error),
  });
  $('game-view').hidden=false;$('canvas').focus();music.stop();
  $('host').disabled=$('join').disabled=true;
  return {phase:lobbyRuntime.engine.phase};
}
export function commitLobbyStart(){
  if(!lobbyStartOwner||!lobbyRuntime)throw Error('Agreed host start required.');
  const owner=lobbyStartOwner,runtime=lobbyRuntime;
  owner.sendRuntimeCommit('host',runtime);
  const handoff=owner.handoff('host',runtime);lobbyStartOwner=undefined;
  for(const {peer,channel} of handoff.channels)channel.addEventListener('close',()=>{if(lobbyRuntime===runtime&&lobbyStartChannels.get(peer)===channel)suspendLobbyConnection(peer);},{once:true});
  for(const {peer,channel} of handoff.channels)channel.addEventListener('message',event=>{
    if(lobbyRuntime!==runtime||lobbyStartChannels.get(peer)!==channel)return;
    try{if(typeof event.data!=='string'||event.data.length>4096||!receiveRunningRecovery(peer,channel,JSON.parse(event.data)))throw Error('Unexpected running control packet.');}catch(error){fail(error);}
  });
  return runCommittedLobbyGame(handoff.commit,{runtime,acknowledgment:{...handoff.commit,type:'session-start-ack'}},'host');
}
const lobbyInputChannels=new Map();
export function bindLobbyInput(receive,disconnected){
  if(lobbyInputRouter)throw Error('Lobby input already bound.');
  const plan=lobbyInputPlan();
  if(!plan||lobbyInputChannels.size!==(lobbyHost?plan.bindings.length-1:1)||[...lobbyInputChannels.values()].some(channel=>channel.readyState!=='open'))throw Error('Every peer needs an open input channel.');
  const session=Uint8Array.from(sessionConfiguration.session.match(/../g),b=>parseInt(b,16));
  lobbyInputRouter=createLobbyInputRouter({plan,session,host:!!lobbyHost,channels:new Map(lobbyInputChannels),receive,
    disconnected:(owners,loss)=>{inputRouteLosses.push({...loss,at:performance.now()});if(inputRouteLosses.length>32)inputRouteLosses.shift();disconnected?.(owners);for(const owner of owners){const binding=plan.bindings.find(binding=>binding.owner===owner);if(binding)suspendLobbyConnection(binding.peer,false);}},error:(_,error)=>fail(error)});
  return lobbyInputRouter;
}
const preparedReports=new Map();
export function lobbyPreparedReports(){return Object.fromEntries([...preparedReports].map(([id,report])=>[id,{...report}]));}
function checkPreparedAgreement(){
  const hostReport=preparedReports.get('host');if(!hostReport)return;
  for(const [id,report] of preparedReports)if(id!=='host'&&!comparePreparedRuntimeReports(hostReport,report))
    throw Error('Prepared game differs for '+id+'.');
}
export function lobbyRuntimeState(){return lobbyRuntime?{phase:lobbyRuntime.engine.phase,
  frame:lobbyRuntime.module._PartyBoard_GameFrameCount(),buildId:lobbyRuntime.buildId,checkpoint:lobbyRuntime.checkpoint,seed:sessionConfiguration?.seed,
  audio:{sourceChannels:lobbyRuntime.module._PartyBoardBrowserAudioSourceChannels?.(),synthGainPermille:lobbyRuntime.module._PartyBoardBrowserAudioSynthMetric?.(14),streamGainPermille:lobbyRuntime.module._PartyBoardBrowserAudioSynthMetric?.(15)},
  pipelineSeed:lobbyRuntime.pipelineSeed,inputExchangeInstalled:!!lobbyGameInput,rendererPreparation:lobbyRuntime.rendererPreparation}:null;}
function releaseLobbyRuntime(){
  lobbyStartBarrier=undefined;$('start-game').disabled=$('start-agreement').disabled=true;
  resetRecoveryControls();
  lobbyRecoveryRound?.barrier?.cancel();lobbyRecoveryRound=undefined;
  lobbyCommittedIdentity=undefined;
  for(const replacement of lobbyReplacements)replacement.close();lobbyReplacements.clear();
  const recovery=lobbyRecovery;lobbyRecovery=undefined;lobbyRecoveryPlan=undefined;
  announcedInputLoss.clear();
  recovery?.close().catch(error=>console.error('Lobby recovery cleanup failed',error));
  lobbyGameInput?.close();lobbyGameInput=undefined;lobbyInputMembership=undefined;
  const pending=lobbyPreparing,owned=lobbyRuntime;lobbyRuntime=undefined;gameTouchControls.refresh();speakers.bind(undefined);
  display.setLocked(false);
  controlEditor.setLocked(false);
  lobbyCleanup=lobbyCleanup.then(async()=>{await pending?.catch(()=>{});if(owned)await owned.dispose();});
  lobbyCleanup.catch(error=>console.error('Lobby runtime cleanup failed',error));
  lobbyCleanup.then(()=>downloadCacheControls?.refresh(),()=>downloadCacheControls?.refresh());
}
async function prepareLobbyGame(){
  if(lobbyPreparing||lobbyRuntime)return;
  const current=generation,signal=aborter?.signal,configuration=sessionConfiguration;
  if(!signal||!configuration||(!lobbyHost&&!runtimeAsset)){status('Connect and verify assets before preparing the game.');return;}
  const resolution=display.resolution();display.setLocked(true);
  const controlProfile=controlEditor.profile();controlEditor.setLocked(true);
  $('prepare-game').disabled=true;status('Preparing lobby game runtime…');
  lobbyPreparing=(async()=>{
    await stopLocalGame();await lobbyCleanup;signal.throwIfAborted();
    const [{localAssetStore},{loadBrowserRuntimePackage},{prepareFingerprintedSessionRuntime},
      {browserPlatformCallbacks},{prepareRuntimeGlyphs}]=await Promise.all([
      import('./local-asset-store.mjs'),import('./browser-runtime-package.mjs'),import('./session-runtime.mjs'),
      import('./browser-platform.mjs'),import('./runtime-glyphs.mjs')]);
    const source=lobbyHost?localAssetStore(asset):store;
    const files=await loadBrowserRuntimePackage({baseURL:new URL('./',location.href).href,signal,
      supplementalArtifacts:configuration.runtimeArtifacts??[]});
    const glyphs=await prepareRuntimeGlyphs({signal,bindings:controlBindingGlyphs(controlProfile)});
    const prepared=await prepareFingerprintedSessionRuntime(source,configuration.assetId,files,{
      runtimeBuild:{expectedBuildId:configuration.buildId},
      sessionSeed:configuration.seed,pipelineSeed:lobbyPipelineSeed,signal,...capturingPlatformCallbacks(browserPlatformCallbacks,{resolution,maxSteps:lobbyPipelineSeed?10000:60,
        progress:({queued,created})=>{if(current===generation)status('Preparing recorded shader pipelines: '+created+' compiled, '+queued+' queued.');}},true),
      runtimeOptions:{canvas:$('canvas'),partyBoardGlyphTextures:glyphs.initial,
        print:line=>console.info(line),printErr:line=>console.warn(line)},
      progress:(bytes,total)=>{if(current===generation)status('Checking lobby game assets: '+Math.floor(bytes/total*100)+'%');},
    });
    if(current!==generation||signal.aborted){await prepared.dispose();return;}
    lobbyRuntime=prepared;
    gameTouchControls.refresh();
    if(controlEditor.customized('keyboard'))applyRuntimeKeyboardBindings(prepared.module,controlProfile);
    if(controlEditor.customized('controller'))applyRuntimeControllerBindings(prepared.module,controlProfile);
    if(typeof prepared.module._PartyBoardBrowserHeapFailureAddress==='function')
      prepared.module.partyBoardHeapFailureAddress=prepared.module._PartyBoardBrowserHeapFailureAddress()>>>0;
    speakers.bind(prepared.module);
    const report=preparedRuntimeReport(prepared,configuration);
    if(lobbyHost){preparedReports.set('host',report);checkPreparedAgreement();}
    else {
      if(guestPreparationChannel?.readyState!=='open')throw Error('Preparation connection closed.');
      guestPreparationChannel.send(JSON.stringify(report));preparedReports.set(localPeer,report);
    }
    status(manualLobby?'Lobby game initialized and paused at frame zero. Mark ready after the known shader pipelines finish preparing. Additional scene coverage remains under development.':'Game initialized. Completing automatic readiness checks…');
  })();
  try{await lobbyPreparing;}catch(error){if(current===generation&&!signal.aborted){
    const diagnostic=(cause,depth=0)=>({message:String(cause?.message??cause),stack:cause?.stack,
      ...(depth<4&&Array.isArray(cause?.errors)?{errors:cause.errors.map(child=>diagnostic(child,depth+1))}:{})});
    console.error('Lobby preparation diagnostic: '+JSON.stringify(diagnostic(error)));
    if(lobbyRuntime){const failed=lobbyRuntime;lobbyRuntime=undefined;preparedReports.delete(lobbyHost?'host':localPeer);try{await failed.dispose();}catch(cleanup){console.error(cleanup);}}
    status('Lobby game preparation failed: '+error.message);throw error;}} 
  finally{lobbyPreparing=undefined;$('prepare-game').disabled=false;display.setLocked(Boolean(lobbyRuntime));controlEditor.setLocked(Boolean(lobbyRuntime));}
}
$('prepare-game').onclick=()=>prepareLobbyGame().catch(()=>{});
let capturedPipelineSeed,capturedLobbyPipelineSeed;
export function lastCapturedPipelineSeed(){return capturedPipelineSeed;}
export function lastCapturedLobbyPipelineSeed(){return capturedLobbyPipelineSeed;}
function capturingPlatformCallbacks(browserPlatformCallbacks,options={},lobby=false){
  const callbacks=browserPlatformCallbacks({...options,audioChannels:speakers.channels}),shutdown=callbacks.shutdown,setup=callbacks.setup;
  return {...callbacks,setup:async(module,options)=>{await setup(module,options);gameAudio.bind(module);},shutdown:async module=>{
    gameAudio.unbind(module);await shutdown(module);
    try{
      const seed=await captureClosedPipelineSeed(module);
      if(lobby)capturedLobbyPipelineSeed=seed;else capturedPipelineSeed=seed;
    }catch(error){console.warn('Pipeline capture unavailable: '+error.message);}
  }};
}
let localRuntime,localBridge,localAbort,localPreparing,localGlyphs;
const gameTouchControls=touchControls({container:$('game-view'),canvas:$('canvas'),profile:()=>controlEditor.profile(),
  enabled:()=>Boolean(localRuntime||lobbyRuntime)});
export function lobbyRewindDiagnostics() {
  return lobbyRuntime?.module?.partyboardRewindDiagnostics || [];
}

export function lobbyHeapFailure() {
  const module=lobbyRuntime?.module;
  const address=module?.partyBoardHeapFailureAddress;
  const bytes=module?.HEAPU8;
  if(Number.isInteger(address)&&address>0&&bytes&&address+36<=bytes.byteLength){
    const view=new DataView(bytes.buffer,bytes.byteOffset+address,36);
    const words=Array.from({length:9},(_,index)=>view.getUint32(index*4,true));
    const snapshot={reader:'direct-memory',address,memoryBytes:bytes.byteLength,words,
      asyncifyState:module.Asyncify?.state};
    if(!words[0])return {available:true,failed:false,snapshot};
    const [,heap,requested,aligned,tag,freeBytes,largestFree,blocks,returnAddress]=words;
    return {available:true,failed:true,heap,requested,aligned,tag,freeBytes,largestFree,blocks,returnAddress,snapshot};
  }
  const asyncifyState=lobbyRuntime?.module?.Asyncify?.state;
  // A trapped rewind can leave Asyncify active. Calling another Wasm export
  // then may consume rewind data or return a synthetic zero instead of reading.
  if(asyncifyState!==undefined&&asyncifyState!==0)return {available:false,reason:'runtime-suspended-or-rewinding',asyncifyState};
  const read=lobbyRuntime?.module?._PartyBoardBrowserHeapFailureWord;
  if(typeof read!=='function')return {available:false};
  const words=Array.from({length:9},(_,index)=>read(index)>>>0);
  if(!words[0])return {available:true,failed:false};
  const [,heap,requested,aligned,tag,freeBytes,largestFree,blocks,returnAddress]=words;
  return {available:true,failed:true,heap,requested,aligned,tag,freeBytes,largestFree,blocks,returnAddress};
}
export function localGameState() {
  if(!localRuntime)return null;
  const module=localRuntime.module,reads=localRuntime.assetReadStats;
  const asyncify=module?.Asyncify,data=asyncify?.currData,heap=module?.HEAPU8;
  const asyncifyStackHeader=Number.isInteger(data)&&data>0&&heap&&data+12<=heap.byteLength?
    Array.from({length:3},(_,index)=>new DataView(heap.buffer,heap.byteOffset+data+index*4,4).getUint32(0,true)):undefined;
  const openAssetStreams=module?.FS?.streams?.filter(stream=>stream?.node?.asyncBlob).map(stream=>({path:stream.path,position:stream.position}))??[];
  return {phase:localRuntime.engine.phase,frame:module?._PartyBoard_GameFrameCount(),asyncifyState:module?.Asyncify?.state,
      asyncifyDetail:asyncify?{state:asyncify.state,currData:data,stackHeader:asyncifyStackHeader,
        pendingSleepCallbacks:asyncify.sleepCallbacks?.length,hasPromiseHandlers:Boolean(asyncify.asyncPromiseHandlers),
        exportCallStack:asyncify.exportCallStack?.map(fn=>fn?.name||'<anonymous>'),
        rewindDiagnostics:module?.partyboardRewindDiagnostics?.slice(-5)}:undefined,
      assetReads:reads?{reads:reads.reads,bytes:reads.bytes,largestRead:reads.largestRead,pending:reads.pending,
        lastPending:reads.lastPending,lastError:reads.lastError,openStreams:openAssetStreams}:undefined,
    audio:{synthGainPermille:module?._PartyBoardBrowserAudioSynthMetric?.(14),streamGainPermille:module?._PartyBoardBrowserAudioSynthMetric?.(15)},
    progress:gameProgress(module),buildId:localRuntime.buildId};
}
async function stopLocalGame() {
  localAbort?.abort();localBridge?.close();localBridge=undefined;
  speakers.bind(undefined);
  localGlyphs?.close();localGlyphs=undefined;
  await localPreparing?.catch(()=>{});
  if(localRuntime){await localRuntime.dispose();localRuntime=undefined;}
  $('game-view').hidden=true;gameTouchControls.refresh();$('play-local').disabled=false;display.setLocked(false);controlEditor.setLocked(false);
}
$('stop-game').onclick=async()=>{try{await stopLocalGame();status('Local game stopped.');}catch(error){status(error.message);}};
$('play-local').onclick=async()=>{
  if(localPreparing||localRuntime)return;
  const file=$('file').files[0];if(!file){status('Choose your local game file first.');return;}
  stop();localAbort=new AbortController();const signal=localAbort.signal;
  $('play-local').disabled=true;$('game-view').hidden=false;display.setLocked(true);controlEditor.setLocked(true);
  status('Preparing local game…');
  localPreparing=(async()=>{
    const [{localAssetStore},{loadBrowserRuntimePackage},{prepareFingerprintedSessionRuntime},
      {browserPlatformCallbacks},{installOnlineMenuHandoff},{prepareRuntimeGlyphs}]=await Promise.all([
      import('./local-asset-store.mjs'),import('./browser-runtime-package.mjs'),import('./session-runtime.mjs'),
      import('./browser-platform.mjs'),import('./online-menu-handoff.mjs'),import('./runtime-glyphs.mjs')]);
    signal.throwIfAborted();
    if(asset?.file!==file)asset=await prepare(file,value=>{$('progress').value=value;},signal);
    const files=await loadBrowserRuntimePackage({baseURL:new URL('./',location.href).href,signal});
    const seed=crypto.getRandomValues(new Uint32Array(1))[0];
    const controlProfile=controlEditor.profile(),resolution=display.resolution();
    const glyphs=await prepareRuntimeGlyphs({signal,bindings:controlBindingGlyphs(controlProfile)});
    const runtime=await prepareFingerprintedSessionRuntime(localAssetStore(asset),asset.manifest.id,files,{
      sessionSeed:seed,signal,...capturingPlatformCallbacks(browserPlatformCallbacks,{resolution}),
      runtimeOptions:{canvas:$('canvas'),partyBoardGlyphTextures:glyphs.initial,print:line=>console.info(line),printErr:line=>console.warn(line)},
      progress:(bytes,total)=>{status(`Checking local game assets: ${Math.floor(bytes/total*100)}%`);},
    });
    localRuntime=runtime;
    gameTouchControls.refresh();
    if(controlEditor.customized('keyboard'))applyRuntimeKeyboardBindings(runtime.module,controlProfile);
    if(controlEditor.customized('controller'))applyRuntimeControllerBindings(runtime.module,controlProfile);
    speakers.bind(runtime.module);
    localGlyphs=glyphs.attach(runtime.module,$('canvas'));
    localBridge=installOnlineMenuHandoff(runtime,{
      openLobby:async mode=>{
        speakers.bind(undefined);
        localGlyphs?.close();localGlyphs=undefined;
        localRuntime=undefined;localBridge?.close();localBridge=undefined;
        $('game-view').hidden=true;gameTouchControls.refresh();$('play-local').disabled=false;
        display.setLocked(false);controlEditor.setLocked(false);
        if(mode==='host')await (manualLobby?$('host').onclick():openAutomaticLobby('host'));
        else if(manualLobby){status('Paste the host invitation, then choose Join.');$('remote').focus();}
        else {status('Open or paste the host’s lobby link to join.');$('join-link').focus();}
      },onError:error=>status(`Game shutdown failed: ${error.message}`),
    });
    status('Local game running. Online Multiplayer hands off to the Host/Join peer lobby.');
    $('canvas').focus();
    runtime.engine.resume().catch(async error=>{
      try{await stopLocalGame();}catch(cleanup){console.error(cleanup);}
      status(`Local game failed: ${error.message}`);
    });
  })();
  try{await localPreparing;}catch(error){
    console.error(error);
    if(localRuntime){
      localGlyphs?.close();localGlyphs=undefined;
      localBridge?.close();localBridge=undefined;
      try{await localRuntime.dispose();localRuntime=undefined;}catch(cleanup){console.error(cleanup);}
    }
    if(!signal.aborted)status(`Local game unavailable: ${error.message}`);
    $('game-view').hidden=true;gameTouchControls.refresh();$('play-local').disabled=false;
  }finally{localPreparing=undefined;if(!localRuntime){display.setLocked(false);controlEditor.setLocked(false);}}
};
let lobbyHost,statusChannel,lobbyClient,pendingSnapshot,reportSequence=0;
let guestCount=0,localPeer='guest',pendingPlayer;
const connections=new Set();
const guestConnections=new Map();
const routeStatus=document.createElement('p');routeStatus.id='connection-route';routeStatus.setAttribute('role','status');
$('lobby-players').before(routeStatus);
const routeMonitor=monitorConnectionRoutes({
  current:()=>lobbyHost?[...guestConnections].map(([peer,connection])=>({peer,label:peer,connection})):
    pc&&connections.has(pc)?[{peer:'host',label:'Host',connection:pc}]:[],
  changed:routes=>{routeStatus.textContent=routes.length?routes.map(item=>item.label+': '+connectionRouteLabel(item.route)).join(' · '):'Connection: not connected.';},
});
downloadCacheControls=cachedDownloadControls($('remove-download'),$('cache-download-status'),{
  available:()=>!pc&&!lobbyHost&&!lobbyRuntime&&!lobbyPreparing&&!localRuntime&&!localPreparing,
  settled:()=>lobbyCleanup,epoch:()=>generation,openStore:browserStore,status,
});
function dropGuest(playerId,error) {
  if(lobbyRecovery&&error){fail(error);return;}
  if(!error&&suspendLobbyConnection(playerId))return;
  const connection=guestConnections.get(playerId);if(!connection)return;
  guestConnections.delete(playerId);connections.delete(connection);
  recoveryCredentials.delete(playerId);
  preparedReports.delete(playerId);lobbyStartChannels.delete(playerId);lobbyStartOwner?.close();lobbyStartOwner=undefined;lobbyGameInput?.cancel();if(lobbyRuntime)lobbyRuntime.inputExchangeInstalled=false;lobbyInputChannels.delete(playerId);lobbyInputRouter?.close();lobbyInputRouter=undefined;
  if(pendingPlayer===playerId){pendingPlayer=undefined;$('cancel-invite').disabled=true;$('answer').disabled=true;}
  lobbyHost?.detach(playerId);connection.close();
  void automaticLobby?.remove(playerId).catch(cause=>status('Lobby cleanup: '+cause.message));
  $('host').disabled=!!pendingPlayer||guestConnections.size>=3;
  $('host').textContent='Invite next guest';
  status(`${playerId} left the lobby. Other downloads can continue.${error?' '+(error.message||String(error)):''}`);
}
let currentLobbySnapshot,currentLobbyPeer;
export function lobbyInputPlan(){return currentLobbySnapshot?lobbyInputOwnership(currentLobbySnapshot,currentLobbyPeer):null;}
const showLobby=(snapshot,peer)=>{if(lobbyGameInput&&snapshot.membership!==lobbyInputMembership){lobbyGameInput.cancel();if(lobbyRuntime)lobbyRuntime.inputExchangeInstalled=false;}currentLobbySnapshot=structuredClone(snapshot);currentLobbyPeer=peer;renderLobby($('lobby-players'),snapshot,peer);$('start-agreement').hidden=$('start-game').hidden=peer!=='host';$('start-agreement').disabled=peer!=='host'||!snapshot.canStart||!!lobbyStartOwner||!!lobbyRecovery;lobbyStartOwner?.refresh();};
const downloadReports=downloadProgressReporter((bytes,verified)=>{
  if(statusChannel?.readyState!=='open')return false;
  statusChannel.send(JSON.stringify({type:'lobby-report',version:1,
    report:{assetId:offer.id,sequence:++reportSequence,bytes,verified,shaders:false,ready:false}}));
});
function reportDownload(bytes,verified=false){downloadReports.report(bytes,offer.size,verified);}
function stop() {
  replacementNonces.clear();
  recoveryCredentials.clear();
  guestPipelineChannel=undefined;
  lobbyStartOwner?.close();lobbyStartOwner=undefined;clientStartPreparation=undefined;startPreparing=false;lobbyStartChannels.clear();originalRecoveryChannels.clear();
  lobbyInputRouter?.close();lobbyInputRouter=undefined;lobbyInputChannels.clear();
  currentLobbySnapshot=currentLobbyPeer=undefined;
  preparedReports.clear();guestPreparationChannel=undefined;
  releaseLobbyRuntime();
  sessionConfiguration=undefined;
  music.stop();
  generation++; aborter?.abort(); receiver?.cancel(); pc?.close(); store?.close();
  runtimeAsset=undefined;
  for(const connection of connections)connection.close();connections.clear();guestCount=0;
  guestConnections.clear();pendingPlayer=undefined;$('cancel-invite').disabled=true;
  $('file').disabled=false;$('host').textContent='Create host invitation';
  pc=receiver=store=undefined;
  void routeMonitor.refresh();
  lobbyHost?.close();lobbyHost=statusChannel=lobbyClient=pendingSnapshot=undefined;reportSequence=0;downloadReports.reset();
  $('lobby-players').replaceChildren();
  $('guest-progress').value=0;
  $('guest-download').textContent='No active guest transfer.';
  for(const id of ['accept','answer','copy'])$(id).disabled=true;
  $('local').value=''; $('host').disabled=$('join').disabled=false;
  downloadCacheControls.refresh();
}
function fail(error) {stop();status(error.message || String(error));}
function iceConfiguration(){
  return iceServerConfiguration({stun:$('stun').value,turn:$('turn').value,username:$('turn-username').value,credential:$('turn-credential').value});
}
function peer() {
  pc=new RTCPeerConnection({iceServers:iceConfiguration()});
  const connection=pc;connections.add(connection);
  downloadCacheControls.refresh();
  pc.onconnectionstatechange=()=>{
    if(['failed','disconnected'].includes(connection.connectionState)&&connections.has(connection)) {
      const entry=[...guestConnections].find(([,peer])=>peer===connection);
      if(suspendLobbyConnection(entry?.[0]||'host'))return;
      if(connection.connectionState!=='failed')return;
      if(entry)dropGuest(entry[0],Error('Connection failed.'));
      else fail(Error('Connection failed. Check the network or create a new invitation.'));
    }
  };
  $('host').disabled=$('join').disabled=true;
  return pc;
}
async function description(connection,playerId=localPeer) {
  if(connection.iceGatheringState!=='complete')await new Promise((resolve,reject)=>{
    const done=()=>{if(connection.iceGatheringState==='complete'){clearTimeout(timer);connection.removeEventListener('icegatheringstatechange',done);resolve();}};
    const timer=setTimeout(()=>{connection.removeEventListener('icegatheringstatechange',done);reject(Error('Connection setup timed out. Check the network options for a reachable STUN server or TURN relay, then try again.'));},15000);
    connection.addEventListener('icegatheringstatechange',done);done();
  });
  return btoa(JSON.stringify({version:1,playerId,description:connection.localDescription}));
}
function remote(type,value=$('remote').value) {
  const code=value.trim();
  if(code.length>100000)throw Error('Connection code is too large.');
  const parsed=JSON.parse(atob(code));
  if(parsed.version!==1 || parsed.description?.type!==type || typeof parsed.description.sdp!=='string')throw Error(`Expected a ${type} connection code.`);
  if(typeof parsed.playerId!=='string'||!/^guest(?:[1-9][0-9]{0,8})?$/.test(parsed.playerId))throw Error('Invalid invitation player');
  return parsed;
}
async function createLobbyInvitation(selectedFile){
  // Later guests must not unlock or detach the already prepared host runtime.
  if(localRuntime||localPreparing)await stopLocalGame();
  let current=generation;
  let attemptedPlayer,attemptedConnection;
  try {
    const file=selectedFile??$('file').files[0];if(!file)throw Error('Choose a local file first.');
    if(!lobbyHost){stop();current=generation;aborter=new AbortController();}
    $('file').disabled=true;
    if(guestConnections.size>=3||pendingPlayer)throw Error('Apply the pending reply or wait for an open slot.');
    $('host').disabled=$('join').disabled=true;
    status('Preparing integrity manifest…');
    if(asset?.file!==file)asset=await prepare(file,value=>{$('progress').value=value;},aborter.signal);
    if(current!==generation)return;
    let initialConfiguration;
    if(!lobbyHost){
      status('Checking runtime package before creating the lobby…');
      const [{loadBrowserRuntimePackage,loadSharedRuntimeInventory},{fingerprintRuntimeFiles}]=await Promise.all([
        import('./browser-runtime-package.mjs'),import('./runtime-build.mjs')]);
      const baseURL=new URL('./',location.href).href;
      const inventory=await loadSharedRuntimeInventory({baseURL,signal:aborter.signal});
      const files=await loadBrowserRuntimePackage({baseURL,signal:aborter.signal,supplementalArtifacts:inventory?.runtimeArtifacts??[]});
      const identity=await fingerprintRuntimeFiles(files,{signal:aborter.signal});
      if(inventory&&identity.buildId!==inventory.buildId)throw Error('Shared runtime files differ from the lobby website’s measured package.');
      if(current!==generation)return;
      initialConfiguration=createSessionConfiguration(asset.manifest.id,crypto,identity.buildId,inventory?.runtimeArtifacts);
    }
    const connection=peer();attemptedConnection=connection;
    const channel=connection.createDataChannel('partyboard-assets-v1',{ordered:true});
    if(!lobbyHost) {
    sessionConfiguration=initialConfiguration;
    const state=new LobbyState(asset.manifest.id,asset.manifest.size,{requireSave:false});state.join('host','Host');
    state.report('host',{assetId:asset.manifest.id,sequence:1,bytes:asset.manifest.size,verified:true,shaders:false,ready:false});
    lobbyHost=new LobbyHost(state,{changed:snapshot=>{if(current===generation)showLobby(snapshot,'host');},error:(playerId,error)=>{if(current===generation)dropGuest(playerId,error);}});
    }
    const playerId='guest'+(++guestCount);
    attemptedPlayer=playerId;
    pendingPlayer=playerId;
    $('cancel-invite').disabled=false;
    guestConnections.set(playerId,connection);
    recoveryCredentials.set(playerId,createRecoveryCredential(sessionConfiguration,playerId));
    const pipelineChannel=connection.createDataChannel('partyboard-pipelines-v1',{ordered:true});
    pipelineChannel.onopen=()=>{if(current===generation&&lobbyPipelineSeed)sendPipelineSeed(pipelineChannel,lobbyPipelineSeed,{signal:aborter.signal}).catch(error=>{
      if(guestConnections.get(playerId)!==connection)return;
      // Native guests decline this stream and use their locally verified bundle.
      // Closing this optional channel grants no readiness: their actual prepared
      // receipt must still match the host's seed, runtime and frame-zero state.
      if(['closing','closed'].includes(pipelineChannel.readyState)){status('Shader transfer closed. Waiting for the guest’s verified preparation report.');return;}
      dropGuest(playerId,error);
    });};
    const startChannel=connection.createDataChannel('partyboard-start-v1',{ordered:true});
    lobbyStartChannels.set(playerId,startChannel);
    const inputChannel=connection.createDataChannel('partyboard-input-v1',{ordered:true});
    inputChannel.binaryType='arraybuffer';lobbyInputChannels.set(playerId,inputChannel);
    const preparationChannel=connection.createDataChannel('partyboard-preparation-v1',{ordered:true});
    preparationChannel.onmessage=message=>{
      if(current!==generation||guestConnections.get(playerId)!==connection)return;
      try{const report=receivePreparedRuntimeReport(message.data,sessionConfiguration,preparedReports.get(playerId));
        preparedReports.set(playerId,report);checkPreparedAgreement();
      }catch(error){dropGuest(playerId,error);}
    };
    preparationChannel.addEventListener('open',()=>{
      if(current===generation&&guestConnections.get(playerId)===connection)
        {preparationChannel.send(JSON.stringify(sessionConfiguration));preparationChannel.send(JSON.stringify(recoveryCredentials.get(playerId)));}
    });
    const lobbyChannel=connection.createDataChannel('partyboard-lobby-v1',{ordered:true});
    lobbyHost.attach(playerId,guestCount===1?'Guest':`Guest ${guestCount}`,lobbyChannel);
    lobbyChannel.addEventListener('close',()=>{if(current===generation&&guestConnections.get(playerId)===connection)dropGuest(playerId);});
    $('file').disabled=true;
    host(channel,asset,error=>{if(current===generation&&guestConnections.get(playerId)===connection)dropGuest(playerId,error);},()=>{if(current===generation&&guestConnections.get(playerId)===connection)status('Guest reports the asset is verified and cached.');},(bytes,total)=>{if(current===generation&&guestConnections.get(playerId)===connection){$('guest-progress').value=bytes/total;$('guest-download').textContent=`Guest: `+Math.floor(bytes/total*100)+`% · `+(bytes/1024**2).toFixed(1)+` / `+(total/1024**2).toFixed(1)+` MiB verified`;}});
    channel.onopen=()=>{if(current===generation)status(manualLobby?'Guest connected. Waiting for the guest to accept the file.':'Guest connected. Starting the verified asset transfer…');};
    await connection.setLocalDescription(await connection.createOffer());
    const code=await description(connection,playerId);if(current!==generation||pendingPlayer!==playerId)return;
    $('local').value=code;$('copy').disabled=$('answer').disabled=false;
    status(manualLobby?'Send this invitation privately. Paste the guest’s reply, then apply it.':'Connecting a guest…');
    return code;
  }catch(error){
    if(current!==generation||(attemptedPlayer&&!guestConnections.has(attemptedPlayer)))return;
    if(!lobbyHost){fail(error);throw error;}
    // A failed new invitation must not cancel existing guest transfers.
    if(attemptedPlayer)dropGuest(attemptedPlayer);
    else if(attemptedConnection){connections.delete(attemptedConnection);attemptedConnection.close();}
    $('host').disabled=!!pendingPlayer||guestConnections.size>=3;
    $('host').textContent='Invite next guest';
    $('local').value='';$('copy').disabled=$('answer').disabled=true;
    status(`Invitation failed. Existing guests remain connected. ${error.message||String(error)}`);throw error;
  }
}
$('host').onclick=()=>createLobbyInvitation().catch(()=>{});
async function joinLobbyInvitation(invitationCode){
  await stopLocalGame();
  let current=generation;
  try {
    const invitation=remote('offer',invitationCode);stop();lobbyPipelineSeed=undefined;current=generation;aborter=new AbortController();localPeer=invitation.playerId;music.start();
    const connection=peer();const openedStore=await browserStore();
    if(current!==generation){openedStore.close();return;}store=openedStore;
    const guard=fn=>(...args)=>{if(current===generation&&pc===connection)return fn(...args);};
    connection.ondatachannel=event=>{
      if(current!==generation){event.channel.close();return;}
      if(event.channel.label==='partyboard-recovery-v1'){
        if(originalRecoveryChannels.has('host')){event.channel.close();return;}
        const channel=event.channel;originalRecoveryChannels.set('host',channel);
        channel.onmessage=guard(message=>{try{
          if(typeof message.data!=='string'||message.data.length>2048)throw Error('Invalid original recovery packet.');
          if(!receiveRunningRecovery('host',channel,JSON.parse(message.data)))throw Error('Unexpected original recovery packet.');
        }catch(error){if(current===generation)fail(error);}});return;
      }
      if(event.channel.label==='partyboard-pipelines-v1'){
        if(guestPipelineChannel){event.channel.close();return;}guestPipelineChannel=event.channel;
        receivePipelineSeed(event.channel,{signal:aborter.signal,complete:seed=>{if(current===generation)setLobbyPipelineSeed(seed);},error:guard(fail)});return;
      }
      if(event.channel.label==='partyboard-start-v1'){
        if(lobbyStartChannels.has('host')){event.channel.close();return;}
        const channel=event.channel;lobbyStartChannels.set('host',channel);
        channel.onmessage=guard(async message=>{
          try{
            if(startPreparing||typeof message.data!=='string'||message.data.length>2048)throw Error('Unexpected start packet.');
            const proposal=JSON.parse(message.data);
            if(receiveRunningRecovery('host',channel,proposal))return;
            if(proposal.type==='session-input-loss'){
              if(!lobbyRecovery)throw Error('Input loss before committed start.');
              const peer=receiveSessionInputLoss(proposal,{session:sessionConfiguration.session,plan:lobbyRecoveryPlan});
              suspendLobbyConnection(peer,false);return;
            }
            if(proposal.type==='session-start-commit'){
              if(!clientStartPreparation)throw Error('Unacknowledged start commit.');
              runCommittedLobbyGame(proposal,clientStartPreparation,localPeer);return;
            }
            if(clientStartPreparation)throw Error('Duplicate start proposal.');
            if(!lobbyRuntime||proposal.session!==sessionConfiguration?.session||proposal.seed!==sessionConfiguration.seed)throw Error('Start session differs from preparation.');
            if(!lobbyGameInput)connectLobbyGameInput();startPreparing=true;
            const prepared=await prepareStartAcknowledgment(proposal,{peer:localPeer,buildId:lobbyRuntime.buildId,
              snapshot:()=>currentLobbySnapshot,prepare:async()=>lobbyRuntime,signal:aborter.signal,requireOnlineInput:true});
            if(current!==generation)return;
            clientStartPreparation=prepared;channel.send(JSON.stringify(prepared.acknowledgment));
          }catch(error){if(current===generation)fail(error);}finally{startPreparing=false;}
        });return;
      }
      if(event.channel.label==='partyboard-input-v1'){
        if(lobbyInputChannels.has('host')){event.channel.close();return;}
        event.channel.binaryType='arraybuffer';lobbyInputChannels.set('host',event.channel);return;
      }
      if(event.channel.label==='partyboard-preparation-v1') {
        if(guestPreparationChannel){event.channel.close();return;}
        guestPreparationChannel=event.channel;
        event.channel.onmessage=guard(message=>{try{
          if(typeof message.data==='string'&&message.data.length<=2048){
            const packet=JSON.parse(message.data);
            if(packet.type==='recovery-credential'){
              if(!sessionConfiguration)throw Error('Recovery credential before session configuration.');
              recoveryCredentials.set(localPeer,receiveRecoveryCredential(packet,{session:sessionConfiguration.session,assetId:sessionConfiguration.assetId,peer:localPeer,current:recoveryCredentials.get(localPeer)}));return;
            }
          }
          sessionConfiguration=receiveSessionConfiguration(message.data,{assetId:offer?.id,current:sessionConfiguration});
        }catch(error){fail(error);}});
        return;
      }
      if(event.channel.label==='partyboard-lobby-v1'&&!statusChannel) {
        statusChannel=event.channel;
        statusChannel.onmessage=guard(message=>{try {
          if(!lobbyClient){if(typeof message.data!=='string'||message.data.length>16384)throw Error('Invalid lobby snapshot');pendingSnapshot=message.data;return;}
          const snapshot=lobbyClient.receive(message.data);if(snapshot)showLobby(snapshot,localPeer);
        }catch(error){fail(error);}});
        return;
      }
      if(event.channel.label!=='partyboard-assets-v1' || receiver){event.channel.close();return;}
      receiver=guest(event.channel,store,{
        offer:guard(m=>{downloadCacheControls.remember(m);if(sessionConfiguration)validateSessionConfiguration(sessionConfiguration,m.id);offer=m;lobbyClient=new LobbyClient(m.id,m.size);if(pendingSnapshot){const snapshot=lobbyClient.receive(pendingSnapshot);if(snapshot)showLobby(snapshot,localPeer);pendingSnapshot=undefined;}$('offer').textContent=`${m.name} · ${(m.size/1024**2).toFixed(1)} MiB`;$('accept').disabled=false;status(manualLobby?'Connected. Accept to store this asset in your browser.':'Connected. Preparing the verified asset download…');}),
        progress:guard((bytes,total)=>{reportDownload(bytes);$('progress').value=bytes/total;status(`Verified ${(bytes/1024**2).toFixed(1)} / ${(total/1024**2).toFixed(1)} MiB`);}),
        complete:guard(async m=>{
          const signal=aborter.signal;
          try {
            status('Checking cached assets before runtime loading…');
            const manifest=await verifyStoredAsset(store,m.id,{signal,progress:(bytes,total)=>{
              if(current===generation)status(`Checking cached assets: ${Math.floor(bytes/total*100)}%`);
            }});
            // Preflight does not allocate a gameplay copy. Runtime preparation
            // verifies its own fresh mount and owns that mount's temporary file.
            if(current!==generation)return;
            runtimeAsset={manifest};
            reportDownload(m.size,true);
            status(manualLobby?`Assets checked and prepared. Asset ID: ${manifest.id}\nChoose Prepare lobby game, then mark ready to join the agreed start. Additional shader scene coverage remains under development.`:'Game assets verified. Preparing the runtime and known shader pipelines…');
        }catch(error){if(current===generation&&!signal.aborted)fail(error);}
        }),error:guard(fail),
      });
    event.channel.addEventListener('close',guard(()=>{if(pc!==connection)return;if(suspendLobbyConnection('host'))return;recoveryCredentials.clear();aborter?.abort();releaseLobbyRuntime();clientStartPreparation=undefined;lobbyStartChannels.clear();originalRecoveryChannels.clear();lobbyInputRouter?.close();lobbyInputRouter=undefined;lobbyInputChannels.clear();currentLobbySnapshot=currentLobbyPeer=undefined;preparedReports.clear();guestPreparationChannel=undefined;sessionConfiguration=undefined;runtimeAsset=undefined;music.stop();status('Connection closed. Verified chunks are retained for resume.');}));
    };
    await connection.setRemoteDescription(invitation.description);
    await connection.setLocalDescription(await connection.createAnswer());
    const code=await description(connection);if(current!==generation)return;
    $('local').value=code;$('copy').disabled=false;status(manualLobby?'Send this reply privately to the host.':'Connecting to the host…');return code;
  }catch(error){if(current===generation)fail(error);throw error;}
}
$('join').onclick=()=>joinLobbyInvitation().catch(()=>{});
async function applyLobbyAnswer(code){const answer=remote('answer',code);if(answer.playerId!==pendingPlayer)throw Error('Reply belongs to another invitation');await pc.setRemoteDescription(answer.description);pendingPlayer=undefined;$('cancel-invite').disabled=true;$('answer').disabled=true;$('host').disabled=guestConnections.size>=3;$('host').textContent=guestConnections.size>=3?'Lobby invitations full':'Invite next guest';status('Connecting. You can invite another guest while downloads continue.');return answer.playerId;}
$('answer').onclick=()=>applyLobbyAnswer().catch(error=>status(error.message||String(error)));
async function acceptLobbyAsset(){const current=generation;try{
  if(statusChannel?.readyState!=='open'){status('Waiting for lobby connection. Try Accept download again shortly.');return;}
  $('accept').disabled=true;
  const estimate=await navigator.storage.estimate();
  if(current!==generation)return;
  if(estimate.quota && estimate.quota-estimate.usage < offer.size*1.1)throw Error('Insufficient browser storage. Free some site data before downloading.');
  await receiver.accept();
}catch(error){if(current===generation)fail(error);throw error;}}
$('accept').onclick=()=>acceptLobbyAsset().catch(()=>{});
$('stop').onclick=async()=>{try{await closeAutomaticLobby();await stopLocalGame();stop();await lobbyCleanup;status('Session stopped. Verified cached chunks are retained.');}catch(error){status(error.message);}};
$('cancel-invite').onclick=()=>{
  if(!pendingPlayer)return;
  dropGuest(pendingPlayer);
  $('local').value='';$('copy').disabled=true;$('answer').disabled=true;
  status('Invitation cancelled. Existing guests remain connected; you can invite another guest.');
};
$('copy').onclick=async()=>{try{await navigator.clipboard.writeText($('local').value);status('Connection code copied.');}catch{status('Select and copy the connection code manually.');}};

const manualLobby=new URL(location.href).searchParams.get('manual')==='1';
for(const element of document.querySelectorAll('[data-manual-lobby]'))element.hidden=!manualLobby;
$('automatic-lobby').hidden=manualLobby;
if(['localhost','127.0.0.1','[::1]'].includes(location.hostname)){
  $('room-service-note').textContent='Local test service: these lobby links work only on this computer. Remote players will need the shared HTTPS lobby website, which is not deployed yet.';
}
let automaticLobby,automaticPrep,automaticTimer,automaticGeneration,automaticStartBusy=false;
let automaticSetup=0,automaticSetupAbort;
const automaticRecoveryOwners=new Map(),automaticRecoveryAttempts=new Set();
let automaticRecoveryBusy=false,automaticResumeAttempted=false;
async function finishAutomaticReplacement(peer){
  const owner=automaticRecoveryOwners.get(peer);if(!owner)throw Error('Owned automatic reconnect required.');
  try{await owner.ready();const transferred=owner.handoff();const receipt=await verifyLobbyReplacementState(transferred);adoptLobbyReplacementState(receipt);}
  finally{if(automaticRecoveryOwners.get(peer)===owner)automaticRecoveryOwners.delete(peer);}
}
async function automaticRecoveryRefresh(){
  if(!lobbyHost||!lobbyRecovery||!lobbyGameInput||automaticRecoveryBusy)return;
  const policy=lobbyGameInput.queue.status(performance.now());
  if(policy.state==='Running'){automaticRecoveryAttempts.clear();automaticResumeAttempted=false;return;}
  if(policy.state!=='Paused')return;automaticRecoveryBusy=true;
  try{
    for(const [peer,connection] of guestConnections){
      if(connection.connectionState==='connected'&&lobbyInputChannels.get(peer)?.readyState==='open'&&lobbyStartChannels.get(peer)?.readyState==='open')continue;
      if(automaticRecoveryAttempts.has(peer))continue;automaticRecoveryAttempts.add(peer);
      await automaticLobby.recover(peer);
    }
    if(!automaticRecoveryOwners.size&&!automaticResumeAttempted&&[...guestConnections].every(([peer,connection])=>connection.connectionState==='connected'&&lobbyInputChannels.get(peer)?.readyState==='open'&&lobbyStartChannels.get(peer)?.readyState==='open')){
      automaticResumeAttempted=true;await beginLobbyRecoveryAgreement();
    }
  }catch(cause){status('Automatic reconnect: '+cause.message+' The original 60-second recovery deadline still applies.');}
  finally{automaticRecoveryBusy=false;}
}
async function closeAutomaticLobby(){
  automaticSetup++;automaticSetupAbort?.abort();automaticSetupAbort=undefined;
  clearInterval(automaticTimer);automaticTimer=undefined;automaticPrep?.close();automaticPrep=undefined;
  const owner=automaticLobby;automaticLobby=undefined;automaticGeneration=undefined;
  if(owner)aborter?.abort();
  for(const replacement of automaticRecoveryOwners.values())replacement.close();automaticRecoveryOwners.clear();automaticRecoveryAttempts.clear();automaticResumeAttempted=false;
  $('host-game').disabled=$('join-game').disabled=false;$('automatic-start').disabled=true;
  await owner?.close();
}
async function automaticRefresh(owner){
  if(automaticLobby!==owner)return;
  if(automaticGeneration!==undefined&&automaticGeneration!==generation){await closeAutomaticLobby();return;}
  await automaticPrep?.refresh();
  if(automaticLobby!==owner)return;
  await automaticRecoveryRefresh();
  $('automatic-start').disabled=!lobbyHost||!currentLobbySnapshot?.canStart||!!lobbyRecovery||automaticStartBusy||!!lobbyCommittedIdentity;
}
async function openAutomaticLobby(mode){
  let request;
  try{
    if(mode==='host'&&!$('file').files[0])throw Error('Choose your game file before hosting.');
    const joinLink=$('join-link').value.trim();
    const hostFile=$('file').files[0];
    if(mode==='guest'&&!joinLink)throw Error('Paste the host’s lobby link first.');
    await closeAutomaticLobby();await stopLocalGame();stop();
    request=automaticSetup;automaticSetupAbort=new AbortController();
    $('host-game').disabled=$('join-game').disabled=true;$('automatic-start').hidden=mode!=='host';
    $('lobby-link').value='';
    if(mode==='host'){
      status('Checking bundled shader definitions…');
      const seed=await loadBundledPipelineSeed({baseURL:location.href,signal:automaticSetupAbort.signal});
      if(request!==automaticSetup)return;setLobbyPipelineSeed(seed);
    }
    const owner=automaticRoom({baseURL:location.href,pageURL:new URL('./',location.href).href,
      createOffer:async()=>{const code=await createLobbyInvitation(hostFile);automaticGeneration=generation;return code;},
      answerOffer:async code=>{const reply=await joinLobbyInvitation(code);automaticGeneration=generation;return reply;},
      applyAnswer:applyLobbyAnswer,
      createRecoveryOffer:async peer=>{const replacement=createLobbyReplacementTransport(peer);automaticRecoveryOwners.set(peer,replacement);return JSON.stringify(await replacement.createOffer());},
      answerRecoveryOffer:async payload=>{suspendLobbyConnection('host');const replacement=createLobbyReplacementTransport('host');automaticRecoveryOwners.set('host',replacement);return JSON.stringify(await replacement.acceptOffer(JSON.parse(payload)));},
      applyRecoveryAnswer:async(peer,payload)=>{const replacement=automaticRecoveryOwners.get(peer);if(!replacement)throw Error('No pending automatic reconnect.');await replacement.acceptAnswer(JSON.parse(payload));await finishAutomaticReplacement(peer);},
      recoveryAnswerSent:()=>finishAutomaticReplacement('host'),
      cancelOffer:()=>{if(pendingPlayer)dropGuest(pendingPlayer);},
      left:peer=>{if(!lobbyCommittedIdentity)dropGuest(peer);},
      changed:state=>{
        if(automaticLobby!==owner)return;
        if(state.link){$('lobby-link').value=state.link;status('Lobby open. Share the join link with up to three guests. Game files transfer directly between players.');}
        else status(state.connected?'Connected. Download and shader preparation will run automatically.':'Joining lobby. Waiting for the host connection…');
      },error:cause=>{if(automaticLobby===owner)status('Lobby setup: '+cause.message);},
    });automaticLobby=owner;
    automaticPrep=automaticPreparation({
      state:()=>{
        const peer=lobbyHost?'host':localPeer,player=currentLobbySnapshot?.players.find(p=>p.peer===peer);
        return {running:!!lobbyCommittedIdentity,canAccept:!!offer&&!!receiver&&statusChannel?.readyState==='open',
          assetsVerified:!!lobbyHost||!!runtimeAsset,configuration:!!sessionConfiguration,seedVerified:!!lobbyPipelineSeed,
          prepared:!!lobbyRuntime&&!lobbyPreparing,assignedVerified:!!player?.verified,ready:!!player?.ready,membership:currentLobbySnapshot?.membership};
      },accept:acceptLobbyAsset,prepare:prepareLobbyGame,ready:()=>{const receipt=markLobbyReady();status('Ready: '+receipt.created+' known shader pipelines prepared. Waiting for the host to start.');},
      error:cause=>status('Automatic game preparation failed: '+cause.message+' Stop the session and try again.'),
    });
    if(mode==='host'){await owner.host();if(automaticLobby===owner)$('file').disabled=true;}else await owner.join(joinLink);
    if(automaticLobby===owner)automaticTimer=setInterval(()=>{void automaticRefresh(owner).catch(cause=>status(cause.message));},250);
  }catch(cause){if(request!==undefined&&request!==automaticSetup)return;await closeAutomaticLobby().catch(()=>{});stop();status(cause.message);}
}
$('host-game').onclick=()=>openAutomaticLobby('host');
$('join-game').onclick=()=>openAutomaticLobby('guest');
$('copy-link').onclick=async()=>{try{if(!$('lobby-link').value)throw Error('Create a lobby first.');await navigator.clipboard.writeText($('lobby-link').value);status('Lobby link copied. Share it privately with your guests.');}catch(cause){status(cause.message);}};
$('automatic-start').onclick=async()=>{
  if(automaticStartBusy)return;automaticStartBusy=true;$('automatic-start').disabled=true;
  const original=generation;
  try{
    await automaticLobby.seal();
    beginLobbyStartAgreement();status('Confirming the synchronized start with every player…');
    const deadline=performance.now()+15000;
    while(generation===original&&lobbyStartOwner&&lobbyStartBarrier?.state!=='Agreed'){
      if(performance.now()>deadline)throw Error('Players did not confirm the start in time. Stop the session and reconnect.');
      await new Promise(resolve=>setTimeout(resolve,50));
    }
    if(generation!==original)throw Error('The lobby closed before the start.');
    commitLobbyStart();status('Online game running.');
  }catch(cause){status(cause.message);}finally{automaticStartBusy=false;}
};
function loadLobbyLink(){if(!manualLobby&&!automaticLobby&&new URLSearchParams(location.hash.slice(1)).has('room')){
  $('join-link').value=location.href;status('Lobby link loaded. Choose Join game to receive the host’s game assets and prepare this device automatically.');
}}
addEventListener('hashchange',loadLobbyLink);loadLobbyLink();

