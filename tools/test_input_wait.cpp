#include "../include/port/input_wait.hpp"
#include "../include/port/recoverable_pad_exchange.hpp"
#include <cassert>
#include <cstdio>
using namespace partyboard::net;
struct FakeIO : WaitIO {
    SessionId session{1};
    uint64_t now{};
    unsigned polls{}, sends{}, deliverAt{3};
    Pump action{Pump::Continue};
    bool failSend{}, throwPump{}, changeState{}, reverseClock{};
    unsigned sendUntil{};
    PADStatus* observed{};
    bool send(std::span<const Packet> packets) override {
        ++sends;
        assert(packets.size() == 1);
        return !failSend;
    }
    bool sendPending() const override { return polls < sendUntil; }
    Pump pump(PadExchange& exchange) override {
        ++polls;
        assert(exchange.nextFrame() == 0 && observed[0].button == 123);
        if (throwPump) throw std::runtime_error("injected transport failure");
        if (polls == deliverAt) {
            Input remote{session, 0, 42, 1, {}}; remote.pad[0] = 7;
            assert(exchange.receive(22, encode(remote)) == Receive::Accepted);
        }
        return action;
    }
    uint32_t checkpoint() override { return (changeState && polls) ? 43 : 42; }
    uint64_t milliseconds() override { return (reverseClock && polls) ? now - 1 : now; }
    void idle() override { ++now; }
};
int main() {
    for(unsigned scenario=0;scenario<3;++scenario){
        SessionId session{1};PADStatus pads[4]{};pads[0].button=55;
        RecoverablePadExchange retained(session,{1,2,0,0},1,{0,-1,-1,-1});
        RecoverySession identity{"01"+std::string(30,'0'),std::string(64,'b'),std::string(64,'a'),"unlocked-no-save-v1",0,1,1,{{"host",0},{"guest",1}}};
        assert(retained.bindOriginalSession(identity,"host","host"));
        assert(!retained.bindOriginalSession(identity,"host","host"));
        RecoverablePadExchange::State state{42,3,5};
        assert(retained.begin(pads,state,100));const auto original=retained.retainedPackets()[0];
        assert(retained.disconnected(2,101));
        const auto captured=retained.capture(2,10,state,101);
        assert(captured.inputFrame==0&&captured.epoch==retained.epoch()&&captured.revision==retained.revision());
        const auto proposal=recoveryProposal(identity,captured,"host",std::string(32,'c'));
        assert(proposal["inputFrame"]=="0"&&proposal["frame"]==10&&proposal["frand"]==3);
        bool wrongOwner=false;
        try{retained.capture(1,10,state,101);}catch(const std::runtime_error&){wrongOwner=true;}
        assert(wrongOwner);
        assert(retained.receive(2,encode({session,0,42,1,{7}}))==Receive::Accepted);
        assert(retained.poll(state,pads,102)==Exchange::Waiting);
        assert(pads[0].button==55&&pads[1].button==0&&retained.nextFrame()==0);
        assert(retained.retainedPackets()[0]==original);
        if(scenario==0){
            assert(!retained.resume(retained.epoch(),retained.revision(),103));
            auto commit=proposal;commit["type"]="session-recovery-commit";
            commit["acknowledged"]=nlohmann::ordered_json::array({"host","guest"});
            auto altered=commit;altered["rand8"]=6;
            auto wrongIdentity=identity;wrongIdentity.session=std::string(32,'2');
            assert(!retained.resumeCommitted(wrongIdentity,"host","host",proposal,commit,retained.epoch(),retained.revision(),10,state,true,103));
            wrongIdentity=identity;wrongIdentity.assetId=std::string(64,'d');
            const auto changedAssetProposal=recoveryProposal(wrongIdentity,captured,"host",std::string(32,'c'));
            auto changedAssetCommit=changedAssetProposal;changedAssetCommit["type"]="session-recovery-commit";
            changedAssetCommit["acknowledged"]=nlohmann::ordered_json::array({"host","guest"});
            assert(!retained.resumeCommitted(wrongIdentity,"host","host",changedAssetProposal,changedAssetCommit,retained.epoch(),retained.revision(),10,state,true,103));
            wrongIdentity=identity;wrongIdentity.players[1].slot=2;
            const auto wrongProposal=recoveryProposal(wrongIdentity,captured,"host",std::string(32,'c'));
            auto wrongCommit=wrongProposal;wrongCommit["type"]="session-recovery-commit";
            wrongCommit["acknowledged"]=nlohmann::ordered_json::array({"host","guest"});
            assert(!retained.resumeCommitted(wrongIdentity,"host","host",wrongProposal,wrongCommit,retained.epoch(),retained.revision(),10,state,true,103));
            assert(!retained.resumeCommitted(identity,"host","host",proposal,altered,retained.epoch(),retained.revision(),10,state,true,103));
            assert(!retained.resumeCommitted(identity,"host","host",proposal,commit,retained.epoch(),retained.revision(),10,state,false,104));
            assert(!retained.resumeCommitted(identity,"host","host",proposal,commit,retained.epoch(),retained.revision()+1,10,state,true,104));
            assert(retained.resumeCommitted(identity,"host","host",proposal,commit,retained.epoch(),retained.revision(),10,state,true,105));
            assert(!retained.resumeCommitted(identity,"host","host",proposal,commit,retained.epoch(),retained.revision(),10,state,true,105));
            assert(retained.poll(state,pads,106)==Exchange::Ready&&pads[1].button==7&&retained.nextFrame()==1);
        }else{
            if(scenario==1)state.frand++;
            assert(retained.poll(state,pads,scenario==1?103:60101)==Exchange::Stopped);
            assert(pads[0].button==55&&pads[1].button==0&&retained.nextFrame()==0);
            assert(retained.retainedPackets().empty());
        }
    }
    {
        SessionId session{1};PADStatus pads[4]{};pads[0].button=55;pads[2].button=99;
        PadExchange guest(session,{1,2,3,4},3,{-1,-1,0,-1});
        assert(guest.begin(pads,42));assert(guest.outgoing().size()==1);
        const auto packet=decode(guest.outgoing()[0]);
        assert(packet&&packet->slot==2&&packet->pad[0]==55);
        pads[0].button=77;assert(decode(guest.outgoing()[0])->pad[0]==55); // Frozen once.
        bool rejected=false;
        try {PadExchange bad(session,{1,3,3,4},3,{-1,0,0,-1});}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected);
        rejected=false;
        try {PadExchange bad(session,{1,2,3,4},3,{0,-1,1,-1});}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected);
    }
    for (unsigned scenario = 0; scenario < 12; ++scenario) {
        PADStatus pads[4]{}; pads[0].button = 123;
        FakeIO io; io.observed = pads; io.now = 100;
        PadExchange exchange(io.session, {11, 22, 0, 0}, 11);
        WaitResult expected = WaitResult::Ready;
        switch (scenario) {
        case 1: io.deliverAt = 99; expected = WaitResult::Timeout; break;
        case 2: io.action = Pump::Cancel; expected = WaitResult::Cancelled; break;
        case 3: io.action = Pump::Disconnected; expected = WaitResult::Disconnected; break;
        case 4: io.failSend = true; expected = WaitResult::TransportError; break;
        case 5: io.throwPump = true; expected = WaitResult::TransportError; break;
        case 6: io.changeState = true; expected = WaitResult::StateMismatch; break;
        case 7: io.reverseClock = true; expected = WaitResult::ClockError; break;
        case 8: expected = WaitResult::InvalidTimeout; break;
        case 9: io.deliverAt = 1; io.sendUntil = 4; break;
        case 10: io.deliverAt = 1; io.sendUntil = 99; expected = WaitResult::Timeout; break;
        case 11: io.changeState = true; io.sendUntil = 99; expected = WaitResult::StateMismatch; break;
        }
        const auto result = waitForInput(exchange, pads, io, scenario == 8 ? 0 : 5);
        assert(result == expected);
        if (scenario == 11) assert(io.polls == 1); // Reject drift before the send drains.
        assert(io.sends == (scenario == 8 ? 0u : 1u));
        if (scenario == 0 || scenario == 9) {
            assert(io.polls == (scenario == 9 ? 4u : 3u) && pads[1].button == 7 && exchange.nextFrame() == 1);
        } else {
            assert(pads[0].button == 123 && pads[1].button == 0 && exchange.nextFrame() == 0);
            assert(!exchange.begin(pads, 42)); // Terminal failures cannot resume stale inputs.
        }
    }
    puts("Input wait tests passed: delayed delivery, timeout, cancel, disconnect, transport/state/clock errors");
}
