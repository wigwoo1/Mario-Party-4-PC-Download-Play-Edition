import {test} from 'node:test';
import assert from 'node:assert/strict';
import {displayControls} from './display-controls.mjs';

class Element{
 constructor(){this.handlers={};this.options=[];this.dataset={};this.children=[];this.hidden=false;this.value='640x480';this.disabled=false;this.attributes={};
  this.style={values:new Map(),priorities:new Map(),getPropertyValue(key){return this.values.get(key)||'';},getPropertyPriority(key){return this.priorities.get(key)||'';},setProperty(key,value,priority=''){this.values.set(key,value);this.priorities.set(key,priority);}};
 }
 addEventListener(type,handler){this.handlers[type]=handler;}
 setAttribute(key,value){this.attributes[key]=value;}
 append(element){element.parentElement=this;this.children.push(element);}
 after(element){if(this.parentElement?.insertAfter)this.parentElement.insertAfter(this,element);}
 async click(){await this.handlers.click?.();}
}
function fixture({saved='1280x960',savedAspect='4:3',hidden=true,fullscreenError}={}){
 const values=new Map([['partyboard-game-resolution',saved],['partyboard-game-aspect',savedAspect]]),storage={getItem:key=>values.get(key)??null,setItem:(key,value)=>values.set(key,value)};
 const container={children:[],querySelector(selector){const key=selector==='[data-display-fullscreen]'?'displayFullscreen':selector==='[data-display-status]'?'displayStatus':selector==='[data-display-aspect]'?'displayAspect':null;const search=elements=>{for(const element of elements){if(key&&Object.hasOwn(element.dataset,key))return element;const nested=search(element.children);if(nested)return nested;}return null;};return search(this.children);},insertAfter(reference,element){element.parentElement=this;this.children.splice(this.children.indexOf(reference)+1,0,element);}};
 const label=new Element();label.parentElement=container;container.children.push(label);
 const doc={handlers:{},head:{append(){}},defaultView:{localStorage:storage,innerWidth:1280,innerHeight:720,addEventListener(){}},fullscreenElement:null,
  createElement:()=>new Element(),addEventListener(type,handler){this.handlers[type]=handler;},
  documentElement:new Element(),async exitFullscreen(){this.fullscreenElement=null;this.handlers.fullscreenchange?.();}};
 const select=new Element();select.parentElement=label;select.options=['640x480','960x720','1280x960'].map(value=>({value,textContent:value}));
 Object.defineProperty(select,'selectedOptions',{get(){return [{textContent:this.value}]}});
 const view=new Element();view.hidden=hidden;const canvas=new Element();view.querySelector=selector=>selector==='canvas'?canvas:view.children.find(element=>selector==='[data-display-fullscreen-exit]'&&Object.hasOwn(element.dataset,'displayFullscreenExit'))||null;view.requestFullscreen=async function(){if(fullscreenError)throw fullscreenError;doc.fullscreenElement=this;doc.handlers.fullscreenchange?.();};
 doc.documentElement.requestFullscreen=async function(){doc.fullscreenElement=this;doc.handlers.fullscreenchange?.();};
 const messages=[],controls=displayControls({container,select,gameView:view,document:doc,storage,status:value=>messages.push(value)});
 return {values,storage,doc,select,view,canvas,messages,controls,container};
}

test('resolution and aspect are restored, saved locally, and frozen during runtime preparation',()=>{
 const f=fixture();assert.equal(f.select.value,'1280x960');assert.deepEqual(f.controls.resolution(),{width:1280,height:960});
 f.select.value='960x720';f.select.handlers.change();assert.equal(f.values.get('partyboard-game-resolution'),'960x720');
 const ratio=f.container.querySelector('[data-display-aspect]');ratio.value='16:9';ratio.handlers.change();
 assert.equal(f.values.get('partyboard-game-aspect'),'16:9');assert.deepEqual(f.controls.resolution(),{width:960,height:540});
 f.controls.setLocked(true);assert.equal(f.select.disabled,true);assert.equal(ratio.disabled,true);
 f.controls.setLocked(false);assert.equal(f.select.disabled,false);assert.equal(ratio.disabled,false);
});

test('full-screen button fills the available screen at the chosen ratio and exits cleanly',async()=>{
 const f=fixture({hidden:false,savedAspect:'16:9'}),button=f.container.querySelector('[data-display-fullscreen]');
 await button.click();assert.equal(f.doc.fullscreenElement,f.view);assert.equal(button.textContent,'Exit full screen');
 assert.equal(f.canvas.style.getPropertyValue('width'),'1280px');assert.equal(f.canvas.style.getPropertyValue('height'),'720px');
 const inGameExit=f.view.querySelector('[data-display-fullscreen-exit]');assert.equal(inGameExit.hidden,false);
 await inGameExit.click();assert.equal(f.doc.fullscreenElement,null);assert.equal(button.textContent,'Full screen');assert.equal(inGameExit.hidden,true);
});

test('waiting users can full-screen the page; browser fullscreen failures remain actionable',async()=>{
 const waiting=fixture({hidden:true});await waiting.container.querySelector('[data-display-fullscreen]').click();
 assert.equal(waiting.doc.fullscreenElement,waiting.doc.documentElement);
 const denied=fixture({hidden:false,fullscreenError:Error('denied')});
 await denied.container.querySelector('[data-display-fullscreen]').click();assert.match(denied.messages.at(-1),/denied/);
});

test('fullscreen entered while waiting fits the game when it appears and restores sizing when hidden',async()=>{
 const f=fixture({hidden:true,savedAspect:'16:9'});
 await f.container.querySelector('[data-display-fullscreen]').click();
 const exit=f.view.querySelector('[data-display-fullscreen-exit]');assert.equal(exit.hidden,true);
 f.view.hidden=false;f.controls.refresh();assert.equal(exit.hidden,false);
 assert.equal(f.canvas.style.getPropertyValue('width'),'1280px');assert.equal(f.canvas.style.getPropertyValue('height'),'720px');
 f.view.hidden=true;f.controls.refresh();assert.equal(exit.hidden,true);assert.equal(f.canvas.style.getPropertyValue('width'),'');
 assert.equal(f.doc.fullscreenElement,f.doc.documentElement);
});
