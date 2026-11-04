const {test}=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm');
const {installVirtualStandardGamepad}=require('./virtual-standard-gamepad.cjs');
test('standard gamepad fixture exposes press/release and restores the original provider',()=>{
 const original=()=>['physical'];const context=vm.createContext({navigator:{getGamepads:original},performance:{now:()=>123}});
 vm.runInContext('('+installVirtualStandardGamepad.toString()+')()',context);
 const fixture=context.testVirtualStandardGamepad;assert.equal(context.navigator.getGamepads()[0].mapping,'standard');assert.equal(fixture.snapshot().buttons.length,17);
 fixture.setButton(0,true);assert.equal(context.navigator.getGamepads()[0].buttons[0].value,1);assert.equal(fixture.snapshot().timestamp,123);
 fixture.setButton(0,false);assert.equal(context.navigator.getGamepads()[0].buttons[0].pressed,false);
 assert.throws(()=>fixture.setButton(17,true),/Invalid/);assert.throws(()=>fixture.setButton(0,1),/Invalid/);
 fixture.close();assert.deepEqual(context.navigator.getGamepads(),['physical']);assert.equal(context.testVirtualStandardGamepad,undefined);
});
