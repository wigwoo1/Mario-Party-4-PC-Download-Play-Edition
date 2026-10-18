import {actionGlyph} from './control-glyphs.mjs';

// Identity hints are cosmetic only; they never change the input mapping.
export function controllerFamily(id='') {
  if (/dualsense|dualshock|playstation|054c/i.test(id)) return 'playstation';
  if (/nintendo|switch|057e/i.test(id)) return 'switch';
  if (/xbox|xinput/i.test(id)) return 'xbox';
  return 'generic';
}

export class InputGlyphState {
  #active=null;
  #previous=new Map();
  #override='auto';
  setStyle(style) {
    if (!['auto','generic','xbox','playstation','switch'].includes(style)) throw TypeError('Unknown glyph style.');
    this.#override=style;
  }
  keyboardActivity() { this.#active={kind:'keyboard'}; }
  // Caller supplies local getGamepads snapshots. Only fresh presses or axis
  // excursions count, so a held button or stick drift cannot steal key prompts.
  sampleGamepads(pads) {
    const next=new Map();
    for (const pad of pads) {
      if (!pad || pad.connected===false) continue;
      const identity=`${pad.index}:${pad.id}`;
      const down=[...pad.buttons.map(b=>!!b.pressed || b.value>0.5),
        ...pad.axes.flatMap(v=>[v>0.6,v< -0.6])];
      const previous=this.#previous.get(identity);
      next.set(identity,down);
      if (previous && down.some((v,i)=>v&&!previous[i])) {
        this.#active={kind:'gamepad',index:pad.index,id:pad.id,mapping:pad.mapping};
      }
    }
    this.#previous=next;
    if (this.#active?.kind==='gamepad' && !next.has(`${this.#active.index}:${this.#active.id}`)) this.#active=null;
  }
  get active() { return this.#active ? {...this.#active} : null; }
  glyph(action,{keyboard,gamepad}) {
    if (!this.#active) return null;
    if (this.#active.kind==='keyboard') return actionGlyph(action,{bindings:keyboard});
    const family=this.#override==='auto'?controllerFamily(this.#active.id):this.#override;
    // Nonstandard devices require the caller's explicit physical mapping too.
    return actionGlyph(action,{family,bindings:gamepad});
  }
}
