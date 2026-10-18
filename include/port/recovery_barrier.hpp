#pragma once
#include <nlohmann/json.hpp>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>

namespace partyboard::net {
// capture() returns a proposal built from the original accepted session and
// actual paused game/input state. Never construct it from a received proposal.
// current() checks the locally retained attempt; authenticated channel identity
// is supplied separately to acknowledge(). This barrier does not resume input.
class RecoveryBarrier {
public:
    using Json=nlohmann::ordered_json;
    using Capture=std::function<Json()>;
    RecoveryBarrier(Capture capture,std::function<bool()> current,
                    std::function<uint64_t()> now,uint64_t deadline)
        :capture_(std::move(capture)),current_(std::move(current)),now_(std::move(now)),deadline_(deadline) {
        if(!capture_||!current_||!now_)throw std::invalid_argument("Live recovery capture required");
        proposal_=capture_();
        if(!proposal_.is_object()||proposal_.value("type","")!="session-recovery-proposal"||
           proposal_.value("version",0)!=1||!proposal_.contains("players")||
           !proposal_["players"].is_array()||proposal_["players"].size()<2||proposal_["players"].size()>4)
            throw std::invalid_argument("Validated paused-state proposal required");
        std::set<unsigned> slots;
        for(const auto& player:proposal_["players"]) {
            const auto peer=player.at("peer").get<std::string>();
            const auto slot=player.at("slot").get<unsigned>();
            if(peer.empty()||peer.size()>128||slot>=4||!players_.insert(peer).second||!slots.insert(slot).second)
                throw std::invalid_argument("Distinct assigned recovery players required");
        }
        const auto host=proposal_.at("host").get<std::string>();
        if(!players_.contains(host))throw std::invalid_argument("Assigned host required");
        validate();acks_.insert(host);
    }
    Json proposal() const { return proposal_; }
    bool acknowledge(const std::string& boundPeer,const Json& message) {
        validate();
        if(!players_.contains(boundPeer)||!message.is_object()||message.size()!=4||
           message.value("type","")!="session-recovery-ack"||message.value("version",0)!=1||
           message.value("peer","")!=boundPeer||!message.contains("accepted")||message["accepted"]!=proposal_)
            throw std::runtime_error("Bound assigned recovery acknowledgment required");
        acks_.insert(boundPeer);return acks_.size()==players_.size();
    }
    Json commit() {
        validate();
        if(acks_.size()!=players_.size())throw std::runtime_error("Every assigned player must acknowledge recovery");
        auto result=proposal_;result["type"]="session-recovery-commit";
        result["acknowledged"]=Json::array();
        for(const auto& player:proposal_["players"])result["acknowledged"].push_back(player["peer"]);
        finished_=true;return result;
    }
    void cancel() { finished_=true; }
private:
    void validate() {
        const auto time=now_();
        if(finished_||time>=deadline_||(hasLast_&&time<last_)||!current_()||capture_()!=proposal_)
            throw std::runtime_error("Recovery round expired or paused state changed");
        last_=time;hasLast_=true;
    }
    Capture capture_;
    std::function<bool()> current_;
    std::function<uint64_t()> now_;
    uint64_t deadline_,last_{};
    bool finished_{},hasLast_{};
    Json proposal_;
    std::set<std::string> players_,acks_;
};
}
