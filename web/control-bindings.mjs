const keyboardDefaults={a:'KeyZ',b:'KeyX',x:'KeyC',y:'KeyV',l:'KeyQ',r:'KeyE',z:'KeyR',start:'Enter',up:'KeyW',down:'KeyS',left:'KeyA',right:'KeyD',stickUp:'ArrowUp',stickDown:'ArrowDown',stickLeft:'ArrowLeft',stickRight:'ArrowRight',cUp:'KeyI',cDown:'KeyK',cLeft:'KeyJ',cRight:'KeyL'};
const controllerDefaults={a:'button:0',b:'button:1',x:'button:2',y:'button:3',l:'button:6',r:'button:7',z:'button:5',start:'button:9',up:'button:12',down:'button:13',left:'button:14',right:'button:15',stickUp:'axis:1:-',stickDown:'axis:1:+',stickLeft:'axis:0:-',stickRight:'axis:0:+',cUp:'axis:3:-',cDown:'axis:3:+',cLeft:'axis:2:-',cRight:'axis:2:+'};
const copy=value=>structuredClone(value);
const padButtons={a:0x100,b:0x200,x:0x400,y:0x800,l:0x40,r:0x20,z:0x10,start:0x1000,up:8,down:4,left:1,right:2};
const padAxes={stickRight:0,stickLeft:1,stickUp:2,stickDown:3,cRight:4,cLeft:5,cUp:6,cDown:7,l:8,r:9};
function scancode(code){
 if(/^Key[A-Z]$/.test(code))return code.charCodeAt(3)-65+4;
 if(/^Digit[0-9]$/.test(code))return code==='Digit0'?39:Number(code.slice(5))+29;
 return {Enter:40,Space:44,Tab:43,Backspace:42,ArrowRight:79,ArrowLeft:80,ArrowDown:81,ArrowUp:82,ControlLeft:224,ShiftLeft:225,ControlRight:228,ShiftRight:229}[code];
}
export function compileKeyboardBindings(value){
 const {keyboard}=validateControlBindings(value);
 return {
  buttons:Object.entries(padButtons).map(([action,padButton])=>({scancode:scancode(keyboard[action]),padButton})),
  // L/R must retain both their digital action and analog trigger value.
  axes:Object.entries(padAxes).map(([action,padAxis])=>({scancode:scancode(keyboard[action]),padAxis,influence:0}))
 };
}
const standardButtons={0:0,1:1,2:2,3:3,4:9,5:10,8:4,9:6,10:7,11:8,12:11,13:12,14:13,15:14,16:5};
function controllerSource(binding){
 const [kind,index,direction]=binding.split(':');
 if(kind==='axis'){
  if(Number(index)>3)throw TypeError('Unsupported standard controller axis.');
  return {nativeAxis:{nativeAxis:Number(index),sign:direction==='+'?1:-1},nativeButton:-1};
 }
 if(index==='6'||index==='7')return {nativeAxis:{nativeAxis:index==='6'?4:5,sign:1},nativeButton:-1};
 if(!Object.hasOwn(standardButtons,index))throw TypeError('Unsupported standard controller button.');
 return {nativeAxis:{nativeAxis:-1,sign:1},nativeButton:standardButtons[index]};
}
export function compileControllerBindings(value){
 const {controller}=validateControlBindings(value);
 const buttons=Object.entries(padButtons).map(([action,padButton])=>{
  const source=controllerSource(controller[action]);
  if(source.nativeAxis.nativeAxis!==-1&&action!=='l'&&action!=='r')throw TypeError('This digital action requires a controller button.');
  return {nativeButton:source.nativeButton===-1?0xffffffff:source.nativeButton,padButton};
 });
 const axes=Object.entries(padAxes).map(([action,padAxis])=>({...controllerSource(controller[action]),padAxis}));
 return {buttons,axes,emulateTriggers:true};
}
export function controlBindingGlyphs(value){
 const profile=validateControlBindings(value);
 const entries={2:'a',3:'b',4:'x',5:'y',6:'r',8:'l',11:'z'};
 const names={0:'south',1:'east',2:'west',3:'north',4:'l1',5:'r1',6:'l2',7:'r2',8:'select',9:'start'};
 const keyboard={},gamepad={};
 for(const [entry,action] of Object.entries(entries)){
  const code=profile.keyboard[action];
  keyboard[entry]=code.startsWith('Key')?code.slice(3):code.startsWith('Digit')?code.slice(5):code.replace('Arrow','').replace('Left',' L').replace('Right',' R');
  const binding=profile.controller[action];
  // Keep the original authored action icon when the physical binding lacks a
  // supported glyph; never show an unrelated default physical button.
  gamepad[entry]=binding.startsWith('button:')?(names[binding.slice(7)]??null):null;
 }
 return {keyboard,gamepad};
}
export function defaultControlBindings(){return {keyboard:copy(keyboardDefaults),controller:copy(controllerDefaults)};}
export function validateControlBindings(value){
 if(!value||typeof value!=='object')throw TypeError('Control bindings required.');
 const result={};
 for(const device of ['keyboard','controller']){
  const map=value[device];if(!map||typeof map!=='object'||Array.isArray(map))throw TypeError('Device bindings required.');
  if(Object.keys(map).length!==Object.keys(keyboardDefaults).length)throw TypeError('Every control needs a binding.');
  const seen=new Map();result[device]={};
  for(const action of Object.keys(keyboardDefaults)){
   const binding=map[action];
   const valid=device==='keyboard'?/^(Key[A-Z]|Digit[0-9]|Arrow(Up|Down|Left|Right)|Enter|Space|Tab|Backspace|Shift(Left|Right)|Control(Left|Right))$/:/^(button:(?:[0-9]|[12][0-9]|3[01])|axis:[0-7]:[+-])$/;
   if(typeof binding!=='string'||!valid.test(binding))throw TypeError('Invalid '+device+' binding for '+action+'.');
   if(seen.has(binding))throw TypeError('Binding conflict: '+action+' and '+seen.get(binding)+'.');
   seen.set(binding,action);result[device][action]=binding;
  }
 }
 return result;
}
export function createControlBindingEditor(initial=defaultControlBindings()){
 let committed=validateControlBindings(initial),draft=copy(committed);
 return {
  snapshot:()=>copy(draft),committed:()=>copy(committed),
  set(device,action,binding){if(!Object.hasOwn(draft,device)||!Object.hasOwn(keyboardDefaults,action))throw TypeError('Unknown control.');draft[device][action]=binding;},
  conflicts(){try{validateControlBindings(draft);return null;}catch(error){return error.message;}},
  cancel(){draft=copy(committed);},reset(){draft=defaultControlBindings();},
  commit(){const validated=validateControlBindings(draft);committed=validated;return copy(committed);}
 };
}
