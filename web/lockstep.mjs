import {decodeInput} from './input-packet.mjs';
const equal = (a,b) => a.length === b.length && a.every((v,i) => v === b[i]);
export class Lockstep {
  static get window() {return 120n;}
  static get frameLimit() {return 10000000n;}
  #session; #owners; #active = 0; #next = 0n; #stop = 'None';
  #pending = Array.from({length:120}, () => new Map());
  constructor(session, owners) {
    if (!(session instanceof Uint8Array) || session.length !== 16 || !session.some(Boolean)
        || !Array.isArray(owners) || owners.length !== 4
        || !owners.every(n => Number.isInteger(n) && n >= 0 && n <= 0xffffffff)
        || !owners.some(Boolean)) throw new TypeError('Session and active owners required');
    this.#session = session.slice(); this.#owners = owners.slice();
    owners.forEach((owner,i) => {if(owner) this.#active |= 1 << i;});
  }
  get nextFrame() {return this.#next;}
  get stopped() {return this.#stop;}
  receive(peer, bytes) {
    if(this.#stop !== 'None') return 'Halted';
    const input = decodeInput(bytes);
    if(!input) return 'Malformed';
    if(!equal(input.session,this.#session)) return 'WrongSession';
    if(!peer || this.#owners[input.slot] !== peer) return 'Unauthorized';
    if(input.frame < this.#next) return 'Stale';
    if(input.frame >= Lockstep.frameLimit || input.frame-this.#next >= Lockstep.window) return 'TooFar';
    const pending = this.#pending[Number(input.frame % Lockstep.window)];
    const previous = pending.get(input.slot);
    if(previous) {
      if(previous.frame === input.frame && previous.checkpoint === input.checkpoint && equal(previous.pad,input.pad)) return 'Duplicate';
      this.#stop = 'Conflict'; return 'Conflict';
    }
    for(const previous of pending.values()) if(previous.checkpoint !== input.checkpoint) {
      this.#stop = 'Divergence'; return 'Divergence';
    }
    pending.set(input.slot,input); return 'Accepted';
  }
  missingSlots() {
    let mask = this.#active;
    for(const slot of this.#pending[Number(this.#next % Lockstep.window)].keys()) mask &= ~(1 << slot);
    return mask;
  }
  advance(checkpoint) {
    if(!Number.isInteger(checkpoint) || checkpoint < 0 || checkpoint > 0xffffffff) throw new TypeError('Invalid checkpoint');
    if(this.#stop !== 'None' || this.missingSlots()) return null;
    const pending = this.#pending[Number(this.#next % Lockstep.window)];
    for(const input of pending.values()) if(input.checkpoint !== checkpoint) {
      this.#stop = 'Divergence'; return null;
    }
    const pads = Array.from({length:4}, (_,slot) => {
      if(pending.has(slot)) return pending.get(slot).pad.slice();
      const pad = new Uint8Array(11); pad[10] = 255; return pad;
    });
    const result = {index:this.#next, checkpoint, pads};
    pending.clear(); this.#next++;
    if(this.#next === Lockstep.frameLimit) this.#stop = 'FrameLimit';
    return result;
  }
  cancel() {if(this.#stop === 'None') this.#stop = 'Cancelled';}
}
