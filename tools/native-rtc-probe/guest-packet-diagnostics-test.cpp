#include "port/guest_input_bridge.hpp"
#include <cassert>
#include <iostream>
using namespace partyboard::net;
int main(){
    SessionId session{};session.fill(7);
    bool deliver=true,failByException=false;
    GuestInputBridge guest(session,1,15,[&](auto){if(failByException)throw 1;return deliver;});
    Lockstep receiver(session,{1,2,3,4});
    const auto local=encode({session,0,456,1,{}});
    assert(guest.sendLocal(local));assert(receiver.receive(2,local)==Receive::Accepted);
    for(uint8_t slot:{0,2,3})assert(guest.enqueueHost(encode({session,0,456,slot,{}}))==InputMailbox::Push::Queued);
    assert(guest.pump(receiver));assert(receiver.advance(456));
    const auto first=guest.diagnostics();
    assert(first.sendAttempts==1&&first.sendFailures==0&&first.lastSentFrame==0);
    for(uint8_t slot:{0,2,3})assert(first.received[slot]==1&&first.lastReceivedFrame[slot]==0);
    assert(first.received[1]==0&&!first.lastReceivedFrame[1]);
    assert(guest.enqueueHost(local)==InputMailbox::Push::Malformed);
    deliver=false;assert(!guest.sendLocal(encode({session,1,789,1,{}})));
    failByException=true;assert(!guest.sendLocal(encode({session,2,789,1,{}})));
    const auto final=guest.diagnostics();
    assert(final.rejected==1&&final.sendAttempts==3&&final.sendFailures==2&&final.lastSentFrame==0);
    // Previously captured snapshots stay stable as later traffic arrives.
    assert(first.rejected==0&&first.sendAttempts==1);
    guest.closeStopped();assert(!guest.sendLocal(local));
    std::cout<<"Guest packet diagnostics preserve delivery, rejection and send failure behavior PASS\n";
}
