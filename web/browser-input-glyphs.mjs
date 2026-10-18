import {InputGlyphState} from './input-glyph-state.mjs';

// Attach to the game canvas, not the document: typing in lobby text fields
// should not change game prompts. Input routing remains owned by the engine.
export function attachInputGlyphs({target,bindings,onChange,environment=globalThis}) {
  const state=new InputGlyphState();
  let closed=false,frame,previous='';
  function publish() {
    const map=bindings(state.active);
    const actions=new Set([...Object.keys(map.keyboard||{}),...Object.keys(map.gamepad||{})]);
    const glyphs=Object.fromEntries([...actions].map(action=>[action,state.glyph(action,map)]));
    const snapshot={active:state.active,glyphs};
    const signature=JSON.stringify(snapshot);
    if(signature!==previous) {previous=signature;onChange(snapshot);}
  }
  function keyboard(event) {
    if(closed||event.repeat||event.isComposing||event.ctrlKey||event.metaKey||event.altKey) return;
    if(event.target!==target) return;
    state.keyboardActivity();publish();
  }
  function poll() {
    if(closed)return;
    // Hidden/unfocused pages cannot claim controller activity.
    if(!environment.document.hidden && environment.document.activeElement===target) {
      let pads=[];
      try {pads=environment.navigator.getGamepads?.()||[];}
      catch(error) {
        // Embedded pages may deny this API through Permissions Policy.
        // Keep keyboard prompts and future polling alive in that case.
        if(error?.name!=='SecurityError')throw error;
      }
      state.sampleGamepads(pads);
      publish();
    } else state.sampleGamepads([]);
    if(!closed)frame=environment.requestAnimationFrame(poll);
  }
  target.addEventListener('keydown',keyboard);
  frame=environment.requestAnimationFrame(poll);
  return {
    setStyle(style) {if(closed)return;state.setStyle(style);publish();},
    refresh() {if(!closed)publish();},
    close() {
      if(closed)return;
      closed=true;environment.cancelAnimationFrame(frame);
      target.removeEventListener('keydown',keyboard);
    }
  };
}
