const key='partyboard.gameAudio';
function valid(value){return value&&Number.isInteger(value.volume)&&value.volume>=0&&value.volume<=100&&typeof value.muted==='boolean';}
// Local playback preferences never enter the shared session/checkpoint state.
export function gameAudioControls(volume,mute,label,storage){
  let preferences={volume:80,muted:false},module;
  try{const saved=JSON.parse(storage?.getItem(key)??'null');if(valid(saved))preferences={volume:saved.volume,muted:saved.muted};}catch{}
  function apply(){
    volume.value=String(preferences.volume);mute.textContent=preferences.muted?'Unmute game audio':'Mute game audio';
    mute.setAttribute('aria-pressed',String(preferences.muted));
    const setter=module?._PartyBoardBrowserSetAudioOutput;
    const supported=typeof setter==='function';
    const accepted=supported&&setter(preferences.volume,Number(preferences.muted))===0;
    label.textContent=accepted?(preferences.muted?'Game audio muted.':`Game volume: ${preferences.volume}%.`):
      module?'This runtime cannot apply game volume. Your preference is saved for the next compatible game.':'Game volume applies when this device prepares a game.';
    return accepted;
  }
  function save(){try{storage?.setItem(key,JSON.stringify(preferences));}catch{}apply();}
  volume.addEventListener('input',()=>{const value=Number(volume.value);if(Number.isInteger(value)&&value>=0&&value<=100){preferences.volume=value;save();}else apply();});
  mute.addEventListener('click',()=>{preferences.muted=!preferences.muted;save();});
  apply();
  return {bind(owner){module=owner;return apply();},unbind(owner){if(module===owner){module=undefined;apply();}},get preferences(){return {...preferences};}};
}
