#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
namespace partyboard::net {
struct PreparedStartPlayer {std::string peer;uint8_t slot;bool ready,verified,shaders;uint64_t bytes;};
struct PreparedStartLobby {std::string assetId;uint64_t total,revision,membership;std::vector<PreparedStartPlayer> players;};
struct PreparedStartRuntime {std::string assetId,buildId;uint32_t seed,checkpoint,preparedCheckpoint;bool idle;};
// Browser start-barrier.mjs agreement. Bound peer authentication, actual
// preparation/state capture and all-player commit transport belong to coordinator.
class SessionStartBarrier {
 using Json=nlohmann::ordered_json;
public:
 enum class State {Awaiting,Agreed,Mismatch,Cancelled};
 // Bind the start proposal to the retained host configuration before sending
 // any proposal. A fresh measured package alone is not prepared-engine proof.
 static SessionStartBarrier fromConfiguration(const PreparedStartLobby& lobby,
        const Json& configuration,const PreparedStartRuntime& runtime){
  const auto& seed=configuration.at("seed");
  if(configuration.at("type")!="session-configuration"||configuration.at("version")!=1||
     configuration.at("profile")!="unlocked-no-save-v1"||!seed.is_number_integer()||
     seed.get<int64_t>()<0||seed.get<uint64_t>()>UINT32_MAX||!runtime.idle||
     runtime.assetId!=lobby.assetId||configuration.at("assetId")!=runtime.assetId||
     configuration.at("buildId")!=runtime.buildId||seed!=runtime.seed||
     runtime.preparedCheckpoint!=runtime.checkpoint)
   throw std::invalid_argument("Prepared runtime differs from retained configuration");
  return SessionStartBarrier(lobby,configuration.at("session").get<std::string>(),
      runtime.seed,runtime.buildId,runtime.checkpoint);
 }
 SessionStartBarrier(const PreparedStartLobby& lobby,const std::string& session,uint32_t seed,const std::string& buildId,uint32_t checkpoint){
  auto players=lineup(lobby);if(!hex(session,32)||session==std::string(32,'0')||!hex(buildId,64))throw std::invalid_argument("Invalid start identity");
  proposal_={{"type","session-start"},{"version",1},{"session",session},{"revision",lobby.revision},{"membership",lobby.membership},{"assetId",lobby.assetId},{"total",lobby.total},{"buildId",buildId},{"seed",seed},{"checkpoint",checkpoint},{"profile","unlocked-no-save-v1"},{"players",players}};
 }
 Json proposal()const{return proposal_;}State state()const{return state_;}
 bool update(const PreparedStartLobby& lobby){if(state_==State::Cancelled||state_==State::Mismatch)return false;try{if(lobby.revision!=proposal_["revision"].get<uint64_t>()||lobby.membership!=proposal_["membership"].get<uint64_t>()||lobby.assetId!=proposal_["assetId"].get<std::string>()||lobby.total!=proposal_["total"].get<uint64_t>()||lineup(lobby)!=proposal_["players"])throw std::runtime_error("Lobby changed");}catch(...){cancel();return false;}return true;}
 bool acknowledge(const std::string& boundPeer,const Json& message,const PreparedStartLobby& lobby){
  if(!update(lobby))return false;bool assigned=false;for(const auto&p:proposal_["players"])if(p["peer"]==boundPeer)assigned=true;if(!assigned)return false;
  try{if(message.at("type")!="session-start-ack"||message.at("version")!=1)return false;
   for(const auto*key:{"session","revision","membership","assetId","total","buildId","seed","profile","players"})if(message.at(key)!=proposal_.at(key))return false;
   if(!message.at("checkpoint").is_number_integer()||message.at("checkpoint")!=proposal_["checkpoint"]){state_=State::Mismatch;return false;}
  }catch(...){return false;}
  acks_.insert(boundPeer);if(acks_.size()==proposal_["players"].size())state_=State::Agreed;return true;
 }
 Json prepareCommit(const PreparedStartLobby& lobby,const PreparedStartRuntime& runtime){
  if(!update(lobby)||state_!=State::Agreed)throw std::runtime_error("Every player must acknowledge before commit");
  if(!runtime.idle||runtime.assetId!=proposal_["assetId"].get<std::string>()||runtime.buildId!=proposal_["buildId"].get<std::string>()||runtime.seed!=proposal_["seed"].get<uint32_t>()||runtime.checkpoint!=proposal_["checkpoint"].get<uint32_t>()||runtime.preparedCheckpoint!=runtime.checkpoint)
   throw std::runtime_error("Prepared host runtime differs from agreed start");
  auto commit=proposal_;commit["type"]="session-start-commit";return commit;
 }
 void cancel(){state_=State::Cancelled;acks_.clear();}
private:
 static bool hex(const std::string&s,size_t n){return s.size()==n&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
 static Json lineup(const PreparedStartLobby& lobby){if(!hex(lobby.assetId,64)||lobby.total<1||lobby.total>2147483648ULL||lobby.revision>9007199254740991ULL||lobby.membership>lobby.revision||lobby.players.size()<2||lobby.players.size()>4)throw std::invalid_argument("Lobby preparation required");
  auto players=lobby.players;std::sort(players.begin(),players.end(),[](const auto&a,const auto&b){return a.slot<b.slot;});std::set<std::string> peers;std::set<uint8_t> slots;Json result=Json::array();
  for(const auto&p:players){if(p.peer.empty()||p.peer.size()>128||!peers.insert(p.peer).second||p.slot>3||!slots.insert(p.slot).second||!p.ready||!p.verified||!p.shaders||p.bytes!=lobby.total)throw std::invalid_argument("All players must prepare");result.push_back({{"peer",p.peer},{"slot",p.slot}});}return result;
 }
 Json proposal_;std::set<std::string> acks_;State state_=State::Awaiting;
};
}

