import {compileKeyboardBindings,compileControllerBindings} from './control-bindings.mjs';
export function applyRuntimeControllerBindings(module,profile){
 const plan=compileControllerBindings(profile);
 if(typeof module._PartyBoardBrowserApplyControllerBindings!=='function'||typeof module._malloc!=='function'||typeof module._free!=='function')throw Error('Runtime controller customization unavailable.');
 const words=Int32Array.from([...plan.buttons.flatMap(b=>[b.nativeButton,b.padButton]),...plan.axes.flatMap(b=>[b.nativeAxis.nativeAxis,b.nativeAxis.sign,b.nativeButton,b.padAxis])]);
 const pointer=module._malloc(words.byteLength);if(!pointer)throw Error('Control binding allocation failed.');
 try{
  module.HEAPU8.set(new Uint8Array(words.buffer,words.byteOffset,words.byteLength),pointer);
  const result=module._PartyBoardBrowserApplyControllerBindings(pointer,words.length);
  if(result===-3)throw Error('Connect your controller before preparing the game, then try again.');
  if(result!==0)throw Error('Runtime rejected controller bindings: '+result);
 }finally{module._free(pointer);}
}
export function applyRuntimeKeyboardBindings(module,profile){
 const plan=compileKeyboardBindings(profile);
 if(typeof module._PartyBoardBrowserApplyKeyboardBindings!=='function'||typeof module._malloc!=='function'||typeof module._free!=='function')throw Error('Runtime control customization unavailable.');
 const words=Int32Array.from([...plan.buttons.flatMap(b=>[b.scancode,b.padButton]),...plan.axes.flatMap(b=>[b.scancode,b.padAxis,b.influence])]);
 const pointer=module._malloc(words.byteLength);if(!pointer)throw Error('Control binding allocation failed.');
 try{
  module.HEAPU8.set(new Uint8Array(words.buffer,words.byteOffset,words.byteLength),pointer);
  const result=module._PartyBoardBrowserApplyKeyboardBindings(pointer,words.length);
  if(result!==0)throw Error('Runtime rejected keyboard bindings: '+result);
 }finally{module._free(pointer);}
}
