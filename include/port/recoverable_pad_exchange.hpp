#pragma once
#include "pad_exchange.hpp"
#include "reconnect_policy.hpp"
#include "paused_state.hpp"
#include "input_trace.h"
#include "native_recovery_poll_replay.hpp"

namespace partyboard::net {
// Game-thread-only retained poll. The session coordinator owns authentication,
// paused-state reconciliation and the all-player commit. Reopening a channel
// alone must never invoke recovered()/resume(). RTC callbacks enqueue elsewhere.
class RecoverablePadExchange {
public:
    std::array<int8_t,4> localPadSlots() const { return exchange_.localPadSlots(); }
    const char* firstStopSite() const { return exchange_.firstStopSite(); }
    Stop firstStopState() const { return exchange_.firstStopState(); }
    struct State { uint32_t checkpoint,frand,rand8; bool operator==(const State&) const=default; };
    struct CompletedPoll {
        uint64_t frame;
        State state;
        std::array<uint8_t,44> pads{};
        std::array<Packet,4> localPackets{};
        size_t localPacketCount=0;
    };
    RecoverablePadExchange(SessionId session,std::array<uint32_t,4> owners,uint32_t local,
                          std::array<int8_t,4> lanes)
        :exchange_(session,owners,local,lanes),policy_(owners),owners_(owners),session_(session),local_(local) {}
    // Install once from the accepted original start, before sampling any input.
    bool bindOriginalSession(const RecoverySession& identity,const std::string& localPeer,const std::string& host) {
        if(originalIdentity_||pending_||exchange_.nextFrame()!=0||policy_.state()!=ReconnectPolicy::State::Running)return false;
        bool localAssigned=false,hostAssigned=false;
        for(const auto& player:identity.players){
            if(player.slot>=4)return false;
            localAssigned|=player.peer==localPeer&&owners_[player.slot]==local_;
            hostAssigned|=player.peer==host;
        }
        if(!localAssigned||!hostAssigned||!recoveryHex(identity.session,32)||!recoveryHex(identity.buildId,64)||
           !recoveryHex(identity.assetId,64)||identity.profile!="unlocked-no-save-v1"||
           identity.revision>9007199254740991ULL||identity.membership>identity.revision||
           identity.players.size()<2||identity.players.size()>4)return false;
        std::set<std::string> peers;
        for(const auto& player:identity.players)
            if(player.peer.empty()||player.peer.size()>128||!peers.insert(player.peer).second)return false;
        if(!matchesSession(identity))return false;
        originalIdentity_=identity;originalLocalPeer_=localPeer;originalHost_=host;return true;
    }
    bool begin(const PADStatus pads[4],State state,uint64_t now) {
        const auto phase=tick(now);
        if(pending_||phase==ReconnectPolicy::State::ReturnToMenu)return false;
        if(phase==ReconnectPolicy::State::Paused&&!replayedTarget_)return false;
        if(replayedTarget_){
            const auto& target=*replayedTarget_;
            if(state.checkpoint!=target.at("checkpoint")||state.frand!=target.at("frand")||state.rand8!=target.at("rand8")){
                cancel();return false;
            }
            replayedTarget_.reset();
        }
        if(!exchange_.begin(pads,state.checkpoint))return false;
        frozen_=state;pending_=true;pendingStarted_=now;return true;
    }
    std::span<const Packet> retainedPackets() const { return exchange_.outgoing(); }
    bool originalSessionBound() const {return originalIdentity_.has_value();}
    bool originalSessionMatches(const RecoverySession& identity) const {return originalIdentity_&&*originalIdentity_==identity;}
    Receive receive(uint32_t owner,std::span<const uint8_t> packet) {
        const auto result=exchange_.receive(owner,packet);
        if(result==Receive::Conflict||result==Receive::Divergence||result==Receive::Halted)cancel();
        return result;
    }
    Exchange poll(State current,PADStatus pads[4],uint64_t now) {
        if(tick(now)==ReconnectPolicy::State::ReturnToMenu)return Exchange::Stopped;
        if(!pending_||current!=frozen_) {cancel();return Exchange::Stopped;}
        if(policy_.state()==ReconnectPolicy::State::Paused)return Exchange::Waiting;
        CompletedPoll completed{exchange_.nextFrame(),frozen_};
        for(const auto& packet:exchange_.outgoing())completed.localPackets[completed.localPacketCount++]=packet;
        const auto result=exchange_.poll(current.checkpoint,pads);
        if(result==Exchange::Ready){
            pending_=false;
            for(size_t slot=0;slot<4;++slot){const auto packed=packPad(pads[slot]);
                for(size_t byte=0;byte<packed.size();++byte)completed.pads[slot*11+byte]=packed[byte];}
            completed_=completed;
        }
        if(result==Exchange::Stopped)cancel();
        return result;
    }
    bool disconnected(uint32_t owner,uint64_t now) {
        return policy_.disconnected(owner,now,pending_?pendingStarted_:now);
    }
    void preparePausedReplay(const std::string& remotePeer,const std::string& nonce,
        const nlohmann::ordered_json& report,const nlohmann::ordered_json& receipt,
        uint32_t missingOwner,uint32_t frame,State current,uint64_t now){
        if(!originalIdentity_||replay_||replayedTarget_||(consumedReplayEpoch_&&*consumedReplayEpoch_==epoch()))throw std::runtime_error("Original unreplayed suspended poll required");
        const auto captured=capture(missingOwner,frame,current,now);
        replay_=validateNativePollReplay(*originalIdentity_,captured,originalLocalPeer_,remotePeer,nonce,report,receipt,owners_,exchange_.outgoing());
        replayEpoch_=epoch();replayRevision_=revision();
    }
    Exchange consumePausedReplay(State current,PADStatus pads[4],uint64_t now){
        if(!replay_)return Exchange::Waiting;
        if(tick(now)!=ReconnectPolicy::State::Paused||!pending_||current!=frozen_||epoch()!=replayEpoch_||revision()!=replayRevision_){cancel();return Exchange::Stopped;}
        for(const auto& [owner,packet]:replay_->packets)if(owner!=local_){
            const auto received=exchange_.receive(owner,packet);
            if(received!=Receive::Accepted&&received!=Receive::Duplicate){cancel();return Exchange::Stopped;}
        }
        CompletedPoll completed{exchange_.nextFrame(),frozen_};
        for(const auto& packet:exchange_.outgoing())completed.localPackets[completed.localPacketCount++]=packet;
        const auto result=exchange_.poll(current.checkpoint,pads);
        if(result!=Exchange::Ready){cancel();return Exchange::Stopped;}
        for(size_t slot=0;slot<4;++slot){const auto packed=packPad(pads[slot]);for(size_t byte=0;byte<11;++byte)completed.pads[slot*11+byte]=packed[byte];}
        completed_=completed;pending_=false;replayedTarget_=replay_->target;consumedReplayEpoch_=epoch();replay_.reset();return Exchange::Ready;
    }
    bool recovered(uint32_t owner,uint64_t epoch,uint64_t revision,uint64_t now) {
        return policy_.markRecovered(owner,epoch,revision,now);
    }
    bool resume(uint64_t epoch,uint64_t revision,uint64_t now) {return policy_.resume(epoch,revision,now);}
    // Called only by the authenticated original-host control coordinator after
    // retaining its own acknowledgment and confirming every assigned transport.
    bool resumeCommitted(const RecoverySession& identity,const std::string& localPeer,
            const std::string& boundHost,const nlohmann::ordered_json& acknowledged,
            const nlohmann::ordered_json& commit,uint64_t epoch,uint64_t revision,
            uint32_t gameFrame,State current,bool transportsReady,uint64_t now) {
        if(tick(now)!=ReconnectPolicy::State::Paused||!transportsReady||
           epoch!=policy_.epoch()||revision!=policy_.revision())return false;
        if(!originalIdentity_||identity!=*originalIdentity_||localPeer!=originalLocalPeer_||boundHost!=originalHost_)return false;
        if(!matchesSession(identity))return false;
        uint32_t missingOwner=0;
        for(unsigned slot=0;slot<4;++slot)if(policy_.missingSlots()[slot]){missingOwner=owners_[slot];break;}
        if(!missingOwner)return false;
        try {
            const auto captured=capture(missingOwner,gameFrame,current,now);
            if(!acceptRecoveryCommit(commit,acknowledged,identity,captured,localPeer,boundHost))return false;
            const auto missing=policy_.missingSlots();
            for(unsigned slot=0;slot<4;++slot)if(missing[slot])
                if(!policy_.markRecovered(owners_[slot],epoch,revision,now)){cancel();return false;}
            return policy_.resume(epoch,revision,now);
        }catch(const std::exception&){return false;}
    }
private:
    bool matchesSession(const RecoverySession& identity) const {
        std::string sessionHex;sessionHex.reserve(32);
        constexpr char digits[]="0123456789abcdef";
        for(auto byte:session_){sessionHex.push_back(digits[byte>>4]);sessionHex.push_back(digits[byte&15]);}
        if(identity.session!=sessionHex)return false;
        std::array<bool,4> assigned{};
        for(const auto& player:identity.players){
            if(player.slot>=4||assigned[player.slot])return false;
            assigned[player.slot]=true;
        }
        for(unsigned slot=0;slot<4;++slot)if(assigned[slot]!=(owners_[slot]!=0))return false;
        return true;
    }
public:
    ReconnectPolicy::State tick(uint64_t now) {
        const auto state=policy_.update(now);
        if(state==ReconnectPolicy::State::ReturnToMenu){exchange_.cancel();pending_=false;}
        return state;
    }
    uint64_t epoch() const {return policy_.epoch();}
    uint64_t revision() const {return policy_.revision();}
    uint64_t nextFrame() const {return exchange_.nextFrame();}
    bool pending()const{return pending_;}
    const std::array<bool,4>& missingSlots()const{return policy_.missingSlots();}
    const std::optional<CompletedPoll>& completedPoll() const {return completed_;}
    uint64_t remainingRecoveryMs(uint64_t now){return policy_.remainingMs(now);}
    PausedCapture captureGame(uint32_t missingOwner,uint64_t now) {
        PartyBoardInputState current{};
        if(!PartyBoardCaptureInputState(&current))throw std::runtime_error("Actual game input state unavailable");
        return capture(missingOwner,current.frame,{current.checkpoint,current.frand,current.rand8},now);
    }
    PausedCapture capture(uint32_t missingOwner,uint32_t gameFrame,State current,uint64_t now) {
        if(tick(now)!=ReconnectPolicy::State::Paused||!pending_)
            throw std::runtime_error("Current suspended native input required");
        if(current!=frozen_){cancel();throw std::runtime_error("Paused native state drifted");}
        bool missing=false;
        for(unsigned slot=0;slot<4;++slot)
            missing|=missingOwner!=0&&owners_[slot]==missingOwner&&policy_.missingSlots()[slot];
        if(!missing)throw std::runtime_error("Assigned missing native owner required");
        return {missingOwner,policy_.epoch(),policy_.revision(),policy_.epoch(),policy_.revision(),
            exchange_.nextFrame(),exchange_.nextFrame(),gameFrame,current.checkpoint,current.frand,current.rand8,
            frozen_.checkpoint,frozen_.frand,frozen_.rand8,true};
    }
    void cancel() {policy_.cancel();exchange_.cancel();pending_=false;replay_.reset();replayedTarget_.reset();}
private:
    PadExchange exchange_;
    ReconnectPolicy policy_;
    std::array<uint32_t,4> owners_;
    SessionId session_;
    uint32_t local_;
    std::optional<RecoverySession> originalIdentity_;
    std::optional<CompletedPoll> completed_;
    std::optional<NativePollReplay> replay_;
    std::optional<nlohmann::ordered_json> replayedTarget_;
    std::optional<uint64_t> consumedReplayEpoch_;
    uint64_t replayEpoch_=0,replayRevision_=0;
    uint64_t pendingStarted_=0;
    std::string originalLocalPeer_,originalHost_;
    State frozen_{};
    bool pending_{};
};
}
