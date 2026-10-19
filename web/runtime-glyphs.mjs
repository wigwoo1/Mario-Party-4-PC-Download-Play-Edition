import {glyphTexture,gxRgba8} from './glyph-texture.mjs';
import {attachInputGlyphs} from './browser-input-glyphs.mjs';
import {controllerFamily} from './input-glyph-state.mjs';
import {controlGlyph} from './control-glyphs.mjs';

const keys={2:'Z',3:'X',4:'C',5:'V',6:'E',8:'Q',11:'R',12:'Enter'};
// Matches Aurora's standard SDL mapping, including analog L/R triggers.
const buttons={2:'south',3:'east',4:'west',5:'north',6:'r2',8:'l2',11:'r1',12:'start'};
export async function prepareRuntimeGlyphs({signal,render=glyphTexture,bindings={}}={}) {
  // These are physical labels indexed by authored window prompt entry. Session
  // ownership and normalized game action numbers are unaffected.
  const keyboard={...keys,...bindings.keyboard},gamepad={...buttons,...bindings.gamepad};
  for(const map of [bindings.keyboard,bindings.gamepad])if(map) {
    for(const entry of Object.keys(map))if(!Object.hasOwn(keys,entry))throw TypeError('Unknown prompt entry.');
  }
  // Validate the whole selection before rendering any textures.
  for(const entry of Object.keys(keys)) {
    controlGlyph({binding:{kind:'key',label:keyboard[entry]}});
    if(gamepad[entry]!==null)controlGlyph({family:'generic',binding:{kind:'button',button:gamepad[entry]}});
  }
  const textures={};
  for(const family of ['keyboard','generic','xbox','playstation','switch']) {
    textures[family]={};
    for(const entry of Object.keys(keys)) {
      signal?.throwIfAborted();
      if(family!=='keyboard'&&gamepad[entry]===null)continue;
      const binding=family==='keyboard'?{kind:'key',label:keyboard[entry]}:{kind:'button',button:gamepad[entry]};
      textures[family][entry]=gxRgba8(await render({family:family==='keyboard'?'generic':family,binding}));
    }
  }
  signal?.throwIfAborted();
  return {initial:textures.keyboard,attach(module,target,{environment=globalThis}={}){
    return attachInputGlyphs({target,environment,bindings:()=>({}),onChange:({active})=>{
      // Unknown physical mappings keep the original emulated-action icon.
      const family=active?.kind==='gamepad'?(active.mapping==='standard'?controllerFamily(active.id):null):'keyboard';
      module.partyBoardGlyphTextures=family?textures[family]:{};
    }});
  }};
}
