import {test} from 'node:test';
import assert from 'node:assert/strict';
import {installBrowserInputExchange} from './browser-input-exchange.mjs';

test('exchange timing counts completed polls and excludes cancellation',async()=>{
 const module={_PartyBoardBrowserRequireOnlineInput(){}};let clock=10,finish;
 const owner=installBrowserInputExchange(module,()=>new Promise(resolve=>finish=resolve),()=>clock);
 const pending=module.partyBoardInputExchange(new Uint8Array(44));await Promise.resolve();clock=35;
 assert.equal(owner.timing().pending,true);assert.equal(owner.timing().pendingExchangeMs,25);
 finish(new Uint8Array(44));await pending;
 assert.deepEqual(owner.timing(),{completedPolls:1,lastExchangeMs:25,totalExchangeMs:25,maxExchangeMs:25,pending:false,pendingExchangeMs:0});
 const snapshot=owner.timing();snapshot.completedPolls=99;assert.equal(owner.timing().completedPolls,1);
 const cancelled=module.partyBoardInputExchange(new Uint8Array(44));await Promise.resolve();clock=100;owner.cancel();assert.equal(await cancelled,null);
 assert.equal(owner.timing().completedPolls,1);assert.equal(owner.timing().pending,false);assert.equal(owner.timing().pendingExchangeMs,0);owner.close();
});
test('checkpoint metadata is copied and preserves unsigned maximum',async()=>{
 const module={_PartyBoardBrowserRequireOnlineInput(){}};let seen;
 const owner=installBrowserInputExchange(module,(pads,options)=>{seen=options.checkpoint;return pads;});
 const metadata={checkpoint:0xffffffff},wait=module.partyBoardInputExchange(new Uint8Array(44),metadata);metadata.checkpoint=0;
 await wait;assert.equal(seen,0xffffffff);
 await assert.rejects(module.partyBoardInputExchange(new Uint8Array(44),{checkpoint:-1}),/checkpoint/);owner.close();
});
test('cancel releases pending input even when provider ignores abort',async()=>{
  const module={_PartyBoardBrowserRequireOnlineInput:value=>assert.equal(value,1)};
  let signal;const owner=installBrowserInputExchange(module,async(input,options)=>{signal=options.signal;return new Promise(()=>{});});
  const pending=module.partyBoardInputExchange(new Uint8Array(44));await Promise.resolve();owner.cancel();
  assert.equal(await pending,null);assert.ok(signal.aborted);owner.close();assert.equal(module.partyBoardInputExchange,undefined);
});
test('pads copied across provider boundary and malformed/overlapping samples reject',async()=>{
  const module={_PartyBoardBrowserRequireOnlineInput(){}};let finish;
  const owner=installBrowserInputExchange(module,()=>new Promise(resolve=>finish=resolve));
  const pending=module.partyBoardInputExchange(new Uint8Array(44));await Promise.resolve();
  await assert.rejects(module.partyBoardInputExchange(new Uint8Array(44)),/overlapping/);
  const result=new Uint8Array(44);finish(result);const merged=await pending;result[0]=3;assert.equal(merged[0],0);
  await assert.rejects(module.partyBoardInputExchange(new Uint8Array(43)),/Invalid/);owner.close();
});
