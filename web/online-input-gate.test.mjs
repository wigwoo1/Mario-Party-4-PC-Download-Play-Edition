import {test} from 'node:test';
import assert from 'node:assert/strict';
import {requirePreparedOnlineInput} from './online-input-gate.mjs';
test('online start needs installation receipt, live callbacks and C requirement',()=>{
  const ready=()=>({inputExchangeInstalled:true,module:{partyBoardInputExchange:()=>{},
    partyBoardCancelInputExchange:()=>{},_PartyBoardBrowserOnlineInputRequired:()=>1}});
  requirePreparedOnlineInput(ready(),true);
  for(const change of ['receipt','exchange','cancel','export','disabled','released']) {
    const runtime=ready();
    if(change==='receipt')runtime.inputExchangeInstalled=false;
    if(change==='exchange')delete runtime.module.partyBoardInputExchange;
    if(change==='cancel')delete runtime.module.partyBoardCancelInputExchange;
    if(change==='export')delete runtime.module._PartyBoardBrowserOnlineInputRequired;
    if(change==='disabled')runtime.module._PartyBoardBrowserOnlineInputRequired=()=>0;
    if(change==='released')runtime.module=undefined;
    assert.throws(()=>requirePreparedOnlineInput(runtime,true),/online input hook/);
  }
  requirePreparedOnlineInput(undefined,false);
  assert.throws(()=>requirePreparedOnlineInput(ready(),'true'),/Boolean/);
});
