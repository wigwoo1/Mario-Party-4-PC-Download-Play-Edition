import {configureAudioDestination} from './browser-audio-destination.mjs';
export function audioLayout(select,label,storage) {
  const key='partyboard.audioChannels';
  let module,channels=2;
  try { if(storage.getItem(key)==='6') channels=6; } catch {}
  select.value=String(channels);select.disabled=false;
  function refreshOutput() {
    if(!module) {
      label.textContent='Saved as a preference for the next compatible game runtime.';
      return;
    }
    const device=module?._PartyBoardBrowserAudioDeviceChannels?.()||0;
    const source=module?._PartyBoardBrowserAudioSourceChannels?.()||0;
    if(source===channels)configureAudioDestination(module,channels);
    const destination=module?.SDL3?.audioContext?.destination;
    const routing=module?._PartyBoardBrowserAudioSynthMetric?.(13)===1;
    const material=routing?(source===6?'Original mono surround feeds both rear speakers; center/LFE content is not synthesized. ':'Stereo uses the front speakers. Six-channel output supports the original mono surround signal. '):
      'Stereo sources use the front speakers; surround effects are in development. ';
    label.textContent='Applies to the next game and newly opened music streams. '+material+
      (device ? `SDL output: ${device} channels; source: ${source} channels. Speaker mapping is unverified.` : 'Playback device channels are unavailable until a music stream opens. Your device may downmix to stereo.')+
      (destination ? ` Browser destination: ${destination.channelCount} channels (maximum ${destination.maxChannelCount}).` : '');
  }
  function apply() {
    if(!module) {
      select.disabled=false;
      refreshOutput();
      return true;
    }
    if(typeof module?._PartyBoardBrowserSetAudioChannels!=='function') {
      select.disabled=true;label.textContent='Speaker selection is available after preparing a compatible game runtime.';return false;
    }
    if(module._PartyBoardBrowserSetAudioChannels(channels)!==0) {
      select.disabled=true;label.textContent='Speaker selection is unavailable for this runtime.';return false;
    }
    select.disabled=false;
    refreshOutput();
    return true;
  }
  select.addEventListener('change',()=>{
    const value=Number(select.value);
    if(value!==2&&value!==6) { select.value=String(channels);return; }
    const previous=channels;channels=value;
    if(!apply()) { channels=previous;select.value=String(channels);return; }
    try { storage.setItem(key,String(channels)); } catch {}
  });
  select.addEventListener('focus',refreshOutput);
  apply();
  return {get channels(){return channels;},bind(runtimeModule){module=runtimeModule;return apply();}};
}
