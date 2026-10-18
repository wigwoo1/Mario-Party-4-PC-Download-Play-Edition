#include "port/native_guest_invitation.hpp"
#include "port/native_host_invitation.hpp"
#include "port/native_host_guest.hpp"
#include "port/native_asset_receiver.hpp"
#include "port/native_guest_asset_transfer.hpp"
#include "port/native_guest_join_session.hpp"
#include "port/native_guest_staging_file.hpp"
#include "port/native_guest_start_control.hpp"
#include "port/native_guest_lobby_snapshot.hpp"
#include "port/native_lobby_state.hpp"
#include "port/guest_input_bridge.hpp"
#include "port/native_guest_replacement.hpp"
#include "port/native_host_replacement.hpp"
#include <sstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>
using namespace partyboard::net;
template<class Predicate>void waitFor(Predicate predicate,std::chrono::seconds budget=std::chrono::seconds(20)){
    const auto deadline=std::chrono::steady_clock::now()+budget;
    while(!predicate()){
        if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("RTC acceptance deadline exceeded");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
int main(int argc,char** argv){try{
    if(argc!=2&&argc!=3)throw std::invalid_argument("Test directory and optional private asset required");
    {
        // A native guest with a locally installed cache may decline the optional
        // browser seed channel without losing its six original transports.
        std::filesystem::create_directories(argv[1]);
        const auto filename=std::filesystem::path(argv[1])/"optional-seed-asset.bin";
        {std::ofstream file(filename,std::ios::binary);const std::string bytes(131072,'s');file.write(bytes.data(),bytes.size());}
        NativePipelineSeed seed{std::vector<unsigned char>(100,0),{}};
        std::memcpy(seed.bytes.data(),"SQLite format 3\0",16);
        constexpr char digits[]="0123456789abcdef";
        for(auto value:partyboard::crypto::sha256(seed.bytes)){seed.sha256+=digits[value>>4];seed.sha256+=digits[value&15];}
        NativeRtcSessionControl seedControls;
        NativeHostGuest seedHost("guest1",seedControls,std::make_shared<NativeSharedAsset>(filename,"fixture.bin"),
            rtc::Configuration{},std::make_shared<const NativePipelineSeed>(std::move(seed)));
        waitFor([&]{return seedHost.invitation().has_value();});
        NativeGuestInvitation nativeGuest(*seedHost.invitation());
        waitFor([&]{return nativeGuest.reply().has_value();});seedHost.acceptReply(*nativeGuest.reply());
        waitFor([&]{for(const auto* label:{"partyboard-preparation-v1","partyboard-start-v1","partyboard-lobby-v1",
                "partyboard-input-v1","partyboard-recovery-v1","partyboard-assets-v1"}){
            const auto channel=nativeGuest.channel(label);if(!channel||!channel->isOpen())return false;
        }return seedControls.connected("guest1");});
        if(seedHost.connectionRoute()!=NativeConnectionRoute::Direct||nativeGuest.connectionRoute()!=NativeConnectionRoute::Direct)
            throw std::runtime_error("Actual invitation pair did not report its selected direct route");
        for(unsigned iteration=0;iteration<100;++iteration){seedHost.pumpPipelineSeed();std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        if(!nativeGuest.error().empty()||!seedControls.connected("guest1"))
            throw std::runtime_error("Optional browser seed channel broke native guest control transport");
        nativeGuest.close();seedHost.close();
        if(seedHost.connectionRoute()!=NativeConnectionRoute::Closed||nativeGuest.connectionRoute()!=NativeConnectionRoute::Closed)
            throw std::runtime_error("Retired invitation route remained connected");
        std::cout<<"PASS native guest declines optional shader stream and preserves original channels\n";
    }
    for(bool duplicateSeed:{false,true}){
        NativeRtcSessionControl rejectionControls;
        NativeHostInvitation rejectedHost("guest3",rejectionControls);
        std::vector<std::shared_ptr<rtc::DataChannel>> rejectedChannels;
        rejectedChannels.push_back(rejectedHost.createChannel(duplicateSeed?"partyboard-pipelines-v1":"partyboard-unexpected-v1"));
        if(duplicateSeed)rejectedChannels.push_back(rejectedHost.createChannel("partyboard-pipelines-v1"));
        waitFor([&]{return rejectedHost.invitation().has_value();});
        NativeGuestInvitation rejectedGuest(*rejectedHost.invitation());
        waitFor([&]{return rejectedGuest.reply().has_value();});rejectedHost.acceptReply(*rejectedGuest.reply());
        waitFor([&]{return !rejectedGuest.error().empty();});
        if(rejectedGuest.error()!="Unexpected or duplicate Join channel")
            throw std::runtime_error("Optional seed exception weakened channel rejection");
        rejectedGuest.close();rejectedHost.close();
    }
    std::cout<<"PASS unknown and duplicate optional shader channels remain rejected\n";
    {
        const nlohmann::ordered_json credential={{"type","recovery-credential"},{"version",1},{"session",std::string(32,'1')},
            {"assetId",std::string(64,'a')},{"peer","guest2"},{"secret",std::string(64,'b')}};
        RecoveryIdentity hostIdentity(credential),guestIdentity(credential);
        const std::string nonce(32,'c');
        NativeHostReplacement replacementHost(60000,[&](const auto& description){return hostIdentity.sign(description,nonce);},
            [&](const auto& packet){return hostIdentity.verify(packet,"answer",nonce);});
        std::optional<nlohmann::ordered_json> offer;
        waitFor([&]{offer=replacementHost.offer(0);return offer.has_value();});
        auto forged=*offer;forged["signature"]=std::string(64,'0');
        bool forgedRejected=false;try{NativeGuestReplacement invalid(guestIdentity,forged,0,60000);}catch(const std::exception&){forgedRejected=true;}
        if(!forgedRejected)throw std::runtime_error("Forged guest replacement accepted");
        bool lateRejected=false;try{NativeGuestReplacement late(guestIdentity,*offer,60000,60000);}catch(const std::exception&){lateRejected=true;}
        if(!lateRejected)throw std::runtime_error("Expired offer opened a replacement");
        {
            RecoveryIdentity rollbackIdentity(credential);
            NativeGuestReplacement rollback(rollbackIdentity,*offer,10,60000);rollback.check(11);
            bool reversed=false;try{rollback.check(10);}catch(const std::exception&){reversed=true;}
            if(!reversed)throw std::runtime_error("Replacement clock reversal accepted");
        }
        NativeGuestReplacement replacementGuest(guestIdentity,*offer,0,60000);
        std::optional<nlohmann::ordered_json> answer;
        waitFor([&]{answer=replacementGuest.answer(0);return answer.has_value();});
        replacementHost.acceptAnswer(*answer,0);
        waitFor([&]{return replacementHost.ready(0)&&replacementGuest.ready(0);});
        replacementHost.recovery(0)->send(std::string("original signed replacement transport"));
        std::optional<NativeGuestInvitation::Message> received;
        waitFor([&]{received=replacementGuest.takeMessage(0);return received.has_value();});
        if(received->label!="partyboard-recovery-v1"||std::get<std::string>(received->payload)!="original signed replacement transport")
            throw std::runtime_error("Guest replacement recovery delivery differs");
        bool replayRejected=false;try{NativeGuestReplacement replay(guestIdentity,*offer,1,60000);}catch(const std::exception&){replayRejected=true;}
        if(!replayRejected)throw std::runtime_error("Original replacement offer replay accepted");
        bool expired=false;try{replacementGuest.ready(60000);}catch(const std::exception&){expired=true;}
        if(!expired)throw std::runtime_error("Guest replacement extended original deadline");
        replacementGuest.close();replacementHost.close();
    }
    NativeRtcSessionControl controls;
    NativeHostInvitation host("guest2",controls);
    const std::array<std::string,3> extra{"partyboard-input-v1","partyboard-recovery-v1","partyboard-assets-v1"};
    std::array<std::shared_ptr<rtc::DataChannel>,3> hostChannels;
    for(size_t i=0;i<extra.size();++i)hostChannels[i]=host.createChannel(extra[i]);
    waitFor([&]{return host.invitation().has_value();});
    auto staging=std::make_unique<NativeGuestStagingFile>(std::filesystem::path(argv[1])/"private-staging");
    NativeGuestJoinSession join(*host.invitation(),staging->stream());
    bool earlyPathRejected=false;try{staging->verifiedPath(join);}catch(const std::exception&){earlyPathRejected=true;}
    if(!earlyPathRejected)throw std::runtime_error("Join exposed unverified staging path");
    auto& guest=join.transport();
    if(guest.assignedPeer()!="guest2")throw std::runtime_error("Wrong assigned guest");
    waitFor([&]{return guest.reply().has_value();});
    host.acceptReply(*guest.reply());
    const std::array<std::string,6> labels{"partyboard-preparation-v1","partyboard-start-v1","partyboard-lobby-v1",extra[0],extra[1],extra[2]};
    waitFor([&]{for(const auto& label:labels){auto channel=guest.channel(label);if(!channel||!channel->isOpen())return false;}return controls.connected("guest2");});
    const auto connectedDiagnostics=guest.diagnosticState();
    if(connectedDiagnostics.at("ownerClosed")||connectedDiagnostics.at("channels").size()!=6)
        throw std::runtime_error("Connected Join diagnostics lost channel ownership");
    for(const auto& channel:connectedDiagnostics.at("channels"))if(!channel.at("open").get<bool>())
        throw std::runtime_error("Connected Join diagnostic channel is not open");
    std::array<bool,6> received{};
    controls.send("guest2",NativeRtcControlChannel::Purpose::Preparation,labels[0]);
    controls.send("guest2",NativeRtcControlChannel::Purpose::Start,labels[1]);
    controls.send("guest2",NativeRtcControlChannel::Purpose::Lobby,labels[2]);
    for(size_t i=0;i<extra.size();++i)hostChannels[i]->send(extra[i]);
    // Deliberately defer game-thread consumption until after all sends.
    waitFor([&]{
        while(auto message=guest.takeMessage()){
            const auto* text=std::get_if<std::string>(&message->payload);
            if(!text||*text!=message->label)throw std::runtime_error("Join mailbox lost protocol ownership");
            for(size_t i=0;i<labels.size();++i)if(message->label==labels[i])received[i]=true;
        }
        for(auto value:received)if(!value)return false;return true;
    });
    bool rejected=false;try{host.acceptReply(*guest.reply());}catch(const std::exception&){rejected=true;}
    if(!rejected)throw std::runtime_error("Duplicate initial answer accepted");
    const std::filesystem::path directory(argv[1]);std::filesystem::create_directories(directory);
    const auto fixture=argc==3?std::filesystem::path(argv[2]):directory/"join-asset.bin";
    if(argc==2){std::ofstream file(fixture,std::ios::binary);for(unsigned i=0;i<500000;++i)file.put(char((i*29)&255));if(!file)throw std::runtime_error("Fixture write failed");}
    NativeAsset asset(fixture,"game.ciso");NativeAssetHost sender(asset);
    struct Requests {std::mutex mutex;std::deque<std::string> messages;};
    const auto requests=std::make_shared<Requests>();
    hostChannels[2]->onMessage([requests](rtc::message_variant message){
        std::lock_guard lock(requests->mutex);
        if(const auto* text=std::get_if<std::string>(&message))requests->messages.push_back(*text);
    });
    const auto configuration=nativeSessionConfiguration(std::string(32,'1'),12345,asset.manifest().at("id"),std::string(64,'2'));
    controls.send("guest2",NativeRtcControlChannel::Purpose::Preparation,configuration.dump());
    const nlohmann::ordered_json credential{{"type","recovery-credential"},{"version",1},{"session",configuration.at("session")},{"assetId",asset.manifest().at("id")},{"peer","guest2"},{"secret",std::string(64,'4')}};
    controls.send("guest2",NativeRtcControlChannel::Purpose::Preparation,credential.dump());
    hostChannels[2]->send(asset.manifest().dump());
    waitFor([&]{
        join.pump();
        std::deque<std::string> pending;{std::lock_guard lock(requests->mutex);pending.swap(requests->messages);}
        for(const auto& request:pending)for(const auto& packet:sender.control(request))
            hostChannels[2]->send(reinterpret_cast<const rtc::byte*>(packet.data()),packet.size());
        if(!join.error().empty())throw std::runtime_error(join.error());
        return sender.complete()&&join.assetsComplete();
    },std::chrono::seconds(180));
    const auto verifiedPath=staging->sealVerified(join);
    bool sealedWriterRejected=false;try{staging->stream();}catch(const std::exception&){sealedWriterRejected=true;}
    if(!sealedWriterRejected)throw std::runtime_error("Verified staging writer reopened");
    const auto expectedSize=asset.manifest().at("size").get<uint64_t>();
    if(!join.assetsComplete()||join.receivedBytes()!=expectedSize||std::filesystem::file_size(verifiedPath)!=expectedSize)throw std::runtime_error("Incomplete native RTC asset");
    {NativeAsset staged(verifiedPath,"game.ciso");if(staged.manifest().at("id")!=asset.manifest().at("id"))throw std::runtime_error("Staged native RTC asset identity differs");}
    bool retained=false,credentialRetained=false;
    waitFor([&]{join.pump();while(auto message=join.takeControl()){
        if(message->label==labels[0]){
            const auto text=std::get<std::string>(message->payload);
            if(nlohmann::ordered_json::parse(text).at("type")=="recovery-credential"){
                auto wrong=*message;auto wrongPacket=credential;wrongPacket["peer"]="guest1";wrong.payload=wrongPacket.dump();
                bool rejectedPeer=false;try{join.retainOriginalRecoveryCredential(wrong);}catch(const std::exception&){rejectedPeer=true;}
                if(!rejectedPeer)throw std::runtime_error("Join accepted another guest credential");
                join.retainOriginalRecoveryCredential(*message);
                bool duplicate=false;try{join.retainOriginalRecoveryCredential(*message);}catch(const std::exception&){duplicate=true;}
                if(!duplicate)throw std::runtime_error("Join replaced original recovery credential");
                RecoveryIdentity hostIdentity(credential);
                const auto signedOffer=hostIdentity.sign({{"type","offer"},{"sdp","test recovery SDP"}},std::string(32,'5'));
                if(!join.recoveryIdentity().verify(signedOffer,"offer")||join.recoveryIdentity().verify(signedOffer,"offer"))throw std::runtime_error("Original Join signature/replay check failed");
                credentialRetained=true;continue;
            }
            bool wrongBuildRejected=false;try{join.retainConfiguration(text,std::string(64,'3'));}catch(const std::exception&){wrongBuildRejected=true;}
            if(!wrongBuildRejected)throw std::runtime_error("Join accepted wrong measured build");
            join.retainConfiguration(text,std::string(64,'2'));
            if(join.configuration()!=configuration)throw std::runtime_error("Join configuration changed");
            bool duplicateRejected=false;try{join.retainConfiguration(text,std::string(64,'2'));}catch(const std::exception&){duplicateRejected=true;}
            if(!duplicateRejected)throw std::runtime_error("Join replaced original configuration");retained=true;
        }
    }return retained&&credentialRetained;});
    // Real assigned RTC Start delivery using protocol-fixture engine/lobby state.
    NativeLobbyState hostLobby(asset.manifest().at("id"),expectedSize);
    hostLobby.join("host","Host");hostLobby.join("guest2","Guest");
    for(const auto* peer:{"host","guest2"})hostLobby.report(peer,{{"assetId",asset.manifest().at("id")},{"sequence",1},{"bytes",expectedSize},{"verified",true},{"shaders",true},{"ready",true},{"membership",2},{"saveHash",nullptr}});
    const nlohmann::ordered_json snapshotPacket{{"type","lobby-snapshot"},{"version",1},{"snapshot",hostLobby.snapshot()}};
    NativeGuestLobbySnapshot guestLobby(asset.manifest().at("id"),expectedSize,"guest2");
    controls.send("guest2",NativeRtcControlChannel::Purpose::Lobby,snapshotPacket.dump());
    bool snapshotReceived=false;
    waitFor([&]{join.pump();while(auto message=join.takeControl()){
        if(message->label!="partyboard-lobby-v1")throw std::runtime_error("Wrong lobby channel");
        guestLobby.receive(std::get<std::string>(message->payload));snapshotReceived=true;
    }return snapshotReceived;});
    guestLobby.receive(snapshotPacket.dump()); // Exact retransmission is harmless.
    auto invalidSnapshot=snapshotPacket;invalidSnapshot["snapshot"]["revision"]=0;
    bool staleRejected=false;try{guestLobby.receive(invalidSnapshot.dump());}catch(const std::exception&){staleRejected=true;}
    if(!staleRejected)throw std::runtime_error("Guest accepted stale snapshot");
    const auto readyLobby=guestLobby.current();
    PreparedStartRuntime prepared{asset.manifest().at("id"),std::string(64,'2'),12345,456,456,true};
    SessionStartBarrier barrier=SessionStartBarrier::fromConfiguration(readyLobby,configuration,prepared);
    NativeGuestStartControl guestStart(join);
    if(!controls.send("guest2",NativeRtcControlChannel::Purpose::Start,barrier.proposal().dump()))throw std::runtime_error("Proposal send failed");
    bool acknowledged=false;
    waitFor([&]{
        join.pump();while(auto message=join.takeControl())guestStart.receive(*message,readyLobby,prepared);
        while(auto message=controls.take(NativeRtcControlChannel::Purpose::Start)){
            if(message->route.peer!="guest2"||!barrier.acknowledge(message->route.peer,nlohmann::ordered_json::parse(message->text),readyLobby))throw std::runtime_error("Assigned guest ack rejected");
            acknowledged=true;
        }
        return acknowledged;
    });
    if(guestStart.committed())throw std::runtime_error("Guest committed before host agreement");
    auto hostAck=barrier.proposal();hostAck["type"]="session-start-ack";
    if(!barrier.acknowledge("host",hostAck,readyLobby))throw std::runtime_error("Host acknowledgment rejected");
    if(!controls.send("guest2",NativeRtcControlChannel::Purpose::Start,barrier.prepareCommit(readyLobby,prepared).dump()))throw std::runtime_error("Commit send failed");
    waitFor([&]{join.pump();while(auto message=join.takeControl())guestStart.receive(*message,readyLobby,prepared);return guestStart.committed();});
    SessionId inputSession{};inputSession.fill(0x11);
    const auto hostSawLocal=std::make_shared<std::atomic<bool>>(false);
    const auto localInput=encode({inputSession,0,456,1,{}});
    hostChannels[0]->onMessage([hostSawLocal,localInput](rtc::message_variant message){
        if(const auto* bytes=std::get_if<rtc::binary>(&message)){
            const auto decoded=decode(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes->data()),bytes->size()));
            if(decoded&&encode(*decoded)==localInput)*hostSawLocal=true;
        }
    });
    GuestInputBridge guestInput(inputSession,1,3,[&guest](std::span<const uint8_t> bytes){
        const auto channel=guest.channel("partyboard-input-v1");if(!channel||!channel->isOpen())return false;
        channel->send(reinterpret_cast<const rtc::byte*>(bytes.data()),bytes.size());return true;
    });
    Lockstep inputReceiver(inputSession,{1,2,0,0});
    if(inputReceiver.receive(2,localInput)!=Receive::Accepted||!guestInput.sendLocal(localInput))throw std::runtime_error("Guest local PAD send rejected");
    const auto hostInput=encode({inputSession,0,456,0,{}});
    hostChannels[0]->send(reinterpret_cast<const rtc::byte*>(hostInput.data()),hostInput.size());
    bool hostInputReceived=false;
    waitFor([&]{join.pump();
        // The prepared lobby's selective drains must leave early PAD intact.
        if(join.takeControl("partyboard-lobby-v1")||join.takeControl("partyboard-start-v1"))
            throw std::runtime_error("Unexpected lobby/Start packet during PAD handoff");
        while(auto message=join.takeControl()){
        if(message->label!="partyboard-input-v1")throw std::runtime_error("PAD arrived on wrong channel");
        const auto* bytes=std::get_if<rtc::binary>(&message->payload);
        if(!bytes||guestInput.enqueueHost(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes->data()),bytes->size()))!=InputMailbox::Push::Queued)throw std::runtime_error("Host PAD relay rejected");
        hostInputReceived=true;
    }return hostInputReceived&&hostSawLocal->load();});
    if(!guestInput.pump(inputReceiver)||!inputReceiver.advance(456))throw std::runtime_error("Guest synchronized PAD frame unavailable");
    if(guestInput.enqueueHost(localInput)!=InputMailbox::Push::Malformed||guestInput.sendLocal(hostInput))throw std::runtime_error("Guest PAD slot spoof accepted");
    guestInput.closeStopped();hostChannels[0]->resetCallbacks();
    if(guestInput.sendLocal(localInput)||guestInput.enqueueHost(hostInput)!=InputMailbox::Push::Retired)throw std::runtime_error("Stopped guest PAD route remained active");
    hostChannels[0]->close();
    waitFor([&]{const auto diagnostic=guest.diagnosticState();for(const auto& event:diagnostic.at("history").at("events"))
        if(event.at("type")=="channel-closed"&&event.at("detail")=="partyboard-input-v1")return true;return false;});
    if(!guest.error().empty())throw std::runtime_error("Diagnostic channel closure changed Join failure policy");
    // Detach the host callback before its stack-owned mailbox is destroyed.
    hostChannels[2]->resetCallbacks();
    // A stalled game thread must not allow an unbounded RTC message queue.
    for(unsigned i=0;i<257;++i)hostChannels[2]->send(std::string("queued asset control"));
    waitFor([&]{return !guest.error().empty();});
    if(guest.error()!="Join message bounds exceeded"||guest.takeMessage()||guest.reply())
        throw std::runtime_error("Failed Join mailbox published retained data");
    join.pump();if(join.error().empty()||join.assetsComplete()||guestStart.committed())throw std::runtime_error("Failed Join session retained readiness/commit");
    join.close();join.close();
    const auto closedDiagnostics=guest.diagnosticState();unsigned ownerCloses=0;bool overflowRecorded=false;
    for(const auto& event:closedDiagnostics.at("history").at("events")){
        if(event.at("type")=="owner-close")++ownerCloses;
        if(event.at("type")=="mailbox-overflow")overflowRecorded=true;
    }
    if(!closedDiagnostics.at("ownerClosed").get<bool>()||ownerCloses!=1||!overflowRecorded)
        throw std::runtime_error("Join close or overflow diagnostic evidence missing");
    staging.reset();if(std::filesystem::exists(verifiedPath)||std::filesystem::exists(verifiedPath.parent_path()))throw std::runtime_error("Owned staging files survived cleanup");
    if(guest.reply()||guest.takeMessage())throw std::runtime_error("Closed guest published data");
    for(auto& channel:hostChannels)channel->close();controls.retireAuthenticated("guest2");host.close();
    std::cout<<"PASS actual native Join: assigned reply, six channels, "<<expectedSize<<"-byte verified RTC asset, staged disk identity, assigned Start ack/commit, duplicate answer/queue overflow rejection and close\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
