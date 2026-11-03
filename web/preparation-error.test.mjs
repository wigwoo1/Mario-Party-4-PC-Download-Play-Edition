import test from 'node:test';
import assert from 'node:assert/strict';
import {preparationFailureMessage,retainPreparationLog} from './preparation-error.mjs';

test('browser quota failure gives actionable storage guidance through cleanup wrappers',()=>{
  const quota=new DOMException('Browser-specific quota wording','QuotaExceededError');
  for(const error of [quota,new Error('Mount failed',{cause:quota}),
    new AggregateError([new Error('Cleanup busy'),new Error('Verification failed',{cause:quota})],'Verification and cleanup failed')]){
    const message=preparationFailureMessage(error,{automatic:true});
    assert.match(message,/^Automatic game preparation failed:/);
    assert.match(message,/free space for this site/);
    assert.match(message,/regular browser window and rejoin/);
    assert.match(message,/not removed automatically/);
    assert.doesNotMatch(message,/try again/);
  }
});

test('unrelated failures retain their actual message and cyclic causes terminate',()=>{
  const error=new Error('Shader pipeline rejected');error.cause=error;
  assert.equal(preparationFailureMessage(error),'Lobby game preparation failed: Shader pipeline rejected');
  assert.equal(preparationFailureMessage(error,{automatic:true}),
    'Automatic game preparation failed: Shader pipeline rejected Stop the session and try again.');
  assert.doesNotMatch(preparationFailureMessage(new Error('Quota mentioned by unrelated error')),/free space/);
});

test('wrapped preparation and cleanup failures expose their underlying messages',()=>{
  const failure=new AggregateError([
    new Error('Runtime module initialization failed'),
    new Error('Runtime shutdown failed while Asyncify was active'),
  ],'Session preparation and cleanup failed.');
  assert.equal(preparationFailureMessage(failure,{automatic:true}),
    'Automatic game preparation failed: Session preparation and cleanup failed. Details: Runtime module initialization failed Details: Runtime shutdown failed while Asyncify was active Stop the session and try again.');
});

test('captured platform error logs are included without leaking resource URLs',()=>{
  const failure=new Error('RuntimeError: Aborted (native code called abort())');
  failure.partyBoardRuntimeLogs=[
    'aurora: Aurora initializing',
    'aurora: Error creating window: adapter unavailable at https://example.invalid/private',
  ];
  const message=preparationFailureMessage(failure,{automatic:true});
  assert.match(message,/Graphics log: aurora: Error creating window: adapter unavailable/);
  assert.doesNotMatch(message,/example\.invalid|https:\/\//);
});

test('late renderer failure survives verbose shader preparation in the bounded log',()=>{
  const logs=[];
  for(let i=0;i<300;i++)retainPreparationLog(logs,'Pipeline compiled '+i);
  retainPreparationLog(logs,'aurora: Cannot begin frame: renderer frame resources are unavailable');
  retainPreparationLog(logs,'   ');
  assert.equal(logs.length,128);
  const error=new Error('Browser platform operation failed: PartyBoardBrowserAdvancePreparedRenderer (-2)');
  error.partyBoardRuntimeLogs=logs.slice(-32);
  assert.match(preparationFailureMessage(error,{automatic:true}),
    /Graphics log: aurora: Cannot begin frame: renderer frame resources are unavailable/);
  assert.ok(logs.every(line=>line.length<=500));
  retainPreparationLog(logs,'warning: '+'x'.repeat(2000));
  assert.equal(logs.length,128);
  assert.equal(logs.at(-1).length,500);
});
