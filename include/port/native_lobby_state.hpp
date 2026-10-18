#pragma once
#include <nlohmann/json.hpp>
#include <map>
#include <algorithm>
#include <stdexcept>

namespace partyboard::net {
// Host-owned unlocked/no-save lobby wire state. Peer identity is supplied by
// its bound transport, never taken from a guest's status payload.
class NativeLobbyState {
    using Json=nlohmann::ordered_json;
    std::string asset_;
    uint64_t total_,revision_=0,membership_=0;
    std::map<std::string,Json> players_;
    static uint64_t integer(const Json& value){
        if(!value.is_number_integer()||value.get<int64_t>()<0||value.get<uint64_t>()>9007199254740991ULL)
            throw std::invalid_argument("Safe lobby integer required");
        return value.get<uint64_t>();
    }
    void invalidate(){for(auto& [peer,player]:players_)player["ready"]=false;}
public:
    NativeLobbyState(std::string asset,uint64_t total):asset_(std::move(asset)),total_(total){
        if(asset_.size()!=64||!std::all_of(asset_.begin(),asset_.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');})||!total||total>2147483648ULL)
            throw std::invalid_argument("Bounded native lobby asset required");
    }
    unsigned join(const std::string& peer,const std::string& name){
        if(peer.empty()||peer.size()>128||name.empty()||name.size()>40||name.find_first_not_of(" \t\r\n")==std::string::npos||players_.contains(peer)||players_.size()==4)
            throw std::invalid_argument("Native lobby slot unavailable");
        unsigned slot=0;
        for(;slot<4;++slot)if(std::none_of(players_.begin(),players_.end(),[&](const auto& p){return p.second.at("slot")==slot;}))break;
        invalidate();
        players_[peer]={{"peer",peer},{"name",name},{"slot",slot},{"sequence",0},{"bytes",0},{"verified",false},{"shaders",false},{"ready",false},{"saveHash",nullptr}};
        ++membership_;++revision_;return slot;
    }
    void leave(const std::string& peer){if(players_.erase(peer)){invalidate();++membership_;++revision_;}}
    void report(const std::string& peer,const Json& report){
        const auto found=players_.find(peer);
        if(found==players_.end()||report.at("assetId")!=asset_)throw std::invalid_argument("Assigned native lobby peer required");
        const auto sequence=integer(report.at("sequence")),bytes=integer(report.at("bytes"));
        for(const auto key:{"verified","shaders","ready"})if(!report.at(key).is_boolean())throw std::invalid_argument("Boolean lobby status required");
        const bool verified=report.at("verified"),shaders=report.at("shaders"),ready=report.at("ready");
        if(sequence<=integer(found->second.at("sequence"))||bytes<integer(found->second.at("bytes"))||bytes>total_||(verified&&bytes!=total_)||(shaders&&!verified)||(ready&&!shaders))
            throw std::invalid_argument("Invalid native lobby progress");
        const auto membership=ready?integer(report.at("membership")):membership_;
        if(ready&&membership>membership_)throw std::invalid_argument("Future lobby membership");
        if(report.contains("saveHash")&&!report.at("saveHash").is_null())throw std::invalid_argument("No game saves in native lobby");
        auto& player=found->second;
        player["sequence"]=sequence;player["bytes"]=bytes;player["verified"]=verified;player["shaders"]=shaders;player["ready"]=ready&&membership==membership_;++revision_;
    }
    Json snapshot()const{
        std::vector<Json> players;bool canStart=players_.size()>=2;
        for(const auto& [peer,original]:players_){
            auto player=original;const auto bytes=integer(player.at("bytes"));
            player["progress"]=double(bytes)/double(total_);
            const bool ready=player.at("ready"),verified=player.at("verified"),shaders=player.at("shaders");
            player["phase"]=ready?"Ready":shaders?"Waiting for ready":verified?"Awaiting shader preparation":bytes==total_?"Verifying assets":"Downloading assets";
            canStart=canStart&&ready&&verified&&shaders;players.push_back(std::move(player));
        }
        std::sort(players.begin(),players.end(),[](const auto& a,const auto& b){return a.at("slot")<b.at("slot");});
        return {{"revision",revision_},{"membership",membership_},{"assetId",asset_},{"total",total_},{"players",players},{"saveRequired",false},{"saveHash",nullptr},{"canStart",canStart}};
    }
};
}
