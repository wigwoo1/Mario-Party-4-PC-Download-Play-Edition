// Policy only: caller verifies identity, recovered state and synchronized resume.
// Pass monotonic milliseconds (e.g. performance.now()), never simulation time.
export class ReconnectPolicy {
  #owners; #missing=[false,false,false,false]; #state='Running';
  #last=null; #started=0; #epoch=0n; #revision=0n;
  constructor(owners){
    if(!Array.isArray(owners)||owners.length!==4||!owners.some(Boolean)||
      !owners.every(n=>Number.isInteger(n)&&n>=0&&n<=0xffffffff))throw TypeError('Active owners required');
    this.#owners=owners.slice();
  }
  get state(){return this.#state;}
  get epoch(){return this.#epoch;}
  get revision(){return this.#revision;}
  get missingSlots(){return this.#missing.slice();}
  update(now){
    if(this.#state==='ReturnToMenu')return this.#state;
    if(!Number.isFinite(now)||now<0||now>Number.MAX_SAFE_INTEGER||
      (this.#last!==null&&now<this.#last))return this.#state='ReturnToMenu';
    this.#last=now;
    if(this.#state==='Paused'&&now-this.#started>=60000)this.#state='ReturnToMenu';
    return this.#state;
  }
  #owns(peer){return !!peer&&this.#owners.includes(peer);}
  // Detection may lag the local pending-input interruption. Keep monotonic
  // observation time separate from the earlier, locally captured wait anchor.
  disconnected(peer,now,interruptedAt=now){
    if(this.update(now)==='ReturnToMenu'||!this.#owns(peer))return false;
    if(!Number.isFinite(interruptedAt)||interruptedAt<0||interruptedAt>now){this.cancel();return false;}
    this.#revision++;
    if(this.#state==='Running'){this.#state='Paused';this.#started=interruptedAt;this.#epoch++;}
    this.#owners.forEach((owner,i)=>{if(owner===peer)this.#missing[i]=true;});return this.update(now)!=='ReturnToMenu';
  }
  markRecovered(peer,epoch,revision,now){
    if(this.update(now)!=='Paused'||epoch!==this.#epoch||revision!==this.#revision||!this.#owns(peer))return false;
    this.#owners.forEach((owner,i)=>{if(owner===peer)this.#missing[i]=false;});return true;
  }
  readyToResume(now){return this.update(now)==='Paused'&&!this.#missing.some(Boolean);}
  resume(epoch,revision,now){
    if(!this.readyToResume(now)||epoch!==this.#epoch||revision!==this.#revision)return false;
    this.#state='Running';return true;
  }
  remainingMs(now){return this.update(now)==='Paused'?60000-(now-this.#started):0;}
  cancel(){this.#state='ReturnToMenu';}
}
