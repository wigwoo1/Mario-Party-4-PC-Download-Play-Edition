'use strict';
// Test-only negotiation delay. Never changes the simulation, session deadline,
// packet contents or production reconnect policy.
function installRecoveryNegotiationGate(root=globalThis){
 const Original=root.RTCPeerConnection;
 const state={until:0,blocked:0,released:0};
 const wait=async()=>{while(root.performance.now()<state.until){state.blocked++;await new Promise(resolve=>root.setTimeout(resolve,Math.min(100,state.until-root.performance.now())));}state.released++;};
 root.testRecoveryNegotiation={holdFor(ms){if(!Number.isFinite(ms)||ms<0||ms>70000)throw Error('Negotiation hold must be 0 through 70000ms');state.until=root.performance.now()+ms;},snapshot(){return {...state};}};
 root.RTCPeerConnection=class extends Original{
  async createOffer(...args){await wait();return super.createOffer(...args);}
  async createAnswer(...args){await wait();return super.createAnswer(...args);}
 };
}
module.exports={installRecoveryNegotiationGate};
