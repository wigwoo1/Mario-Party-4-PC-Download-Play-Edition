// Probe-only observer: run SDL's original callback first, then read its output.
// Never write PCM or change node connections, channel layout or game state.
export function attachAudioChannelMeter(node){
  const original=node?.onaudioprocess;
  if(typeof original!=='function')throw Error('Existing SDL audio callback required.');
  let callbacks,frames,energy,channels,nonFinite;
  const reset=()=>{callbacks=frames=nonFinite=0;energy=Array(6).fill(0);channels=new Set();};
  reset();
  function observe(event){
    const result=original.call(this,event),buffer=event.outputBuffer;
    callbacks++;frames+=buffer.length;channels.add(buffer.numberOfChannels);
    for(let channel=0;channel<buffer.numberOfChannels;channel++){
      energy[channel]??=0;
      for(const value of buffer.getChannelData(channel)){
        if(Number.isFinite(value))energy[channel]+=Math.abs(value);else nonFinite++;
      }
    }
    return result;
  }
  node.onaudioprocess=observe;
  return {reset,snapshot(){return {callbacks,frames,energy:[...energy],channels:[...channels],nonFinite};},
    restore(){if(node.onaudioprocess===observe){node.onaudioprocess=original;return true;}return false;}};
}
