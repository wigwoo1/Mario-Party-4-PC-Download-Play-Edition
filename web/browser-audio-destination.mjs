// The SDL source layout and Web Audio destination are separate. A browser can
// expose only stereo even when an SDL stream accepts six source channels.
export function configureAudioDestination(module,channels){
  if(channels!==2&&channels!==6)throw Error('Stereo or six-channel speaker layout required.');
  const destination=module?.SDL3?.audioContext?.destination;
  if(!destination)return {requested:channels,available:false,applied:0};
  const maximum=destination.maxChannelCount;
  if(!Number.isInteger(maximum)||maximum<2)
    return {requested:channels,available:true,applied:destination.channelCount,maximum};
  const selected=channels===6&&maximum>=6?6:2;
  try{destination.channelCount=selected;}
  catch{return {requested:channels,available:true,applied:destination.channelCount,maximum,rejected:true};}
  return {requested:channels,available:true,applied:destination.channelCount,maximum};
}
