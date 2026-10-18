#include "port/native_guest_start_agreement.hpp"
#include "port/guest_input_bridge.hpp"
#include "port/native_guest_input_loss.hpp"
#include "port/native_guest_recovery_control.hpp"
#include "port/native_guest_replacement_verification.hpp"
#include <iostream>
using namespace partyboard::net;
template<class F>void rejects(F&& action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("Invalid start accepted");}
int main(){try{
    const std::string asset(64,'a'),build(64,'b'),session(32,'c');
    RecoverySession recovery{session,build,asset,"unlocked-no-save-v1",123,8,2,{{"host",0},{"guest1",1},{"guest2",2},{"guest3",3}}};
    nlohmann::ordered_json loss={{"type","session-input-loss"},{"version",1},{"session",session},{"peer","guest3"}};
    if(receiveNativeGuestInputLoss(loss,recovery,"guest1","host")!=4)throw std::runtime_error("Assigned loss owner differs");
    rejects([&]{receiveNativeGuestInputLoss(loss,recovery,"guest1","guest2");});
    rejects([&]{receiveNativeGuestInputLoss(loss,recovery,"unassigned","host");});
    for(const auto& peer:{"guest1","unassigned"}){auto invalid=loss;invalid["peer"]=peer;rejects([&]{receiveNativeGuestInputLoss(invalid,recovery,"guest1","host");});}
    auto invalidLoss=loss;invalidLoss["session"]=std::string(32,'d');rejects([&]{receiveNativeGuestInputLoss(invalidLoss,recovery,"guest1","host");});
    invalidLoss=loss;invalidLoss["extra"]=true;rejects([&]{receiveNativeGuestInputLoss(invalidLoss,recovery,"guest1","host");});
    {
        SessionId recoveryId{};recoveryId.fill(0xcc);uint64_t clock=100;
        RecoverablePadExchange queue(recoveryId,{1,2,3,4},2,{-1,0,-1,-1});
        if(!queue.bindOriginalSession(recovery,"guest1","host"))throw std::runtime_error("Original recovery binding failed");
        PADStatus pads[4]{};if(!queue.begin(pads,{456,4,5},clock))throw std::runtime_error("Suspended poll fixture failed");
        nlohmann::ordered_json sent;
        std::vector<nlohmann::ordered_json> guestPackets;
        NativeGuestRecoveryControl control(queue,recovery,"guest1","host",[&]{return clock;},
            [&](uint32_t owner){return queue.capture(owner,0,{456,4,5},clock);},[]{return true;},[&](const auto& packet){sent=packet;guestPackets.push_back(packet);return true;});
        control.receive("host",loss);const auto revision=queue.revision();clock+=10;control.receive("host",loss);
        if(queue.revision()!=revision||queue.remainingRecoveryMs(clock)!=59990)throw std::runtime_error("Duplicate notice changed original pause");
        auto paused=queue.capture(4,0,{456,4,5},clock);
        // Exercise both endpoints of the original host's alignment phase.
        RecoverablePadExchange hostQueue(recoveryId,{1,2,3,4},1,{0,-1,-1,-1});
        if(!hostQueue.bindOriginalSession(recovery,"host","host")||!hostQueue.begin(pads,{456,4,5},clock)||!hostQueue.disconnected(4,clock))
            throw std::runtime_error("Host alignment fixture failed");
        const std::string alignmentNonce(32,'f');
        control.receive("host",{{"type","session-poll-align"},{"version",1},{"nonce",alignmentNonce},
            {"session",session},{"buildId",build},{"assetId",asset}});
        std::vector<nlohmann::ordered_json> hostPackets;
        NativePausedPollReconciliation hostAlignment(hostQueue,recovery,"host","guest1",alignmentNonce,
            [&]()->std::optional<PausedCapture>{return hostQueue.capture(4,0,{456,4,5},clock);},[]{return true;},[&]{return clock;},60100,
            [&](const auto& packet){hostPackets.push_back(packet);return true;});
        for(unsigned step=0;step<10&&!hostAlignment.complete();++step){
            clock+=20;hostAlignment.tick();
            const auto deliver=std::move(hostPackets);hostPackets.clear();
            for(const auto& packet:deliver)control.receive("host",packet);
            control.tick();
            for(const auto& packet:guestPackets)if(!hostAlignment.complete())hostAlignment.receive("guest1",packet);
            guestPackets.clear();
        }
        for(const auto& packet:hostPackets)control.receive("host",packet);
        if(!hostAlignment.complete()||queue.tick(clock)!=ReconnectPolicy::State::Paused||queue.nextFrame()!=0)
            throw std::runtime_error("Original two-sided alignment failed or resumed input");
        // A completed round can retransmit, but cannot authorize another round.
        control.receive("host",hostAlignment.report());
        auto proposal=recoveryProposal(recovery,paused,"host",std::string(32,'e'));
        auto committed=proposal;committed["type"]="session-recovery-commit";committed["acknowledged"]={"host","guest1","guest2","guest3"};
        rejects([&]{control.receive("host",committed);});
        rejects([&]{control.receive("guest2",proposal);});
        control.receive("host",proposal);
        if(sent.at("type")!="session-recovery-ack"||sent.at("accepted")!=proposal||queue.tick(clock)!=ReconnectPolicy::State::Paused)
            throw std::runtime_error("Guest ack prematurely resumed input");
        auto incomplete=committed;incomplete["acknowledged"]={"host","guest1"};rejects([&]{control.receive("host",incomplete);});
        control.receive("host",committed);
        if(queue.tick(clock)!=ReconnectPolicy::State::Running||queue.nextFrame()!=0)throw std::runtime_error("All-peer commit failed or advanced input");
    }
    {
        SessionId id{};id.fill(0xcc);uint64_t clock=100;
        RecoverablePadExchange guest(id,{1,2,3,4},2,{-1,0,-1,-1}),host(id,{1,2,3,4},1,{0,-1,-1,-1});
        PADStatus pads[4]{};
        if(!guest.bindOriginalSession(recovery,"guest1","host")||!host.bindOriginalSession(recovery,"host","host")||
           !guest.begin(pads,{456,4,5},clock)||!host.begin(pads,{456,4,5},clock)||
           !guest.disconnected(1,clock)||!host.disconnected(2,clock))throw std::runtime_error("Replacement suspended fixture failed");
        std::vector<nlohmann::ordered_json> toHost,toGuest;const std::string nonce(32,'f');
        NativeGuestReplacementVerification guestVerification(guest,recovery,"guest1","host",nonce,
            [&]{return guest.capture(1,0,{456,4,5},clock);},[]{return true;},[&]{return clock;},60100,
            [&](const auto& packet){toHost.push_back(packet);return true;});
        NativeGuestReplacementVerification hostVerification(host,recovery,"host","guest1",nonce,
            [&]{return host.capture(2,0,{456,4,5},clock);},[]{return true;},[&]{return clock;},60100,
            [&](const auto& packet){toGuest.push_back(packet);return true;});
        for(unsigned step=0;step<20&&(!guestVerification.verified()||!hostVerification.verified());++step){
            clock+=20;guestVerification.tick();hostVerification.tick();
            auto guestDelivery=std::move(toGuest);toGuest.clear();
            for(const auto& packet:guestDelivery)guestVerification.receive("host",packet);
            auto hostDelivery=std::move(toHost);toHost.clear();
            for(const auto& packet:hostDelivery)hostVerification.receive("guest1",packet);
        }
        if(!guestVerification.verified()||!hostVerification.verified()||guest.nextFrame()!=0||host.nextFrame()!=0||
           guest.tick(clock)!=ReconnectPolicy::State::Paused||host.tick(clock)!=ReconnectPolicy::State::Paused)
            throw std::runtime_error("Replacement verification failed or resumed input");
        NativeGuestRecoveryControl adopted(guest,recovery,"guest1","host",[&]{return clock;},
            [&](uint32_t owner){return guest.capture(owner,0,{456,4,5},clock);},[]{return true;},[](const auto&){return true;});
        adopted.verifiedLocalReplacement(nonce);
        if(adopted.status().at("owner")!=1||guest.tick(clock)!=ReconnectPolicy::State::Paused||guest.nextFrame()!=0)
            throw std::runtime_error("Verified adoption resumed original guest");
        adopted.receive("host",nativePausedStateReport(recovery,host.capture(2,0,{456,4,5},clock),"host",nonce));
        auto selfProposal=recoveryProposal(recovery,host.capture(2,0,{456,4,5},clock),"host",std::string(32,'e'));
        adopted.receive("host",selfProposal);
        auto selfCommit=selfProposal;selfCommit["type"]="session-recovery-commit";
        selfCommit["acknowledged"]={"host","guest1"};
        rejects([&]{adopted.receive("host",selfCommit);});
        if(guest.tick(clock)!=ReconnectPolicy::State::Paused)throw std::runtime_error("Partial self-recovery commit resumed guest");
        selfCommit["acknowledged"]={"host","guest1","guest2","guest3"};
        adopted.receive("host",selfCommit);
        if(guest.tick(clock)!=ReconnectPolicy::State::Running||guest.nextFrame()!=0)
            throw std::runtime_error("Verified self-recovery all-peer commit failed or advanced frame");
        clock=60100;rejects([&]{guestVerification.tick();});
        rejects([&]{adopted.verifiedLocalReplacement(nonce);});
    }
    PreparedStartLobby lobby{asset,500000,8,2,{{"host",0,true,true,true,500000},{"guest2",1,true,true,true,500000}}};
    PreparedStartRuntime runtime{asset,build,123,456,456,true};
    nlohmann::ordered_json configuration={{"type","session-configuration"},{"version",1},{"profile","unlocked-no-save-v1"},{"session",session},{"assetId",asset},{"buildId",build},{"seed",123}};
    auto proposal=SessionStartBarrier::fromConfiguration(lobby,configuration,runtime).proposal();
    auto commit=proposal;commit["type"]="session-start-commit";
    NativeGuestStartAgreement agreement("guest2",configuration);
    rejects([&]{agreement.acceptCommit(commit.dump(),lobby,runtime);});
    auto bad=proposal;bad["checkpoint"]=457;
    rejects([&]{agreement.acknowledge(bad.dump(),lobby,runtime);});
    NativeGuestStartAgreement foreign("guest3",configuration);
    rejects([&]{foreign.acknowledge(proposal.dump(),lobby,runtime);});
    auto ack=agreement.acknowledge(proposal.dump(),lobby,runtime);
    if(ack.at("type")!="session-start-ack"||agreement.committed())throw std::runtime_error("Premature commit");
    rejects([&]{agreement.acknowledge(proposal.dump(),lobby,runtime);});
    auto changed=lobby;++changed.revision;
    rejects([&]{agreement.acceptCommit(commit.dump(),changed,runtime);});
    auto running=runtime;running.idle=false;
    rejects([&]{agreement.acceptCommit(commit.dump(),lobby,running);});
    bad=commit;bad["seed"]=124;
    rejects([&]{agreement.acceptCommit(bad.dump(),lobby,runtime);});
    agreement.acceptCommit(commit.dump(),lobby,runtime);
    if(!agreement.committed())throw std::runtime_error("Commit unavailable");
    rejects([&]{agreement.acceptCommit(commit.dump(),lobby,runtime);});
    SessionId padSession{};padSession.fill(7);bool localSent=false;
    GuestInputBridge bridge(padSession,1,15,[&](std::span<const uint8_t> bytes){localSent=decode(bytes)->slot==1;return localSent;});
    Lockstep receiver(padSession,{1,2,3,4});
    const auto localPacket=encode({padSession,0,456,1,{}});
    if(!bridge.sendLocal(localPacket)||!localSent||receiver.receive(2,localPacket)!=Receive::Accepted)throw std::runtime_error("Four-player local input failed");
    for(uint8_t slot:{0,2,3})if(bridge.enqueueHost(encode({padSession,0,456,slot,{}}))!=InputMailbox::Push::Queued)throw std::runtime_error("Four-player remote input failed");
    if(!bridge.pump(receiver)||!receiver.advance(456))throw std::runtime_error("Four-player PAD agreement failed");
    auto wrongSession=padSession;wrongSession[0]=8;
    if(bridge.enqueueHost(encode({wrongSession,1,456,0,{}}))!=InputMailbox::Push::Malformed||bridge.enqueueHost(localPacket)!=InputMailbox::Push::Malformed)throw std::runtime_error("PAD session/slot spoof accepted");
    GuestInputBridge bounded(padSession,1,3,[](auto){return true;});
    if(bounded.enqueueHost(encode({padSession,0,456,3,{}}))!=InputMailbox::Push::Malformed)throw std::runtime_error("Inactive slot accepted");
    const auto remotePacket=encode({padSession,0,456,0,{}});
    for(size_t i=0;i<InputMailbox::Capacity;++i)if(bounded.enqueueHost(remotePacket)!=InputMailbox::Push::Queued)throw std::runtime_error("Mailbox capacity changed");
    if(bounded.enqueueHost(remotePacket)!=InputMailbox::Push::Overflow||bounded.pump(receiver))throw std::runtime_error("Input overflow did not halt");
    std::cout<<"Native guest start agreement rejection/commit tests PASS\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
