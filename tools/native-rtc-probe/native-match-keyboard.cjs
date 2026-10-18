'use strict';
// Match-driver adapter. Commands supply local physical input only; the native
// runtime retains its original session ownership and checkpoint capture.
function createNativeMatchKeyboard(send,{sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms))}={}){
 const held=new Set(),buttons={z:256,x:512,c:1024,v:2048,Enter:4096,q:64,e:32,r:16,ArrowUp:8,ArrowDown:4,ArrowLeft:1,ArrowRight:2};
 const supported=new Set([...Object.keys(buttons),'w','a','s','d','i','j','k','l']);
 const packet=()=>({type:'diagnostic-local-input',buttons:[...held].reduce((mask,key)=>mask|(buttons[key]||0),0),
  stickX:127*(Number(held.has('d'))-Number(held.has('a'))),stickY:127*(Number(held.has('w'))-Number(held.has('s'))),
  substickX:127*(Number(held.has('l'))-Number(held.has('j'))),substickY:127*(Number(held.has('i'))-Number(held.has('k'))),
  triggerLeft:held.has('q')?255:0,triggerRight:held.has('e')?255:0});
 const change=async(key,down)=>{
  if(!supported.has(key))throw Error('Unsupported native match key '+key);
  const previous=new Set(held);if(down)held.add(key);else held.delete(key);
  try{await send(packet());}catch(error){held.clear();for(const value of previous)held.add(value);throw error;}
 };
 return {down:key=>change(key,true),up:key=>change(key,false),
  async press(key,{delay=120}={}){
   if(!Number.isFinite(delay)||delay<0||delay>5000)throw Error('Bounded key delay required');
   await change(key,true);try{await sleep(delay);}finally{await change(key,false);}
  },async releaseAll(){held.clear();await send(packet());}};
}
module.exports={createNativeMatchKeyboard};
