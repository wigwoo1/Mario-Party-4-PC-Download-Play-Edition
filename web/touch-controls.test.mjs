import {test} from 'node:test';
import assert from 'node:assert/strict';
import {touchControls} from './touch-controls.mjs';
import {defaultControlBindings} from './control-bindings.mjs';

class Element{
 constructor(name='div'){this.tagName=name;this.handlers=new Map();this.children=[];this.attributes={};this.dataset={};this.hidden=false;this.style={};this.textContent='';}
 addEventListener(type,handler){const list=this.handlers.get(type)||[];list.push(handler);this.handlers.set(type,list);}
 removeEventListener(type,handler){this.handlers.set(type,(this.handlers.get(type)||[]).filter(value=>value!==handler));}
 setAttribute(key,value){this.attributes[key]=value;}
 append(...children){this.children.push(...children);}
 remove(){this.removed=true;}
 setPointerCapture(){}
 focus(){}
 dispatchEvent(event){this.dispatched??=[];this.dispatched.push(event);for(const handler of this.handlers.get(event.type)||[])handler(event);return true;}
 fire(type,event={}){event.type=type;event.preventDefault??=()=>{event.defaultPrevented=true;};for(const handler of this.handlers.get(type)||[])handler(event);return event;}
}
class KeyboardEvent{
 constructor(type,init){this.type=type;Object.assign(this,init);}
}
function fixture(options={}){
 const {touchPoints=1,coarsePointer=false,...controlOptions}=options;
 const doc=new Element('document'),view=new Element('window'),head=new Element('head'),container=new Element(),canvas=new Element('canvas');
 view.navigator={maxTouchPoints:touchPoints};view.matchMedia=()=>({matches:coarsePointer});
 view.KeyboardEvent=KeyboardEvent;doc.defaultView=view;doc.head=head;doc.documentElement=new Element();doc.hidden=false;doc.fullscreenElement=null;doc.createElement=name=>new Element(name);
 const profile=defaultControlBindings();let active=false;
 const api=touchControls({container,canvas,profile:()=>profile,enabled:()=>active,document:doc,...controlOptions});
 return {doc,view,head,container,canvas,profile,api,setActive(value){active=value;api.refresh();}};
}
function controls(root){const found=[];const visit=node=>{if(node.tagName==='button'&&node.dataset.touchAction)found.push(node);for(const child of node.children||[])visit(child);};visit(root);return found;}
function pointer(id){return {pointerId:id,defaultPrevented:false,preventDefault(){this.defaultPrevented=true;}};}

test('touch overlay stays hidden outside fullscreen or without a running game',()=>{
 const f=fixture();assert.equal(f.container.children[0].hidden,true);
 f.doc.fullscreenElement=f.container;f.doc.handlers.get('fullscreenchange')[0]();assert.equal(f.container.children[0].hidden,true);
 f.setActive(true);assert.equal(f.container.children[0].hidden,false);
 f.doc.fullscreenElement=null;f.doc.handlers.get('fullscreenchange')[0]();assert.equal(f.container.children[0].hidden,true);
});

test('multiple touch buttons use the saved bindings and inject held/released keyboard events',()=>{
 const f=fixture();f.setActive(true);f.doc.fullscreenElement=f.container;f.doc.handlers.get('fullscreenchange')[0]();
 const root=f.container.children[0],buttons=controls(root),a=buttons.find(button=>button.dataset.touchAction==='a'),up=buttons.find(button=>button.dataset.touchAction==='up');
 const downA=pointer(1),downUp=pointer(2);a.fire('pointerdown',downA);up.fire('pointerdown',downUp);
 assert.equal(downA.defaultPrevented,true);assert.equal(downUp.defaultPrevented,true);
 const keys=f.canvas.dispatched;
 a.fire('pointerup',pointer(1));up.fire('pointerup',pointer(2));
 const next=pointer(3);a.fire('pointerdown',next);a.fire('pointerup',pointer(3));
 assert.deepEqual(keys.map(event=>[event.type,event.code,event.key]),[['keydown','KeyZ','z'],['keydown','KeyW','w'],['keyup','KeyZ','z'],['keyup','KeyW','w'],['keydown','KeyZ','z'],['keyup','KeyZ','z']]);
 assert.ok(keys.every(event=>event.partyBoardTouchInput));
});

test('fullscreen loss and page hiding release every held touch key',()=>{
 const f=fixture(),keys=f.canvas.dispatched=[];
 f.setActive(true);f.doc.fullscreenElement=f.container;f.doc.handlers.get('fullscreenchange')[0]();
 const a=controls(f.container.children[0]).find(button=>button.dataset.touchAction==='a');
 a.fire('pointerdown',pointer(9));assert.deepEqual(keys.map(event=>[event.type,event.code]),[['keydown','KeyZ']]);
 f.doc.fullscreenElement=null;f.doc.handlers.get('fullscreenchange')[0]();assert.deepEqual(keys.map(event=>[event.type,event.code]),[['keydown','KeyZ'],['keyup','KeyZ']]);
});

test('a held physical key is not released while its matching touch remains down',()=>{
 const f=fixture(),keys=f.canvas.dispatched=[];
 f.setActive(true);f.doc.fullscreenElement=f.container;f.doc.handlers.get('fullscreenchange')[0]();
 const a=controls(f.container.children[0]).find(button=>button.dataset.touchAction==='a');
 a.fire('pointerdown',pointer(1));assert.deepEqual(keys.map(event=>[event.type,event.code]),[['keydown','KeyZ']]);
 const physicalDown={type:'keydown',code:'KeyZ'};for(const handler of f.doc.handlers.get('keydown'))handler(physicalDown);
 const physicalUp={type:'keyup',code:'KeyZ'};for(const handler of f.doc.handlers.get('keyup'))handler(physicalUp);
 assert.deepEqual(keys.map(event=>[event.type,event.code]),[['keydown','KeyZ'],['keydown','KeyZ']]);
 a.fire('pointerup',pointer(1));assert.deepEqual(keys.map(event=>[event.type,event.code]),[['keydown','KeyZ'],['keydown','KeyZ'],['keyup','KeyZ']]);
});

test('touch settings use currently saved remapped keyboard controls',()=>{
 const f=fixture(),keys=f.canvas.dispatched=[];
 f.profile.keyboard.a='Space';f.setActive(true);f.doc.fullscreenElement=f.doc.documentElement;f.doc.handlers.get('fullscreenchange')[0]();
 const a=controls(f.container.children[0]).find(button=>button.dataset.touchAction==='a');a.fire('pointerdown',pointer(1));a.fire('pointerup',pointer(1));
 assert.deepEqual(keys.map(event=>[event.type,event.code]),[['keydown','Space'],['keyup','Space']]);
 f.api.close();assert.equal(f.container.children[0].removed,true);assert.equal(f.head.children[0].removed,true);
});


test('physical gamepad input hides touch pads, releases held keys and restores after idle',()=>{
 let time=0,pressed=false;
 const f=fixture({now:()=>time,gamepads:()=>[null,{connected:true,buttons:[{pressed,value:pressed?1:0}]}]});
 f.setActive(true);f.doc.fullscreenElement=f.container;f.api.refresh();
 const root=f.container.children[0],toggle=root.children[0],pads=root.children.slice(1);
 controls(root).find(b=>b.dataset.touchAction==='a').fire('pointerdown',pointer(1));
 pressed=true;f.api.refresh();assert.equal(root.hidden,false);assert.ok(pads.every(p=>p.hidden));assert.equal(toggle.textContent,'Show touch controls');
 assert.deepEqual(f.canvas.dispatched.map(e=>e.type),['keydown','keyup']);
 time=5000;f.api.refresh();assert.ok(pads.every(p=>p.hidden),'held button keeps overlay hidden');
 pressed=false;time=7999;f.api.refresh();assert.ok(pads.every(p=>p.hidden));
 time=8000;f.api.refresh();assert.ok(pads.every(p=>!p.hidden));
 toggle.fire('click');assert.ok(pads.every(p=>p.hidden));
 time=12000;f.api.refresh();assert.ok(pads.every(p=>p.hidden),'manual hiding persists');
 toggle.fire('click');assert.ok(pads.every(p=>!p.hidden));
});

test('unavailable or disconnected gamepads do not hide touch controls',()=>{
 const f=fixture({gamepads:()=>[{connected:false,buttons:[{pressed:true,value:1}]}]});
 f.setActive(true);f.doc.fullscreenElement=f.container;f.api.refresh();assert.ok(f.container.children[0].children.slice(1).every(p=>!p.hidden));
});

test('mouse-only desktop fullscreen stays unobstructed until actual touch is detected',()=>{
 const f=fixture({touchPoints:0});f.setActive(true);f.doc.fullscreenElement=f.container;f.api.refresh();
 const root=f.container.children[0];assert.equal(root.hidden,true);
 f.doc.fire('pointerdown',{pointerType:'mouse'});assert.equal(root.hidden,true);
 f.doc.fire('pointerdown',{pointerType:'touch'});assert.equal(root.hidden,false);
 f.api.close();assert.equal(f.doc.handlers.get('pointerdown').length,0);
});
test('coarse pointer devices retain fullscreen touch controls without a reported touch count',()=>{
 const f=fixture({touchPoints:0,coarsePointer:true});f.setActive(true);f.doc.fullscreenElement=f.container;f.api.refresh();
 assert.equal(f.container.children[0].hidden,false);
});
