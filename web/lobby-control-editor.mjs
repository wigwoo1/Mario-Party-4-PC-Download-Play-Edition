import {createControlBindingEditor,defaultControlBindings,validateControlBindings,compileControllerBindings} from './control-bindings.mjs';
const actionNames={a:'A button',b:'B button',x:'X button',y:'Y button',l:'L trigger',r:'R trigger',z:'Z button',start:'Start',up:'D-pad up',down:'D-pad down',left:'D-pad left',right:'D-pad right',stickUp:'Main stick up',stickDown:'Main stick down',stickLeft:'Main stick left',stickRight:'Main stick right',cUp:'C-stick up',cDown:'C-stick down',cLeft:'C-stick left',cRight:'C-stick right'};
const controllerNames=['Bottom face button (A / Cross)','Right face button (B / Circle)','Left face button (X / Square)','Top face button (Y / Triangle)','Left shoulder (LB / L1)','Right shoulder (RB / R1)','Left trigger (LT / L2)','Right trigger (RT / R2)','Back / Select','Start / Options','Left stick click','Right stick click','D-pad up','D-pad down','D-pad left','D-pad right','Guide / Home'];
function bindingLabel(value){
 if(value.startsWith('button:'))return controllerNames[Number(value.split(':')[1])];
 if(value.startsWith('axis:')){const [,axis,sign]=value.split(':');return ['Left stick '+(sign==='+'?'right':'left'),'Left stick '+(sign==='+'?'down':'up'),'Right stick '+(sign==='+'?'right':'left'),'Right stick '+(sign==='+'?'down':'up')][Number(axis)];}
 return value.replace(/^Key|^Digit/,'').replace(/^Arrow/,'Arrow ').replace(/^(Shift|Control)(Left|Right)$/,'$2 $1');
}
export function lobbyControlEditor(container,{storage}={}){
 const doc=container.ownerDocument;
 if(storage===undefined){try{storage=doc.defaultView?.localStorage;}catch{storage=null;}}
 let initial=defaultControlBindings();
 try{const saved=storage?.getItem('partyboard-control-bindings');if(saved)initial=validateControlBindings(JSON.parse(saved));}catch{/* Ignore stale or unavailable browser settings and use defaults. */}
 const editor=createControlBindingEditor(initial);
 const open=doc.createElement('button');open.type='button';open.textContent='Customize Controls';
 const panel=doc.createElement('fieldset');panel.hidden=true;
 const legend=doc.createElement('legend');legend.textContent='Keyboard and controller controls';panel.append(legend);
 const feedback=doc.createElement('p');feedback.setAttribute('role','status');
 const fields=[];
 const keys=[...Array.from({length:26},(_,i)=>'Key'+String.fromCharCode(65+i)),...Array.from({length:10},(_,i)=>'Digit'+i),'Enter','Space','Tab','Backspace','ArrowUp','ArrowDown','ArrowLeft','ArrowRight','ShiftLeft','ShiftRight','ControlLeft','ControlRight'];
 const buttons=Array.from({length:17},(_,i)=>'button:'+i),axes=Array.from({length:4},(_,i)=>['axis:'+i+':+','axis:'+i+':-']).flat();
 for(const device of ['keyboard','controller'])for(const action of Object.keys(editor.snapshot()[device])){
  const label=doc.createElement('label');label.textContent=(device==='keyboard'?'Keyboard: ':'Controller: ')+actionNames[action]+' ';
  const select=doc.createElement('select');select.setAttribute('aria-label',(device==='keyboard'?'Keyboard: ':'Controller: ')+actionNames[action]);
  const options=device==='keyboard'?keys:(['l','r'].includes(action)||action.startsWith('stick')||action.startsWith('c')?[...buttons,...axes]:buttons);
  for(const value of options){const option=doc.createElement('option');option.value=value;option.textContent=bindingLabel(value);select.append(option);}
  select.addEventListener('change',()=>{editor.set(device,action,select.value);feedback.textContent=editor.conflicts()??'';});
  label.append(select);panel.append(label,doc.createElement('br'));fields.push({select,device,action});
 }
 const refresh=()=>{const snapshot=editor.snapshot();for(const field of fields)field.select.value=snapshot[field.device][field.action];feedback.textContent='';};
 const actionButton=(text,fn)=>{const button=doc.createElement('button');button.type='button';button.textContent=text;button.addEventListener('click',fn);panel.append(button);};
 actionButton('Apply controls',()=>{try{compileControllerBindings(editor.snapshot());const value=editor.commit();try{storage?.setItem('partyboard-control-bindings',JSON.stringify(value));feedback.textContent='Controls saved for the next local or online game on this device.';}catch{feedback.textContent='Controls selected for the next game but could not be saved by this browser.';}panel.hidden=true;}catch(error){feedback.textContent=error.message;}});
 actionButton('Cancel',()=>{editor.cancel();refresh();panel.hidden=true;});
 actionButton('Reset to defaults',()=>{editor.reset();refresh();});
 panel.append(feedback);open.addEventListener('click',()=>{editor.cancel();refresh();panel.hidden=!panel.hidden;});
 panel.addEventListener('keydown',event=>event.stopPropagation());container.append(open,panel);
 refresh();
 return {profile:()=>editor.committed(),customized(device){return JSON.stringify(editor.committed()[device])!==JSON.stringify(defaultControlBindings()[device]);},setLocked(locked){open.disabled=locked;panel.disabled=locked;if(locked){editor.cancel();panel.hidden=true;}}};
}

