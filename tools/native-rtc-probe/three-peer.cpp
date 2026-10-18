#include <rtc/rtc.hpp>
#include "port/host_input_router.hpp"
#include "port/recovery_credentials.hpp"
#include "port/host_input_bridge.hpp"
#include "port/recoverable_pad_exchange.hpp"
#include "port/native_input_poll.hpp"
#include "port/native_rtc_host_input.hpp"
#include <iostream>
#include <mutex>
#include <thread>
#include <deque>
#include "port/session_start_barrier.hpp"
using Json = nlohmann::ordered_json;
static std::mutex output;
static void report(const Json& value) { std::lock_guard lock(output); std::cout << value.dump() << std::endl; }
// Original credential delivery below is an explicit trusted test bootstrap,
// not a production lobby protocol, asset transfer, or game start agreement.
int main(int argc,char**argv) {
    try {
        using namespace partyboard::net;
        const bool startMode=argc==2&&std::string(argv[1])=="--start";
        const bool ownerMode=argc==2&&std::string(argv[1])=="--rtc-owner";
        const bool pollMode=ownerMode||(argc==2&&std::string(argv[1])=="--poll");
        auto startLobby=std::make_shared<PreparedStartLobby>(PreparedStartLobby{std::string(64,'a'),50000,5,3,{{"host",0,true,true,true,50000},{"guest1",1,true,true,true,50000},{"guest2",2,true,true,true,50000},{"guest3",3,true,true,true,50000}}});
        auto startBarrier=std::make_shared<SessionStartBarrier>(*startLobby,std::string(32,'1'),7,std::string(64,'b'),123);
        auto startMessages=std::make_shared<std::deque<std::pair<uint8_t,std::string>>>();
        SessionId session{}; session.fill(0x11);
        auto rtcOwner=std::make_shared<NativeRtcHostInput>(session,0,15);
        auto queue=std::make_shared<RecoverablePadExchange>(session,std::array<uint32_t,4>{1,2,3,4},1,
            std::array<int8_t,4>{0,-1,-1,-1});
        auto pads=std::make_shared<std::array<PADStatus,4>>();
        auto router=std::make_shared<HostInputRouter>(session,0,15);
        auto mutex=std::make_shared<std::mutex>();
        auto channels=std::make_shared<std::array<std::shared_ptr<rtc::DataChannel>,3>>();
        auto opens=std::make_shared<unsigned>(0);
        auto pending=std::make_shared<bool>(false);
        auto closedSlots=std::make_shared<uint8_t>(0);
        auto bridge=std::make_shared<HostInputBridge>(session,0,15,[channels](uint8_t slot,std::span<const uint8_t> bytes){
            auto channel=(*channels)[slot-1];
            if(!channel || !channel->isOpen())return false;
            channel->send(reinterpret_cast<const rtc::byte*>(bytes.data()),bytes.size());return true;
        });
        std::array<std::shared_ptr<rtc::PeerConnection>,3> connections;
        std::array<std::shared_ptr<RecoveryIdentity>,3> identities;
        std::array<std::string,3> nonces;
        Pad hostPad{}; for(unsigned i=0;i<11;++i)hostPad[i]=uint8_t(i*23);
        (*pads)[0]=unpackPad(hostPad);(*pads)[0].extButton=12345;
        if(pollMode){RecoverySession accepted{std::string(32,'1'),std::string(64,'b'),std::string(64,'a'),"unlocked-no-save-v1",0,0,0,{{"host",0},{"guest1",1},{"guest2",2},{"guest3",3}}};if(!queue->bindOriginalSession(accepted,"host","host"))throw std::runtime_error("Original session rejected");}
        else if(!queue->begin(pads->data(),{0xdeadbeef,3,5},0) || queue->retainedPackets().size()!=1)throw std::runtime_error("Host PAD input rejected");
        const Packet hostPacket=pollMode?Packet{}:queue->retainedPackets()[0];
        for(unsigned index=0;index<3;++index) {
            const auto slot=uint8_t(index+1);
            const auto binding=bridge->attach(slot);
            auto credential=createRecoveryCredential(std::string(32,'1'),std::string(64,'a'),"guest"+std::to_string(slot));
            identities[index]=std::make_shared<RecoveryIdentity>(credential);nonces[index]=createRecoveryNonce();
            report({{"type","original-credential"},{"slot",slot},{"credential",credential}});
            auto pc=connections[index]=std::make_shared<rtc::PeerConnection>(rtc::Configuration{});
            auto identity=identities[index];auto nonce=nonces[index];
            pc->onGatheringStateChange([pc,identity,nonce,slot](rtc::PeerConnection::GatheringState state){
                if(state==rtc::PeerConnection::GatheringState::Complete) {
                    const auto description=pc->localDescription();
                    if(description)report({{"type","offer"},{"slot",slot},{"packet",identity->sign(Json{{"type","offer"},{"sdp",std::string(*description)}},nonce)}});
                }
            });
            auto channel=(*channels)[index]=pc->createDataChannel(startMode?"partyboard-start-v1":"partyboard-input-v1");
            channel->onOpen([opens,mutex,slot]{
                std::lock_guard lock(*mutex);report({{"type","open"},{"slot",slot}});
                ++*opens;
            });
            if(ownerMode)rtcOwner->attachAuthenticated(slot,channel,[slot](InputMailbox::Push result){
                report({{"type",result==InputMailbox::Push::Queued?"routed":"rejected"},{"slot",slot},{"recipients",uint8_t(15u&~(1u<<slot)&~1u)}});
            });
            else { channel->onClosed([closedSlots,mutex,slot]{
                std::lock_guard lock(*mutex);*closedSlots|=uint8_t(1u<<slot);
            });
            channel->onMessage([bridge,binding,mutex,router,pending,slot,startMode,startMessages](rtc::message_variant message){
                std::lock_guard lock(*mutex);
                if(startMode){auto text=std::get_if<std::string>(&message);if(!text||text->size()>65536||startMessages->size()>=8){report({{"type","error"},{"message","Bounded start acknowledgment required"}});return;}startMessages->emplace_back(slot,*text);return;}
                auto bytes=std::get_if<rtc::binary>(&message);
                if(!bytes){report({{"type","rejected"},{"slot",slot}});return;}
                auto packet=std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes->data()),bytes->size());
                const auto route=router->remote(slot,packet);
                if(!route){report({{"type","rejected"},{"slot",slot}});return;}
                if(bridge->enqueue(binding,packet)!=InputMailbox::Push::Queued) {
                    report({{"type","error"},{"message","Input bridge rejected packet or delivery"}});return;
                }
                report({{"type","routed"},{"slot",slot},{"recipients",route->recipients}});
                *pending=true;
            });
            }
        }
        std::jthread simulation([=](std::stop_token stop){
            if(pollMode){
                struct IO {
                    std::stop_token stop;std::mutex& mutex;unsigned& opens;bool& pending;uint8_t& closedSlots;HostInputBridge& bridge;RecoverablePadExchange& queue;std::array<PADStatus,4>& pads;NativeRtcHostInput* owner;unsigned received=0;
                    uint64_t now(){return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
                    PartyBoardInputState capture(){return {0,0xdeadbeef,3,5};}
                    bool pump(){if(owner){if(stop.stop_requested())return false;std::array<Pad,4> before{};for(unsigned i=0;i<4;i++)before[i]=packPad(pads[i]);const bool ok=owner->pump(queue,now(),[this](uint8_t slot){report({{"type","paused-disconnect"},{"slot",slot},{"frame",std::to_string(queue.nextFrame())}});});for(unsigned i=0;i<4;i++)if(before[i]!=packPad(pads[i]))return false;return ok;}std::lock_guard lock(mutex);if(stop.stop_requested())return false;const auto closed=closedSlots;closedSlots=0;for(uint8_t slot=1;slot<4;slot++)if(closed&(1u<<slot)){if(!queue.disconnected(slot+1,now()))return false;report({{"type","paused-disconnect"},{"slot",slot},{"frame",std::to_string(queue.nextFrame())}});}if(pending){pending=false;std::array<Pad,4> before{};for(unsigned i=0;i<4;i++)before[i]=packPad(pads[i]);if(!bridge.pump(queue))return false;for(unsigned i=0;i<4;i++)if(before[i]!=packPad(pads[i]))return false;if(++received<3)report({{"type","waiting-pad"},{"frame","0"}});}return true;}
                    bool send(const Packet& packet){std::lock_guard lock(mutex);return opens==3&&(owner?owner->send(packet):bridge.sendLocal(packet));}
                    void wait(){std::this_thread::sleep_for(std::chrono::milliseconds(1));}
                } io{stop,*mutex,*opens,*pending,*closedSlots,*bridge,*queue,*pads,ownerMode?rtcOwner.get():nullptr};
                NativeInputPollBinding<IO> binding(*queue,io);auto state=io.capture();
                if(!NativeInputPollBinding<IO>::sample(&binding,pads->data(),&state)){if(!stop.stop_requested())report({{"type","error"},{"message","Native poll callback stopped"}});return;}
                std::array<Pad,4> packed{};for(unsigned i=0;i<4;i++){packed[i]=packPad((*pads)[i]);if((*pads)[i].extButton!=0){report({{"type","error"},{"message","Native UI buttons entered network input"}});return;}}
                report({{"type","committed"},{"frame","0"},{"pads",packed},{"nextFrame",std::to_string(queue->nextFrame())}});return;
            }
            bool hostSent=false;
            while(!stop.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                std::lock_guard lock(*mutex);
                if(startMode){
                    if(!hostSent&&*opens==3){hostSent=true;auto ack=startBarrier->proposal();ack["type"]="session-start-ack";if(!startBarrier->acknowledge("host",ack,*startLobby)){report({{"type","error"},{"message","Local start rejected"}});return;}for(auto&c:*channels)c->send(startBarrier->proposal().dump());}
                    while(!startMessages->empty()){auto [slot,message]=std::move(startMessages->front());startMessages->pop_front();try{if(!startBarrier->acknowledge("guest"+std::to_string(slot),Json::parse(message),*startLobby))throw std::runtime_error("Bound start acknowledgment rejected");report({{"type","start-ack"},{"slot",slot},{"agreed",startBarrier->state()==SessionStartBarrier::State::Agreed}});}catch(const std::exception&e){report({{"type","error"},{"message",e.what()}});return;}}
                    if(startBarrier->state()==SessionStartBarrier::State::Agreed){auto commit=startBarrier->prepareCommit(*startLobby,{std::string(64,'a'),std::string(64,'b'),7,123,123,true});for(auto&c:*channels)c->send(commit.dump());report({{"type","start-committed"},{"commit",commit},{"inputFrame",std::to_string(queue->nextFrame())}});return;}
                    continue;
                }
                if(!hostSent&&*opens==3){
                    hostSent=true;
                    if(!bridge->sendLocal(hostPacket)){report({{"type","error"},{"message","Host broadcast failed"}});return;}
                }
                if(!*pending)continue;
                *pending=false;
                if(!bridge->pump(*queue)){report({{"type","error"},{"message","Game-thread input pump failed"}});return;}
                std::array<Pad,4> before{};for(unsigned i=0;i<4;++i)before[i]=packPad((*pads)[i]);
                const auto frame=queue->nextFrame();const auto result=queue->poll({0xdeadbeef,3,5},pads->data(),0);
                if(result==Exchange::Ready) {
                    std::array<Pad,4> packed{};for(unsigned i=0;i<4;++i) {
                        packed[i]=packPad((*pads)[i]);
                        if((*pads)[i].extButton!=0){report({{"type","error"},{"message","Native UI buttons entered network input"}});return;}
                    }
                    report({{"type","committed"},{"frame",std::to_string(frame)},{"pads",packed},{"nextFrame",std::to_string(queue->nextFrame())}});
                } else if(result==Exchange::Waiting) {
                    for(unsigned i=0;i<4;++i)if(before[i]!=packPad((*pads)[i])){report({{"type","error"},{"message","Waiting mutated PADStatus"}});return;}
                    report({{"type","waiting-pad"},{"frame",std::to_string(queue->nextFrame())}});
                } else report({{"type","error"},{"message","PAD exchange stopped"}});
            }
        });
        std::string line;
        while(std::getline(std::cin,line)) {
            if(line.size()>100000)throw std::runtime_error("Oversized signaling");
            auto command=Json::parse(line);if(command.at("type")=="stop")break;
            const auto slot=command.at("slot").get<unsigned>();
            if(command.at("type")!="answer"||slot<1||slot>3)throw std::runtime_error("Assigned answer required");
            bool valid=false;try{valid=identities[slot-1]->verify(command.at("packet"),"answer",nonces[slot-1]);}catch(const std::exception&){}
            if(!valid){report({{"type","rejected-answer"},{"slot",slot}});continue;}
            connections[slot-1]->setRemoteDescription(rtc::Description(command.at("packet").at("description").at("sdp").get<std::string>(),"answer"));
        }
        simulation.request_stop();simulation.join();
        if(ownerMode)rtcOwner->closeStopped();
        for(auto& channel:*channels){channel->resetCallbacks();channel->close();channel.reset();}
        for(auto& pc:connections){pc->resetCallbacks();pc->close();pc.reset();}
        rtc::Cleanup().wait();return 0;
    }catch(const std::exception& error){report({{"type","error"},{"message",error.what()}});return 1;}
}


