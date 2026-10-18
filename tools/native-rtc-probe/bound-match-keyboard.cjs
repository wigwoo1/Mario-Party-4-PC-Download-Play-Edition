'use strict';
// Keep the match driver's game actions stable while sending the user's keys.
function createBoundMatchKeyboard(keyboard, mapping) {
 const resolve = key => Object.hasOwn(mapping, key) ? mapping[key] : key;
 return {
  press: (key, options) => keyboard.press(resolve(key), options),
  down: key => keyboard.down(resolve(key)),
  up: key => keyboard.up(resolve(key)),
 };
}
function browserMatchMapping({keyboard}) {
 const actions={z:'a',x:'b',c:'x',v:'y',q:'l',e:'r',r:'z',Enter:'start',
  w:'stickUp',s:'stickDown',a:'stickLeft',d:'stickRight',
  ArrowUp:'up',ArrowDown:'down',ArrowLeft:'left',ArrowRight:'right',i:'cUp',k:'cDown',j:'cLeft',l:'cRight'};
 return Object.fromEntries(Object.entries(actions).map(([key,action])=>{
  const code=keyboard[action];if(typeof code!=='string')throw Error('Missing browser match binding '+action);
  return [key,/^Key[A-Z]$/.test(code)?code.slice(3).toLowerCase():code];
 }));
}
module.exports = {createBoundMatchKeyboard,browserMatchMapping};
