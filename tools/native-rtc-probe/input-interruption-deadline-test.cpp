#include "port/recoverable_pad_exchange.hpp"
#include <cassert>
#include <iostream>
using namespace partyboard::net;

int main() {
    SessionId session{};session[0]=1;
    PADStatus pads[4]{};pads[0].button=7;
    const RecoverablePadExchange::State state{123,4,5};
    RecoverablePadExchange delayed(session,{1,2,3,0},1,{0,-1,-1,-1});
    assert(delayed.begin(pads,state,100));
    const Packet original=delayed.retainedPackets()[0];
    assert(delayed.poll(state,pads,100)==Exchange::Waiting);
    assert(delayed.poll(state,pads,1000)==Exchange::Waiting);
    assert(delayed.tick(20440)==ReconnectPolicy::State::Running);
    assert(delayed.disconnected(2,20440));
    assert(delayed.remainingRecoveryMs(20440)==39660);
    assert(delayed.disconnected(3,30440));
    assert(delayed.remainingRecoveryMs(30440)==29660);
    assert(delayed.nextFrame()==0 && delayed.pending());
    assert(delayed.retainedPackets()[0]==original && pads[0].button==7 && pads[1].button==0);
    assert(delayed.remainingRecoveryMs(60099)==1);
    assert(delayed.tick(60100)==ReconnectPolicy::State::ReturnToMenu);
    assert(!delayed.pending() && delayed.nextFrame()==0);
    assert(!delayed.recovered(2,delayed.epoch(),delayed.revision(),60100));

    RecoverablePadExchange completed(session,{1,2,0,0},1,{0,-1,-1,-1});
    assert(completed.begin(pads,state,100));
    Pad remote{};remote[0]=42;
    assert(completed.receive(2,encode({session,0,123,1,remote}))==Receive::Accepted);
    assert(completed.poll(state,pads,200)==Exchange::Ready);
    assert(completed.nextFrame()==1 && !completed.pending() && pads[1].button==42);
    // A completed poll's old anchor cannot shorten an unrelated later loss.
    assert(completed.disconnected(2,30000));
    assert(completed.remainingRecoveryMs(30000)==60000);
    assert(completed.recovered(2,completed.epoch(),completed.revision(),30000));
    assert(completed.resume(completed.epoch(),completed.revision(),30000));
    assert(completed.begin(pads,state,31000));
    assert(completed.disconnected(2,51000));
    assert(completed.remainingRecoveryMs(51000)==40000);
    assert(completed.tick(91000)==ReconnectPolicy::State::ReturnToMenu);

    RecoverablePadExchange expired(session,{1,2,0,0},1,{0,-1,-1,-1});
    assert(expired.begin(pads,state,0));
    assert(!expired.disconnected(2,60000));
    assert(expired.tick(60000)==ReconnectPolicy::State::ReturnToMenu);
    std::cout<<"PASS native pending-input interruption budgets, shared deadline, exact expiry, unchanged retained input and completed-poll reset\n";
}
