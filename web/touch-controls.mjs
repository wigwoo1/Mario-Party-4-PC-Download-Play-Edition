const groups={
  directions:{label:'D-pad',actions:['up','left','down','right']},
  stick:{label:'Main stick',actions:['stickUp','stickLeft','stickDown','stickRight']},
  face:{label:'Buttons',actions:['x','y','a','b']},
  shoulder:{label:'Shoulder and Start',actions:['l','z','r','start']},
  cstick:{label:'C-stick',actions:['cUp','cLeft','cDown','cRight']},
};
const names={up:'↑',down:'↓',left:'←',right:'→',stickUp:'↑',stickDown:'↓',stickLeft:'←',stickRight:'→',cUp:'↑',cDown:'↓',cLeft:'←',cRight:'→',a:'A',b:'B',x:'X',y:'Y',l:'L',r:'R',z:'Z',start:'Start'};
const keyNames={Enter:'Enter',Space:' ',Tab:'Tab',Backspace:'Backspace',ArrowUp:'ArrowUp',ArrowDown:'ArrowDown',ArrowLeft:'ArrowLeft',ArrowRight:'ArrowRight',ShiftLeft:'Shift',ShiftRight:'Shift',ControlLeft:'Control',ControlRight:'Control'};
const legacyCodes={Enter:13,Space:32,Tab:9,Backspace:8,ArrowUp:38,ArrowDown:40,ArrowLeft:37,ArrowRight:39,ShiftLeft:16,ShiftRight:16,ControlLeft:17,ControlRight:17};
function keyEventParts(code){
  if(/^Key[A-Z]$/.test(code))return {code,key:code.slice(3).toLowerCase(),keyCode:code.charCodeAt(3)};
  if(/^Digit[0-9]$/.test(code))return {code,key:code.slice(5),keyCode:code.charCodeAt(5)};
  const key=keyNames[code];if(key===undefined)throw TypeError('Unsupported touch control key: '+code);
  return {code,key,keyCode:legacyCodes[code]};
}
function markTouchEvent(event){Object.defineProperty(event,'partyBoardTouchInput',{value:true});return event;}

/** Mobile overlay that injects the device's saved keyboard bindings into SDL. */
export function touchControls({container,canvas,profile,enabled=()=>true,document=globalThis.document,
  gamepads=()=>globalThis.navigator?.getGamepads?.()||[],now=()=>performance.now(),controllerIdleMs=3000}){
  if(!container||!canvas||typeof profile!=='function'||typeof enabled!=='function'||!document?.createElement)
    throw TypeError('Touch controls require a game container, canvas, and saved-control provider.');
  const view=document.defaultView,KeyboardEventType=view?.KeyboardEvent;
  if(typeof KeyboardEventType!=='function')throw Error('This browser cannot create game keyboard input events.');
  const style=document.createElement('style');
  style.textContent=`.partyboard-touch-controls{position:absolute;inset:0;z-index:5;pointer-events:none;touch-action:none;user-select:none;-webkit-user-select:none}.partyboard-touch-controls[hidden],.partyboard-touch-pad[hidden]{display:none!important}.partyboard-touch-toggle{position:absolute;left:calc(8px + env(safe-area-inset-left));top:calc(8px + env(safe-area-inset-top));pointer-events:auto;touch-action:manipulation;opacity:.78}.partyboard-touch-pad{position:absolute;bottom:calc(10px + env(safe-area-inset-bottom));display:flex;align-items:end;gap:clamp(4px,1.5vmin,14px)}.partyboard-touch-left{left:calc(8px + env(safe-area-inset-left));flex-wrap:wrap;width:min(42vw,310px)}.partyboard-touch-center{left:50%;transform:translateX(-50%)}.partyboard-touch-right{right:calc(8px + env(safe-area-inset-right));flex-direction:column;align-items:center}.partyboard-touch-cross,.partyboard-touch-face,.partyboard-touch-extra{display:grid;grid-template-columns:repeat(3,clamp(38px,8vmin,64px));grid-template-rows:repeat(3,clamp(38px,8vmin,64px));gap:2px}.partyboard-touch-cross .up,.partyboard-touch-face .y{grid-column:2}.partyboard-touch-cross .left,.partyboard-touch-face .x{grid-column:1;grid-row:2}.partyboard-touch-cross .down,.partyboard-touch-face .a{grid-column:2;grid-row:3}.partyboard-touch-cross .right,.partyboard-touch-face .b{grid-column:3;grid-row:2}.partyboard-touch-extra{grid-template-columns:repeat(4,clamp(32px,6.5vmin,48px));grid-template-rows:clamp(32px,6.5vmin,48px)}.partyboard-touch-button{pointer-events:auto;touch-action:none;min-width:0;min-height:0;width:100%;height:100%;margin:0;padding:2px;border:2px solid #e9f2ffb8;border-radius:50%;background:#14213bd9;color:white;font:700 clamp(13px,2.4vmin,20px) system-ui;box-shadow:0 2px 8px #0009}.partyboard-touch-button:active,.partyboard-touch-button[aria-pressed=true]{background:#b9dc65;color:#152000;transform:scale(.94)}.partyboard-touch-label{position:absolute;clip-path:inset(50%);width:1px;height:1px;overflow:hidden}`;
  style.textContent+=`.partyboard-touch-cross,.partyboard-touch-face{grid-template-columns:repeat(3,clamp(38px,8vmin,40px));grid-template-rows:repeat(3,clamp(38px,8vmin,40px))}.partyboard-touch-group-directions,.partyboard-touch-group-stick{left:calc(8px + env(safe-area-inset-left))}.partyboard-touch-group-face,.partyboard-touch-group-cstick{right:calc(8px + env(safe-area-inset-right))}.partyboard-touch-group-directions,.partyboard-touch-group-cstick{bottom:calc(144px + env(safe-area-inset-bottom))}.partyboard-touch-group-shoulder{left:50%;transform:translateX(-50%);top:calc(52px + env(safe-area-inset-top));bottom:auto}.partyboard-touch-cross button:nth-child(1){grid-column:2;grid-row:1}.partyboard-touch-cross button:nth-child(2){grid-column:1;grid-row:2}.partyboard-touch-cross button:nth-child(3){grid-column:2;grid-row:3}.partyboard-touch-cross button:nth-child(4){grid-column:3;grid-row:2}`;
  document.head?.append(style);
  const root=document.createElement('div');root.className='partyboard-touch-controls';root.hidden=true;root.setAttribute('aria-label','Touch game controller');container.append(root);
  const toggle=document.createElement('button');toggle.type='button';toggle.className='partyboard-touch-toggle';toggle.textContent='Hide touch controls';toggle.setAttribute('aria-label','Hide touch controls');root.append(toggle);
  const controlPads=new Map(),byPointer=new Map(),heldCodes=new Map(),physicalCodes=new Set();
  let visible=true,closed=false,lastControllerInput=-Infinity,animationFrame;
  const makeEvent=(type,code)=>{
    const parts=keyEventParts(code),event=new KeyboardEventType(type,{...parts,bubbles:true,cancelable:true,composed:true});
    for(const property of ['keyCode','which'])try{Object.defineProperty(event,property,{value:parts.keyCode});}catch{/* Older browsers may expose these as non-configurable. */}
    return markTouchEvent(event);
  };
  const dispatch=(type,code)=>canvas.dispatchEvent(makeEvent(type,code));
  const press=(code)=>{const count=heldCodes.get(code)||0;heldCodes.set(code,count+1);if(!count&&!physicalCodes.has(code))dispatch('keydown',code);};
  const release=(code)=>{const count=heldCodes.get(code)||0;if(count>1){heldCodes.set(code,count-1);return;}heldCodes.delete(code);if(count&&!physicalCodes.has(code))dispatch('keyup',code);};
  const releaseAll=()=>{for(const [pointer,code] of byPointer){const button=controlPads.get(pointer)?.button;if(button)button.setAttribute('aria-pressed','false');release(code);}byPointer.clear();controlPads.clear();};
  function update(){
    const show=!closed&&(document.fullscreenElement===container||document.fullscreenElement===document.documentElement)&&!!enabled();
    if(root.hidden!==!show){root.hidden=!show;if(!show)releaseAll();}
    if(show){
      let pads=[];try{pads=Array.from(gamepads());}catch{/* Touch remains available if gamepad access is unavailable. */}
      if(pads.some(pad=>pad?.connected!==false&&pad?.buttons?.some(button=>button.pressed||button.value>0.5)))lastControllerInput=now();
    }
    const controlsVisible=visible&&now()-lastControllerInput>=controllerIdleMs;
    for(const pad of root.children)if(pad!==toggle)pad.hidden=!controlsVisible;
    toggle.textContent=controlsVisible?'Hide touch controls':'Show touch controls';toggle.setAttribute('aria-label',toggle.textContent);
    if(!controlsVisible)releaseAll();
  }
  for(const [group,definition] of Object.entries(groups)){
    const pad=document.createElement('div');pad.className='partyboard-touch-pad partyboard-touch-group-'+group;
    pad.setAttribute('role','group');pad.setAttribute('aria-label',definition.label);root.append(pad);
    let layout=pad;
    if(['directions','stick','cstick'].includes(group)){layout=document.createElement('div');layout.className='partyboard-touch-cross';pad.append(layout);}
    if(group==='face'){layout=document.createElement('div');layout.className='partyboard-touch-face';pad.append(layout);}
    if(group==='shoulder'){layout=document.createElement('div');layout.className='partyboard-touch-extra';pad.append(layout);}
    for(const action of definition.actions){
      const button=document.createElement('button');button.type='button';button.className='partyboard-touch-button '+action;button.textContent=names[action];button.dataset.touchAction=action;
      button.setAttribute('aria-label',definition.label+' '+(action[0].toUpperCase()+action.slice(1)));button.setAttribute('aria-pressed','false');
      button.addEventListener('contextmenu',event=>event.preventDefault());
      button.addEventListener('pointerdown',event=>{
        if(closed||root.hidden||pad.hidden||!enabled()||byPointer.has(event.pointerId))return;
        const code=profile()?.keyboard?.[action];if(typeof code!=='string')return;
        try{keyEventParts(code);}catch{return;}
        event.preventDefault();canvas.focus?.({preventScroll:true});
        try{button.setPointerCapture?.(event.pointerId);}catch{/* Pointer capture may be unavailable in embedded browsers. */}
        controlPads.set(event.pointerId,{button,action});byPointer.set(event.pointerId,code);button.setAttribute('aria-pressed','true');press(code);
      });
      const finish=event=>{
        const code=byPointer.get(event.pointerId);if(code===undefined)return;
        event.preventDefault();byPointer.delete(event.pointerId);controlPads.get(event.pointerId)?.button.setAttribute('aria-pressed','false');controlPads.delete(event.pointerId);release(code);
      };
      for(const type of ['pointerup','pointercancel','lostpointercapture'])button.addEventListener(type,finish);
      layout.append(button);
    }
  }
  toggle.addEventListener('click',()=>{if(now()-lastControllerInput<controllerIdleMs){lastControllerInput=-Infinity;visible=true;}else visible=!visible;update();});
  const physical=event=>{
    if(event.partyBoardTouchInput||typeof event.code!=='string')return;
    if(event.type==='keydown')physicalCodes.add(event.code);else{
      physicalCodes.delete(event.code);
      if(heldCodes.has(event.code))dispatch('keydown',event.code);
    }
  };
  document.addEventListener('keydown',physical);document.addEventListener('keyup',physical);
  const blur=()=>{releaseAll();physicalCodes.clear();};
  const hidden=()=>{if(document.hidden)blur();};document.addEventListener('visibilitychange',hidden);view?.addEventListener('blur',blur);
  document.addEventListener('fullscreenchange',update);update();
  const animate=()=>{if(closed)return;update();animationFrame=view.requestAnimationFrame(animate);};
  if(view?.requestAnimationFrame)animationFrame=view.requestAnimationFrame(animate);
  return {refresh:update,close(){if(closed)return;closed=true;if(animationFrame!==undefined)view?.cancelAnimationFrame?.(animationFrame);releaseAll();document.removeEventListener('keydown',physical);document.removeEventListener('keyup',physical);document.removeEventListener('fullscreenchange',update);document.removeEventListener('visibilitychange',hidden);view?.removeEventListener('blur',blur);root.remove();style.remove();}};
}



