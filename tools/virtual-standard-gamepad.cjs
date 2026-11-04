'use strict';
// Test-only browser API fixture. All game input still passes through SDL.
function installVirtualStandardGamepad(){
 const original=navigator.getGamepads?.bind(navigator);
 const pad={index:0,id:'DualSense Wireless Controller (test fixture)',mapping:'standard',connected:true,timestamp:0,buttons:Array.from({length:17},()=>({pressed:false,touched:false,value:0})),axes:[0,0,0,0]};
 Object.defineProperty(navigator,'getGamepads',{configurable:true,value:()=>pad.connected?[pad]:[]});
 globalThis.testVirtualStandardGamepad={
  setButton(index,pressed){if(!Number.isInteger(index)||index<0||index>=pad.buttons.length||typeof pressed!=='boolean')throw TypeError('Invalid virtual button');pad.buttons[index]={pressed,touched:pressed,value:pressed?1:0};pad.timestamp=performance.now();},
  snapshot(){return JSON.parse(JSON.stringify(pad));},
  close(){pad.connected=false;if(original)Object.defineProperty(navigator,'getGamepads',{configurable:true,value:original});else delete navigator.getGamepads;delete globalThis.testVirtualStandardGamepad;}
 };
}
module.exports={installVirtualStandardGamepad};
