import test from 'node:test';
import assert from 'node:assert/strict';
import {lobbyMusic} from './lobby-music.mjs';
import {readFileSync} from 'node:fs';

test('both shipped lobby tracks contain stereo 48 kHz Opus at constant 96 kbps',()=>{
  for(const name of ['wifi-menu','currently-spectating']){
    const bytes=readFileSync(new URL('./audio/'+name+'.opus',import.meta.url));
    let offset=0,parts=[],packets=[],granule=0n;
    while(offset<bytes.length){
      assert.ok(offset+27<=bytes.length,'Complete Ogg page header required.');
      assert.equal(bytes.toString('ascii',offset,offset+4),'OggS');
      assert.equal(bytes[offset+4],0);
      const segments=bytes[offset+26];let cursor=offset+27+segments;
      assert.ok(cursor<=bytes.length,'Complete Ogg segment table required.');
      granule=bytes.readBigUInt64LE(offset+6);
      for(let index=0;index<segments;index++){
        const length=bytes[offset+27+index];assert.ok(cursor+length<=bytes.length);
        parts.push(bytes.subarray(cursor,cursor+length));cursor+=length;
        if(length<255){packets.push(Buffer.concat(parts));parts=[];}
      }
      offset=cursor;
    }
    assert.equal(parts.length,0);assert.ok(packets.length>2);
    assert.equal(packets[0].toString('ascii',0,8),'OpusHead');
    assert.equal(packets[1].toString('ascii',0,8),'OpusTags');
    assert.equal(packets[0][9],2);assert.equal(packets[0].readUInt32LE(12),48000);
    assert.ok(granule>BigInt(packets[0].readUInt16LE(10)));
    for(const packet of packets.slice(2)){
      // TOC 0xfc: stereo CELT 20 ms, one frame per packet.
      assert.equal(packet[0],0xfc);assert.equal(packet.length,240);
      assert.equal(packet.length*8/0.02,96000);
    }
  }
});

function fixture(storage) {
  const pending=[];
  const audio=new EventTarget();
  Object.assign(audio,{paused:true,muted:false,play(){return new Promise((resolve,reject)=>pending.push({resolve,reject}));},pause(){this.paused=true;},removeAttribute(){delete this.src;},load(){}});
  const button={disabled:true,textContent:'Mute music',setAttribute(key,value){this[key]=value;}},label={},volume={value:'0.3'};
  return {audio,button,label,volume,pending,music:lobbyMusic(audio,button,label,volume,storage)};
}
const settle=()=>new Promise(resolve=>setImmediate(resolve));

test('playlist alternates both tracks, wraps, and preserves mute and volume',async()=>{
  const f=fixture();f.music.start();
  assert.equal(f.audio.src,'./audio/wifi-menu.opus');
  f.pending[0].resolve();await settle();
  f.audio.paused=false;f.button.onclick();
  f.volume.value='0.6';f.volume.oninput();
  for(const [index,name] of ['currently-spectating','wifi-menu','currently-spectating'].entries()){
    f.audio.dispatchEvent(new Event('ended'));
    assert.equal(f.audio.src,'./audio/'+name+'.opus');
    f.pending[index+1].resolve();await settle();
    assert.equal(f.audio.muted,true);assert.equal(f.audio.volume,0.6);
  }
  f.music.stop();
  assert.equal(f.audio.paused,true);assert.equal(f.audio.src,undefined);
  assert.equal(f.button.disabled,true);
});

test('Play music retries after an error even if the media element is not paused',async()=>{
  const f=fixture();f.music.start();f.audio.paused=false;
  f.audio.dispatchEvent(new Event('error'));
  f.pending[0].resolve();await settle();
  assert.equal(f.button.textContent,'Play music');
  f.button.onclick();assert.equal(f.pending.length,2);assert.equal(f.audio.muted,false);
  f.pending[1].resolve();await settle();
  assert.equal(f.label.textContent,'WIFI Menu');assert.equal(f.button.textContent,'Mute music');
  f.button.onclick();assert.equal(f.audio.muted,true);assert.equal(f.pending.length,2);
});

test('leaving during pending playback ignores late success and failure',async()=>{
  for(const result of ['resolve','reject']) {
    const f=fixture();f.music.start();f.music.stop();
    f.pending[0][result](new Error('Late playback result'));await settle();
    assert.equal(f.label.textContent,'Music starts when you join.');
    assert.equal(f.button.disabled,true);assert.equal(f.audio.src,undefined);
    f.audio.dispatchEvent(new Event('ended'));f.audio.dispatchEvent(new Event('error'));
    assert.equal(f.pending.length,1);assert.equal(f.audio.src,undefined);
  }
});

test('previous session rejection cannot replace a new session title',async()=>{
  const f=fixture();f.music.start();f.music.stop();f.music.start();
  f.pending[1].resolve();await settle();
  f.pending[0].reject(new Error('Old session'));await settle();
  assert.equal(f.label.textContent,'WIFI Menu');assert.equal(f.button.textContent,'Mute music');
});

test('local music settings restore before playback and persist independently of game audio',async()=>{
  const saved=new Map([['partyboard.lobbyMusic',JSON.stringify({volume:0.55,muted:true})],['partyboard.gameAudio','unchanged']]);
  const storage={getItem:key=>saved.get(key),setItem:(key,value)=>saved.set(key,value)};
  const f=fixture(storage);
  assert.equal(f.audio.volume,0.55);assert.equal(f.audio.muted,true);assert.equal(f.button['aria-pressed'],'true');
  f.music.start();f.pending[0].resolve();await settle();f.audio.paused=false;
  f.volume.value='0.2';f.volume.oninput();f.button.onclick();
  f.music.stop();
  const restored=fixture(storage);
  assert.equal(restored.audio.volume,0.2);assert.equal(restored.audio.muted,false);
  assert.equal(saved.get('partyboard.gameAudio'),'unchanged');
});

test('invalid or inaccessible storage cannot break playback or amplify music',async()=>{
  for(const saved of ['{','null','[]','{"volume":2,"muted":false}','{"volume":-1,"muted":false}','{"volume":0.5,"muted":"false"}']) {
    const f=fixture({getItem:()=>saved});assert.equal(f.audio.volume,0.3);assert.equal(f.audio.muted,false);
    for(const value of ['NaN','Infinity','-0.1','1.1']){f.volume.value=value;f.volume.oninput();assert.equal(f.audio.volume,0.3);}
    assert.equal(f.volume.value,'0.3');
  }
  const f=fixture({getItem(){throw Error('blocked');},setItem(){throw Error('blocked');}});
  f.music.start();f.pending[0].resolve();await settle();f.audio.paused=false;
  f.button.onclick();f.volume.value='0';f.volume.oninput();
  assert.equal(f.audio.muted,true);assert.equal(f.audio.volume,0);
});
