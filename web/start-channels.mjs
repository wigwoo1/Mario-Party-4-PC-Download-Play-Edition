import {prepareAgreedHostCommit} from './start-host.mjs';

// Dedicated reliable start-control channels, separate from lobby reports and
// frame input. Caller binds peer identity and refreshes after lobby changes.
// Agreement notification never grants permission to execute game frames.
export class StartChannels {
  #barrier; #snapshot; #bindings=new Map(); #closed=false; #changed; #error;
  #requireOnlineInput; #delivered;
  constructor(barrier,{snapshot,changed=()=>{},error=()=>{},requireOnlineInput=false}) {
    if(typeof requireOnlineInput!=='boolean')throw Error('Boolean online input requirement required.');
    this.#requireOnlineInput=requireOnlineInput;
    if(typeof snapshot!=='function')throw Error('Current lobby snapshot required.');
    this.#barrier=barrier;this.#snapshot=snapshot;this.#changed=changed;this.#error=error;
  }
  refresh() {
    if(this.#closed)return false;
    const valid=this.#barrier.update(this.#snapshot());
    this.#changed(this.#barrier.state,this.#barrier.pending);
    return valid;
  }
  acknowledgeLocal(peer,message) {
    if(!this.refresh())return false;
    const accepted=this.#barrier.acknowledge(peer,message,this.#snapshot());
    this.#changed(this.#barrier.state,this.#barrier.pending);
    return accepted;
  }
  prepareCommit(localPeer) {
    if(!this.refresh()||this.#barrier.state!=='Agreed')throw Error('Start agreement incomplete.');
    const proposal=this.#barrier.proposal;
    if(!proposal.players.some(player=>player.peer===localPeer))throw Error('Local player unassigned.');
    for(const {peer} of proposal.players) {
      if(peer===localPeer)continue;
      if(this.#bindings.get(peer)?.channel.readyState!=='open') {
        this.#barrier.cancel();this.#changed(this.#barrier.state,this.#barrier.pending);
        throw Error('Every remote player needs a current open start channel.');
      }
    }
    // Return a fresh commit for caller-owned delivery. Preparing this does not
    // send, authorize execution, or establish simultaneous delivery to peers.
    return {...proposal,type:'session-start-commit'};
  }
  prepareRuntimeCommit(localPeer,runtime) {
    // Check both transport membership and actual idle host state before the
    // caller sends anything. Delivery and execution remain caller-owned.
    this.prepareCommit(localPeer);
    return prepareAgreedHostCommit(this.#barrier,runtime,this.#snapshot,{requireOnlineInput:this.#requireOnlineInput});
  }
  // Delivery is not atomic across peers. Clients still need synchronized
  // input ownership; this operation never resumes the host simulation.
  sendRuntimeCommit(localPeer,runtime) {
    const commit=this.prepareRuntimeCommit(localPeer,runtime);
    try {
      for(const {peer} of commit.players) {
        if(peer===localPeer)continue;
        // A send callback may synchronously revoke membership or readiness.
        this.prepareRuntimeCommit(localPeer,runtime);
        this.#bindings.get(peer).channel.send(JSON.stringify(commit));
      }
      this.prepareRuntimeCommit(localPeer,runtime);
      this.#delivered={localPeer,runtime};
      return commit;
    } catch(cause) {
      try {this.close();}catch(cleanup){throw new AggregateError([cause,cleanup],'Start delivery and channel cleanup failed.');}
      throw cause;
    }
  }
  // Transfer monitoring to the running session only after complete delivery.
  // The new owner must handle connection loss, execution and runtime disposal.
  handoff(localPeer,runtime) {
    if(this.#closed||this.#delivered?.localPeer!==localPeer||this.#delivered.runtime!==runtime)
      throw Error('Delivered live start session required for handoff.');
    const commit=this.prepareRuntimeCommit(localPeer,runtime);
    const channels=[...this.#bindings].map(([peer,binding])=>({peer,channel:binding.channel}));
    for(const binding of this.#bindings.values())binding.detach();
    this.#bindings.clear();this.#closed=true;
    return {commit,channels,runtime};
  }
  attach(peer,channel) {
    if(this.#closed||!this.#barrier.proposal.players.some(p=>p.peer===peer)||
      !['open','connecting'].includes(channel.readyState)||channel.ordered===false||
      channel.maxRetransmits!=null||channel.maxPacketLifeTime!=null)throw Error('Invalid start channel.');
    if([...this.#bindings].some(([owner,b])=>owner!==peer&&b.channel===channel))throw Error('Start channel already assigned.');
    const old=this.#bindings.get(peer);
    if(old?.channel===channel)return;
    if(old) {old.detach();this.#bindings.delete(peer);this.#barrier.cancel();old.channel.close();}
    const active=()=>!this.#closed&&this.#bindings.get(peer)===binding;
    const fail=error=>{
      if(!active())return;
      this.#barrier.cancel();binding.detach();this.#bindings.delete(peer);channel.close();
      this.#changed(this.#barrier.state,this.#barrier.pending);this.#error(peer,error);
    };
    const send=()=>{
      if(!active()||!this.refresh())return;
      try {channel.send(JSON.stringify(this.#barrier.proposal));}catch(error){fail(error);}
    };
    const message=event=>{
      if(!active())return;
      try {
        if(typeof event.data!=='string'||event.data.length>2048)throw Error('Invalid start acknowledgment.');
        const ack=JSON.parse(event.data);
        if(!this.acknowledgeLocal(peer,ack)&&this.#barrier.state==='Mismatch')
          throw Error('Initialized simulation state differs.');
      } catch(error){fail(error);}
    };
    const lost=()=>fail(Error('Start connection closed.'));
    const binding={channel,detach(){
      channel.removeEventListener('open',send);channel.removeEventListener('message',message);
      channel.removeEventListener('close',lost);channel.removeEventListener('error',lost);
    }};
    this.#bindings.set(peer,binding);
    channel.addEventListener('open',send);channel.addEventListener('message',message);
    channel.addEventListener('close',lost);channel.addEventListener('error',lost);
    if(channel.readyState==='open')send();
  }
  close() {
    if(this.#closed&&this.#bindings.size===0)return;this.#closed=true;this.#barrier.cancel();
    const bindings=[...this.#bindings];this.#bindings.clear();const failures=[];
    for(const [peer,binding] of bindings){binding.detach();try{binding.channel.close();}catch(cause){this.#bindings.set(peer,binding);failures.push(cause);}}
    try{this.#changed(this.#barrier.state,this.#barrier.pending);}catch(cause){failures.push(cause);}
    if(failures.length)throw new AggregateError(failures,'Start channel cleanup failed.');
  }
}
