// Test-only per-slot local speaker preferences. They are not session rules or
// data sent by a remote player; each runtime applies its own local setting.
function parseLocalAudioLayouts(value){
 if(value===undefined||value==='')return null;
 const layouts=JSON.parse(value);
 if(!Array.isArray(layouts)||layouts.length!==4||!layouts.every(channels=>channels===2||channels===6))
  throw Error('Four local stereo/six-channel preferences required');
 return Object.freeze(layouts.slice());
}
function localAudioChannels(layouts,peer){
 if(layouts===null)return undefined;
 const slot=peer==='host'?0:/^guest[1-3]$/.test(peer)?Number(peer.at(-1)):-1;
 if(slot<0)throw Error('Assigned original peer required for local audio selection');
 return layouts[slot];
}
module.exports={parseLocalAudioLayouts,localAudioChannels};
