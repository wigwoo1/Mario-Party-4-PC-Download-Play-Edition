import test from 'node:test';
import assert from 'node:assert/strict';
import {preparationFailureMessage} from './preparation-error.mjs';

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
