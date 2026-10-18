const resolutions=new Set(['640x480','960x720','1280x960','1920x1440','2560x1920']);
const resolutionKey='partyboard-game-resolution';
const aspectKey='partyboard-game-aspect';
const aspects=new Set(['4:3','16:9','16:10','21:9']);

function readResolution(storage){
 try{const value=storage?.getItem(resolutionKey);return resolutions.has(value)?value:null;}catch{return null;}
}

/** Page-level display controls shared by hosts and guests on this device. */
export function displayControls({container,select,gameView,document=globalThis.document,storage,status=()=>{}}){
 if(!container||!select||!gameView||!document)throw TypeError('Fullscreen and resolution controls require page elements.');
 let button=container.querySelector('[data-display-fullscreen]'),message=container.querySelector('[data-display-status]');
 if(!button){button=document.createElement('button');button.type='button';button.dataset.displayFullscreen='';button.textContent='Full screen';select.parentElement.after(button);}
 if(!message){message=document.createElement('p');message.dataset.displayStatus='';message.setAttribute('role','status');message.setAttribute('aria-live','polite');message.textContent='Resolution applies the next time you prepare a game and is saved on this device.';button.after(message);}
 let fullscreenExit=gameView.querySelector('[data-display-fullscreen-exit]');
 if(!fullscreenExit){fullscreenExit=document.createElement('button');fullscreenExit.type='button';fullscreenExit.dataset.displayFullscreenExit='';fullscreenExit.textContent='Exit full screen';fullscreenExit.hidden=true;fullscreenExit.setAttribute('aria-label','Exit full screen');fullscreenExit.style.cssText='position:absolute;top:12px;right:12px;z-index:2';gameView.append(fullscreenExit);}
 if(storage===undefined){try{storage=document.defaultView?.localStorage;}catch{storage=null;}}
 let aspect=container.querySelector('[data-display-aspect]');
 if(!aspect){const label=document.createElement('label');label.textContent='Display aspect ratio ';aspect=document.createElement('select');aspect.dataset.displayAspect='';
  for(const value of aspects){const option=document.createElement('option');option.value=value;option.textContent=value+(value==='4:3'?' (original)':'');aspect.append(option);}label.append(aspect);select.parentElement.after(label);}
 const validOptions=new Set([...select.options].map(option=>option.value).filter(value=>resolutions.has(value)));
 const saved=readResolution(storage);if(saved&&validOptions.has(saved))select.value=saved;
 try{const savedAspect=storage?.getItem(aspectKey);if(aspects.has(savedAspect))aspect.value=savedAspect;}catch{/* Ignore unavailable browser storage. */}
 const change=()=>{if(!validOptions.has(select.value))return;try{storage?.setItem(resolutionKey,select.value);}catch{/* Settings still work for this page when storage is unavailable. */}message.textContent='Saved '+select.selectedOptions?.[0]?.textContent+'. It will be used next time you prepare a game.';};
 const aspectChange=()=>{if(!aspects.has(aspect.value))return;try{storage?.setItem(aspectKey,aspect.value);}catch{/* Settings still work for this page when storage is unavailable. */}message.textContent='Saved '+aspect.value+' output aspect ratio; it applies the next time you prepare a game.';};
 const canvas=gameView.querySelector('canvas');let originalCanvasSize;
 const gameFullscreen=()=>document.fullscreenElement===gameView||(document.fullscreenElement===document.documentElement&&!gameView.hidden);
 const fitCanvas=()=>{
  if(!canvas?.style||!gameFullscreen())return;
  const [ratioWidth,ratioHeight]=aspect.value.split(':').map(Number),view=document.defaultView||globalThis,scale=Math.min(view.innerWidth/ratioWidth,view.innerHeight/ratioHeight);
  if(!originalCanvasSize)originalCanvasSize={width:canvas.style.getPropertyValue('width'),widthPriority:canvas.style.getPropertyPriority('width'),height:canvas.style.getPropertyValue('height'),heightPriority:canvas.style.getPropertyPriority('height')};
  canvas.style.setProperty('width',Math.floor(scale*ratioWidth)+'px','important');canvas.style.setProperty('height',Math.floor(scale*ratioHeight)+'px','important');
 };
 const sync=()=>{button.textContent=document.fullscreenElement?'Exit full screen':'Full screen';fullscreenExit.hidden=!gameFullscreen();if(gameFullscreen())fitCanvas();else if(canvas?.style&&originalCanvasSize){canvas.style.setProperty('width',originalCanvasSize.width,originalCanvasSize.widthPriority);canvas.style.setProperty('height',originalCanvasSize.height,originalCanvasSize.heightPriority);originalCanvasSize=undefined;}};
 const style=document.createElement('style');style.textContent='#game-view:fullscreen,html:fullscreen #game-view:not([hidden]){position:relative;display:grid;place-items:center;align-content:center;gap:12px;box-sizing:border-box;width:100vw;height:100vh;background:#000;padding:0}#game-view:fullscreen canvas,html:fullscreen #game-view:not([hidden]) canvas{max-width:none!important;background:#000}';style.textContent+='#game-view:fullscreen>br,#game-view:fullscreen>#stop-game,html:fullscreen #game-view:not([hidden])>br,html:fullscreen #game-view:not([hidden])>#stop-game{display:none}';style.textContent+='html:fullscreen #game-view:not([hidden]){position:fixed;inset:0;z-index:100}';document.head?.append(style);
 select.addEventListener('change',change);
 aspect.addEventListener('change',aspectChange);
 document.addEventListener('fullscreenchange',sync);
 document.defaultView?.addEventListener('resize',fitCanvas);
 const VisibilityObserver=document.defaultView?.MutationObserver;
 if(VisibilityObserver)new VisibilityObserver(sync).observe(gameView,{attributes:true,attributeFilter:['hidden']});
 const toggleFullscreen=async()=>{
  try{
   if(document.fullscreenElement)await document.exitFullscreen();
   else await (gameView.hidden?document.documentElement:gameView).requestFullscreen();
  }catch(error){message.textContent='Full screen is unavailable: '+(error?.message||String(error));status(message.textContent);}
  sync();
 };
 button.addEventListener('click',toggleFullscreen);fullscreenExit.addEventListener('click',toggleFullscreen);
 sync();
 return {refresh:sync,resolution:()=>{const [maxWidth]=select.value.split('x').map(Number),[ratioWidth,ratioHeight]=aspect.value.split(':').map(Number);return {width:maxWidth,height:Math.round(maxWidth*ratioHeight/ratioWidth)};},setLocked(locked){select.disabled=Boolean(locked);aspect.disabled=Boolean(locked);}};
}


