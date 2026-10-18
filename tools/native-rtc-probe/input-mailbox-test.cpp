#include "port/input_mailbox.hpp"
#include "port/host_input_router.hpp"
#include "port/host_input_bridge.hpp"
#include "port/reconnect_policy.hpp"
#include <vector>
#include <thread>
#include <iostream>
using namespace partyboard::net;
static void require(bool value) { if (!value) throw std::runtime_error("Mailbox regression failed"); }
int main() {
    ReconnectPolicy recovery({1,2,2,4});
    require(recovery.disconnected(2,100));
    require(recovery.missingSlots()==std::array<bool,4>({false,true,true,false}));
    const auto epoch=recovery.epoch(),revision=recovery.revision();
    require(!recovery.resume(epoch,revision,101));
    require(recovery.disconnected(4,200));
    require(recovery.remainingMs(200)==59900); // Further loss never extends the deadline.
    require(!recovery.markRecovered(2,epoch,revision,201));
    require(recovery.markRecovered(2,epoch,recovery.revision(),202));
    require(!recovery.readyToResume(203));
    require(recovery.markRecovered(4,epoch,recovery.revision(),204));
    require(recovery.resume(epoch,recovery.revision(),205));
    require(!recovery.resume(epoch,recovery.revision(),206));
    require(recovery.disconnected(2,300));
    require(!recovery.markRecovered(2,epoch,recovery.revision(),301));
    require(recovery.remainingMs(60299)==1);
    require(!recovery.markRecovered(2,recovery.epoch(),recovery.revision(),60300));
    require(recovery.state()==ReconnectPolicy::State::ReturnToMenu);
    ReconnectPolicy reversed({1,0,0,0});require(reversed.disconnected(1,100));
    require(reversed.update(99)==ReconnectPolicy::State::ReturnToMenu);
    ReconnectPolicy invalidClock({1,0,0,0});
    require(invalidClock.update(9007199254740992ULL)==ReconnectPolicy::State::ReturnToMenu);
    SessionId session{}; session[0] = 1;
    Lockstep queue(session, {11,22,33,44}); InputMailbox mailbox;
    std::array<InputMailbox::Route,4> routes;
    std::array<std::thread,4> producers;
    for (unsigned slot=0;slot<4;++slot) {
        routes[slot]=mailbox.bind(11*(slot+1));
        producers[slot]=std::thread([&,slot] {
            for (unsigned frame=0;frame<100;++frame) {
                Pad pad{};pad[0]=uint8_t(slot+frame);
                require(mailbox.push(routes[slot],encode({session,frame,123,uint8_t(slot),pad}))==InputMailbox::Push::Queued);
            }
        });
    }
    for(auto& producer:producers)producer.join();
    require(queue.nextFrame()==0 && queue.missingSlots()==15);
    require(mailbox.drain(queue));
    for(unsigned frame=0;frame<100;++frame) {
        auto result=queue.advance(123);require(bool(result));
        for(unsigned slot=0;slot<4;++slot)require(result->pads[slot][0]==uint8_t(slot+frame));
    }
    const auto old=routes[1];routes[1]=mailbox.bind(22);
    require(mailbox.push(old,encode({session,100,123,1,{}}))==InputMailbox::Push::Retired);
    mailbox.retire(old); // Old callback cannot retire the replacement.
    require(mailbox.push(routes[1],encode({session,100,123,1,{}}))==InputMailbox::Push::Queued);
    mailbox.retire(routes[1]);require(mailbox.drain(queue));require(queue.missingSlots()==15);
    auto bad=encode({session,100,123,0,{}});bad[47]=1;
    require(mailbox.push(routes[0],bad)==InputMailbox::Push::Malformed);
    InputMailbox unauthorized;auto wrong=unauthorized.bind(22);
    require(unauthorized.push(wrong,encode({session,100,123,0,{}}))==InputMailbox::Push::Queued);
    require(!unauthorized.drain(queue));require(queue.nextFrame()==100);
    InputMailbox saturated;auto route=saturated.bind(11);
    auto packet=encode({session,100,123,0,{}});
    for(size_t i=0;i<InputMailbox::Capacity;++i)require(saturated.push(route,packet)==InputMailbox::Push::Queued);
    require(saturated.push(route,packet)==InputMailbox::Push::Overflow);
    require(!saturated.drain(queue));require(queue.nextFrame()==100);
    InputMailbox replacement;auto original=replacement.bind(11);
    for(size_t i=0;i<InputMailbox::Capacity;++i)require(replacement.push(original,packet)==InputMailbox::Push::Queued);
    auto next=replacement.bind(11);
    require(replacement.push(next,packet)==InputMailbox::Push::Queued);
    replacement.retire(original);
    require(replacement.drain(queue));require(queue.missingSlots()==14);
    // A failed ownership check is terminal even when called again.
    require(!unauthorized.drain(queue));
    HostInputRouter router(session,0,15);
    const auto hostRoute=router.local(encode({session,0,123,0,{}}));
    require(hostRoute && hostRoute->owner==1 && hostRoute->recipients==14);
    for(uint8_t slot=1;slot<4;++slot) {
        auto route=router.remote(slot,encode({session,0,123,slot,{}}));
        require(route && route->owner==uint32_t(slot+1) && route->recipients==uint8_t(14 & ~(1<<slot)));
        require(!router.remote(slot,encode({session,0,123,uint8_t((slot+1)%4),{}})));
    }
    auto otherSession=session;otherSession[0]=2;
    require(!router.remote(1,encode({otherSession,0,123,1,{}})));
    require(!router.remote(0,encode({session,0,123,0,{}})));
    HostInputRouter partial(session,0,3);
    require(!partial.remote(2,encode({session,0,123,2,{}})));
    Lockstep bridged(session,{1,2,3,4});std::vector<uint8_t> destinations;
    HostInputBridge bridge(session,0,15,[&](uint8_t slot,std::span<const uint8_t> bytes){require(bool(decode(bytes)));destinations.push_back(slot);return true;});
    auto local=encode({session,0,123,0,{}});require(bridged.receive(1,local)==Receive::Accepted);require(bridge.sendLocal(local));
    require(destinations==std::vector<uint8_t>({1,2,3}));destinations.clear();
    for(uint8_t slot=1;slot<4;++slot){auto binding=bridge.attach(slot);require(bridge.enqueue(binding,encode({session,0,123,slot,{}}))==InputMailbox::Push::Queued);}
    require(destinations.empty() && bridged.missingSlots()==14);
    require(bridge.pump(bridged));require(bridged.nextFrame()==0);
    require(destinations==std::vector<uint8_t>({2,3,1,3,1,2}));require(bool(bridged.advance(123)));
    // RTC producer threads must never invoke the transport sender or commit input.
    const auto gameThread=std::this_thread::get_id();
    unsigned relays=0;
    Lockstep threadedQueue(session,{1,2,3,4});
    HostInputBridge threadedBridge(session,0,15,[&](uint8_t,std::span<const uint8_t>){
        require(std::this_thread::get_id()==gameThread);++relays;return true;
    });
    std::array<std::thread,3> callbacks;
    for(uint8_t slot=1;slot<4;++slot) {
        const auto binding=threadedBridge.attach(slot);
        callbacks[slot-1]=std::thread([&,slot,binding]{
            for(unsigned frame=0;frame<100;++frame) {
                Pad pad{};pad[0]=uint8_t(slot+frame);
                require(threadedBridge.enqueue(binding,encode({session,frame,123,slot,pad}))==InputMailbox::Push::Queued);
            }
        });
    }
    for(auto& callback:callbacks)callback.join();
    require(relays==0 && threadedQueue.nextFrame()==0 && threadedQueue.missingSlots()==15);
    require(threadedBridge.pump(threadedQueue));
    require(relays==600 && threadedQueue.nextFrame()==0);
    for(unsigned frame=0;frame<100;++frame) {
        Pad pad{};pad[0]=uint8_t(frame);
        require(threadedQueue.receive(1,encode({session,frame,123,0,pad}))==Receive::Accepted);
        const auto committed=threadedQueue.advance(123);require(bool(committed));
        for(unsigned slot=0;slot<4;++slot)require(committed->pads[slot][0]==uint8_t(slot+frame));
    }
    HostInputBridge broken(session,0,15,[](uint8_t,std::span<const uint8_t>){return false;});
    require(!broken.sendLocal(local));require(!broken.pump(bridged));
    std::cout<<"PASS concurrent input mailbox, route retirement, ownership rejection and overflow halt\n";
}
