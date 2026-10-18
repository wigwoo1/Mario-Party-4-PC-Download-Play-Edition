import test from 'node:test';
import assert from 'node:assert/strict';
import {prepareRuntimeGlyphs} from './runtime-glyphs.mjs';
import {defaultControlBindings,controlBindingGlyphs} from './control-bindings.mjs';
test('profile glyphs follow remapped keys and omit unsupported physical icons instead of showing defaults',async()=>{
  const profile=defaultControlBindings();profile.keyboard.a='Space';profile.controller.z='button:16';
  const bindings=controlBindingGlyphs(profile),requests=[];
  assert.equal(bindings.keyboard[2],'Space');assert.equal(bindings.gamepad[11],null);
  const prepared=await prepareRuntimeGlyphs({bindings,render:async request=>{
    requests.push(request);return {width:64,height:64,rgba:new Uint8Array(16384)};
  }});
  assert.ok(prepared.initial[11]);
  assert.equal(requests.filter(r=>r.binding.button==='r1').length,0);
  assert.ok(requests.some(r=>r.binding.label==='Space'));
});
test('selected physical bindings replace authored defaults across keyboard and controller textures',async()=>{
  const requests=[];
  await prepareRuntimeGlyphs({bindings:{keyboard:{2:'Space'},gamepad:{2:'north'}},render:async request=>{
    requests.push(request);return {width:64,height:64,rgba:new Uint8Array(16384)};
  }});
  assert.equal(requests.filter(r=>r.binding.label==='Space').length,1);
  assert.equal(requests.filter(r=>r.binding.label==='Z').length,0);
  assert.equal(requests.filter(r=>r.family==='playstation'&&r.binding.button==='north').length,2);
  assert.ok(requests.some(r=>r.binding.label==='X'));
});
test('invalid binding selection fails before producing partial prompt textures',async()=>{
  let rendered=0;
  for(const bindings of [{keyboard:{99:'A'}},{keyboard:{2:''}},{gamepad:{2:'unknown'}}]){
    await assert.rejects(prepareRuntimeGlyphs({bindings,render:async()=>{rendered++;}}),TypeError);
  }
  assert.equal(rendered,0);
});
test('production glyph bridge follows standard PlayStation activity then keyboard and removes listeners',async()=>{
  const requests=[],render=async request=>{
    requests.push(request);const rgba=new Uint8Array(64*64*4);rgba.fill(request.family==='playstation'?91:12);
    return {width:64,height:64,rgba};
  };
  const glyphs=await prepareRuntimeGlyphs({render});
  assert.ok(requests.some(r=>r.family==='playstation'&&r.binding.button==='r1'));
  assert.ok(requests.some(r=>r.family==='playstation'&&r.binding.button==='r2'));
  assert.ok(requests.some(r=>r.binding.label==='Z'));
  const target=new EventTarget(),module={},pad={index:0,id:'DualSense',mapping:'standard',connected:true,buttons:[{pressed:false,value:0}],axes:[]};
  let poll,cancelled=false;
  const environment={document:{hidden:false,activeElement:target},navigator:{getGamepads:()=>[pad]},
    requestAnimationFrame:callback=>(poll=callback,1),cancelAnimationFrame:()=>cancelled=true};
  const attached=glyphs.attach(module,target,{environment});
  poll();assert.equal(module.partyBoardGlyphTextures,glyphs.initial);
  pad.buttons[0].pressed=true;poll();assert.equal(module.partyBoardGlyphTextures[2][0],91);
  target.dispatchEvent(new Event('keydown'));assert.equal(module.partyBoardGlyphTextures,glyphs.initial);
  pad.mapping='';pad.buttons[0].pressed=false;poll();pad.buttons[0].pressed=true;poll();
  assert.deepEqual(module.partyBoardGlyphTextures,{});
  attached.close();assert.equal(cancelled,true);
  module.partyBoardGlyphTextures=null;target.dispatchEvent(new Event('keydown'));assert.equal(module.partyBoardGlyphTextures,null);
});
test('glyph preparation aborts before generating a partial executable texture set',async()=>{
  const controller=new AbortController();let count=0;
  await assert.rejects(prepareRuntimeGlyphs({signal:controller.signal,render:async()=>{
    count++;controller.abort();return {width:64,height:64,rgba:new Uint8Array(16384)};
  }}),{name:'AbortError'});
  assert.equal(count,1);
});
