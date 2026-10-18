#include "port/native_rtc_engine_probe.hpp"
#include "port/native_rtc_host_input.hpp"
#include "port/native_engine_input_io.hpp"
#include "port/native_input_poll.hpp"
#include "port/recovery_credentials.hpp"
#include <filesystem>
#include "port/main.h"
#include <atomic>
#include <iostream>
#include <thread>

namespace partyboard::net {
namespace {
using Json = nlohmann::ordered_json;
std::mutex diagnosticOutput;
void report(const Json& event) {
    std::lock_guard lock(diagnosticOutput);
    std::cout << "Native RTC diagnostic: " << event.dump() << std::endl;
}
}
int runNativeRtcEngineProbe(unsigned frames,std::function<bool()> pumpEvents,bool diagnosticButtons) {
    if(!pumpEvents)throw std::invalid_argument("Native event pump required");
    if (!frames || frames > 1800 || PartyBoard_GameLifecyclePhase()!=2 || PartyBoard_GameFrameCount()!=0)
        throw std::runtime_error("Fresh prepared native engine and bounded probe required");
    SessionId session{}; session.fill(0x11);
    auto owner = createNativeRtcHostInput(session,0,15);
    std::array<std::shared_ptr<rtc::PeerConnection>,3> connections;
    std::array<std::shared_ptr<RecoveryIdentity>,3> identities;
    std::array<std::string,3> nonces;
    auto opens=std::make_shared<std::atomic<unsigned>>(0);
    // Cleanup is independent of observer duration and always preserves callback
    // lifetime until the synchronous game run returns and input is detached.
    struct TransportCleanup {
        NativeRtcHostInput& owner;
        std::array<std::shared_ptr<rtc::PeerConnection>,3>& connections;
        ~TransportCleanup() {
            owner.closeStopped();
            for (auto& pc:connections) if(pc) {pc->resetCallbacks();pc->close();}
        }
    } cleanup{*owner,connections};
    for (uint8_t index=0;index<3;++index) {
        const uint8_t slot=index+1;
        auto credential=createRecoveryCredential(std::string(32,'1'),std::string(64,'a'),"guest"+std::to_string(slot));
        auto identity=identities[index]=std::make_shared<RecoveryIdentity>(credential);
        auto nonce=nonces[index]=createRecoveryNonce();
        report({{"type","original-credential"},{"slot",slot},{"credential",credential}});
        auto pc=connections[index]=std::make_shared<rtc::PeerConnection>(rtc::Configuration{});
        pc->onGatheringStateChange([pc,identity,nonce,slot](rtc::PeerConnection::GatheringState state) {
            if(state==rtc::PeerConnection::GatheringState::Complete) {
                const auto description=pc->localDescription();
                if(description)report({{"type","offer"},{"slot",slot},{"packet",identity->sign(Json{{"type","offer"},{"sdp",std::string(*description)}},nonce)}});
            }
        });
        auto channel=pc->createDataChannel("partyboard-input-v1");
        owner->attachAuthenticated(slot,channel);
        channel->onOpen([opens,slot]{++*opens;report({{"type","open"},{"slot",slot}});});
    }
    std::array<bool,3> answered{};
    unsigned answers=0;
    std::string line;
    while(answers<3 && std::getline(std::cin,line)) {
        if(line.size()>100000)throw std::runtime_error("Oversized diagnostic signaling");
        const auto command=Json::parse(line);
        const auto slot=command.at("slot").get<unsigned>();
        if(command.at("type")!="answer" || slot<1 || slot>3 || answered[slot-1])
            throw std::runtime_error("Assigned original answer required");
        if(!identities[slot-1]->verify(command.at("packet"),"answer",nonces[slot-1]))
            throw std::runtime_error("Diagnostic answer authentication failed");
        connections[slot-1]->setRemoteDescription(rtc::Description(command.at("packet").at("description").at("sdp").get<std::string>(),"answer"));
        answered[slot-1]=true; ++answers;
    }
    if(answers!=3)throw std::runtime_error("Original diagnostic signaling ended early");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(45);
    while(opens->load()!=3) {
        if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Diagnostic channel connection failed");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    unsigned seed=0;
    if(!PartyBoardFixedSeed(&seed))throw std::runtime_error("Prepared engine seed unavailable");
    RecoverablePadExchange exchange(session,{1,2,3,4},1,{0,-1,-1,-1});
    RecoverySession identity{std::string(32,'1'),std::string(64,'b'),std::string(64,'a'),"unlocked-no-save-v1",seed,0,0,
        {{"host",0},{"guest1",1},{"guest2",2},{"guest3",3}}};
    if(!exchange.bindOriginalSession(identity,"host","host"))throw std::runtime_error("Diagnostic session binding rejected");
    NativeEngineInputIO io([&]{return pumpEvents() && owner->pump(exchange,uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()));},
        [&](const Packet& packet){return owner->send(packet);},[]{std::this_thread::sleep_for(std::chrono::milliseconds(1));});
    NativeInputPollBinding<NativeEngineInputIO> binding(exchange,io);
    struct Probe { NativeInputPollBinding<NativeEngineInputIO>& binding; unsigned limit,accepted=0; bool buttons=false; } probe{binding,frames,0,diagnosticButtons};
    const auto sample=+[](void* context,PADStatus pads[4],const PartyBoardInputState* state)->int {
        try {
        auto& probe=*static_cast<Probe*>(context);
        report({{"type","state"},{"frame",state->frame},{"checkpoint",state->checkpoint},{"frand",state->frand},{"rand8",state->rand8}});
        if(state->frame!=probe.accepted || probe.accepted==probe.limit)return 0;
        if(probe.buttons){pads[0]={};pads[0].err=PAD_ERR_NONE;
            if(state->frame>=60 && state->frame%60<2)pads[0].button=PAD_BUTTON_A|PAD_BUTTON_START;}
        if(!NativeInputPollBinding<NativeEngineInputIO>::sample(&probe.binding,pads,state))return 0;
        std::array<Pad,4> packed{};for(unsigned slot=0;slot<4;++slot)packed[slot]=packPad(pads[slot]);
        ++probe.accepted;
        report({{"type","committed"},{"frame",state->frame},{"pads",packed}});
        return 1;
        } catch (...) { return 0; } // Never unwind through the engine C callback.
    };
    if(!PartyBoardNativeInstallInputExchange(sample,&probe))throw std::runtime_error("Native RTC probe hook installation rejected");
    const int execution=PartyBoard_GameResume();
    PartyBoard_GameRequestStop();
    if(!PartyBoardNativeClearOwnedInputExchange(sample,&probe))
        throw std::runtime_error("Stopped native RTC probe detach failed");
    const auto stopped=io.capture();
    report({{"type","stopped"},{"accepted",probe.accepted},{"frame",stopped.frame},{"checkpoint",stopped.checkpoint},
        {"frand",stopped.frand},{"rand8",stopped.rand8},{"phase",PartyBoard_GameLifecyclePhase()},{"detached",true}});
    if(execution!=0 || probe.accepted!=frames || PartyBoard_GameFrameCount()!=frames)
        throw std::runtime_error("Native RTC engine probe incomplete");
    report({{"type","complete"},{"frames",frames},{"detached",true},{"phase",PartyBoard_GameLifecyclePhase()},
        {"scope","Actual native engine frames and three RTC input guests; no browser game or production start agreement"}});
    // Keep channels alive until the observer confirms the final relayed frame;
    // the engine is idle and its input callback is already detached.
    if(!std::getline(std::cin,line) || line.size()>100000 || Json::parse(line).at("type")!="stop")
        throw std::runtime_error("Diagnostic cleanup acknowledgment required");
    return 0;
}
}
