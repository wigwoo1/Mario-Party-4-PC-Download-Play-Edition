// Asset chunks have their own reliable transport. Lobby progress is a periodic
// summary so a fast download cannot fill the host's bounded control mailbox.
export function downloadProgressReporter(send,{now=()=>performance.now(),intervalMs=250}={}) {
  let lastTime=-Infinity,lastBytes=-1,lastVerified=false;
  return {
    report(bytes,total,verified=false) {
      if(bytes===lastBytes&&verified===lastVerified)return false;
      const time=now(),terminal=verified||(bytes===total&&lastBytes!==total);
      if(!terminal&&time-lastTime<intervalMs)return false;
      if(send(bytes,verified)===false)return false;
      lastTime=time;lastBytes=bytes;lastVerified=verified;return true;
    },
    reset(){lastTime=-Infinity;lastBytes=-1;lastVerified=false;},
  };
}
