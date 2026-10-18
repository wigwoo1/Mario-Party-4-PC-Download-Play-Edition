#include <rtc/rtc.hpp>
#include "port/asset_transfer.hpp"
#include "port/native_rtc_asset_host.hpp"
#include "port/native_host_guest.hpp"
#include "port/native_host_lobby_control.hpp"
#include "port/native_asset_preparation.hpp"
#include "port/native_host_lobby_session.hpp"
#include "port/recovery_credentials.hpp"
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
using Json=nlohmann::ordered_json;
static std::mutex output;
static void report(const Json& j){std::lock_guard lock(output);std::cout<<j.dump()<<std::endl;}
// Acceptance executable: trusted original credential bootstrap over stdin/stdout.
// No production lobby authentication, game start or native game integration.
int main(int argc,char**argv){try{
 if(argc==3&&std::string(argv[1])=="--session-three-asset-lobby"){
  {
   partyboard::net::NativeHostLobbySession session(argv[2]);
   if(session.verifyOriginalReplacementOffer("guest1",nlohmann::ordered_json::object()))
    throw std::runtime_error("Uncommitted replacement offer accepted");
   bool unsignedAnswerRefused=false;
   try{session.signOriginalReplacementAnswer("guest1",{{"type","answer"},{"sdp","fixture"}});}
   catch(const std::exception&){unsignedAnswerRefused=true;}
   if(!unsignedAnswerRefused)throw std::runtime_error("Replacement answer signed without accepted offer");
   while(!session.prepared()){
    if(!session.tick())throw std::runtime_error(session.error());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
   }
   // Protocol fixture identity only; this mode does not prepare/start engines.
   session.retainConfiguration(partyboard::net::nativeSessionConfiguration(std::string(32,'1'),123,session.manifest().at("id"),std::string(64,'2')),std::string(64,'2'));
   for(unsigned slot=1;slot<=3;++slot){
    const auto peer=session.addGuest("Guest "+std::to_string(slot));
    std::optional<std::string> offer;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    while(!(offer=session.invitation(peer))){
     if(!session.tick())throw std::runtime_error(session.error());
     if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Session invitation gathering timeout");
     std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    report({{"type","initial-invitation"},{"peer",peer},{"code",*offer},{"manifest",session.manifest()}});
    std::string reply;if(!std::getline(std::cin,reply)||reply.size()>100000)throw std::runtime_error("Session reply required");
    session.acceptReply(peer,reply);
   }
   bool announced=false;
   for(;;){
    if(!session.tick())throw std::runtime_error(session.error());
    bool connected=true,complete=true;Json sent=Json::array();
    for(unsigned slot=1;slot<=3;++slot){const auto peer="guest"+std::to_string(slot);
     connected=connected&&session.connected(peer);complete=complete&&session.assetsComplete(peer);sent.push_back(session.assetSentBytes(peer));}
    if(connected&&!announced){report({{"type","open"}});announced=true;}
    const auto snapshot=session.snapshot();
    for(const auto& player:snapshot.at("players"))complete=complete&&player.at("verified").get<bool>();
    if(complete){report({{"type","complete"},{"guests",3},{"sentBytes",sent},{"lobbySnapshot",snapshot}});break;}
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
   }
   std::string stop;if(!std::getline(std::cin,stop)||stop!="stop")throw std::runtime_error("Explicit session stop required");
   session.close();if(session.tick()||session.prepared())throw std::runtime_error("Session retained work after close");
  }
  rtc::Cleanup().wait();return 0;
 }
 if(argc==3&&std::string(argv[1])=="--host-lobby-owner"){
  {
   partyboard::net::NativeHostLobbySession session(argv[2]);
   if(session.verifyOriginalReplacementOffer("guest1",Json::object()))throw std::runtime_error("Uncommitted replacement accepted");
   bool answerRefused=false;
   try{session.signOriginalReplacementAnswer("guest1",{{"type","answer"},{"sdp","fixture"}});}
   catch(const std::exception&){answerRefused=true;}
   if(!answerRefused)throw std::runtime_error("Unauthenticated replacement answer signed");
   while(!session.prepared()){
    if(!session.tick())throw std::runtime_error(session.error());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
   }
   for(unsigned slot=1;slot<=3;++slot)session.addGuest("Guest "+std::to_string(slot));
   const auto snapshot=session.snapshot();
   const auto config=partyboard::net::nativeSessionConfiguration(std::string(32,'1'),123,session.manifest().at("id"),std::string(64,'2'));
   session.retainConfiguration(config,std::string(64,'2'));
   session.retainConfiguration(config,std::string(64,'2'));
   auto changed=config;changed["seed"]=124;
   bool driftRejected=false;try{session.retainConfiguration(changed,std::string(64,'2'));}catch(const std::exception&){driftRejected=true;}
   if(!driftRejected)throw std::runtime_error("Retained native configuration drift accepted");
   const partyboard::net::PreparedStartRuntime fixture{config.at("assetId"),config.at("buildId"),123,7,7,true};
   auto invalidReceipt=partyboard::net::nativePreparedRuntimeReport(fixture,config,true);
   invalidReceipt["checkpoint"]=8;
   bool receiptRejected=false;try{session.publishHostPrepared(fixture,invalidReceipt);}catch(const std::exception&){receiptRejected=true;}
   if(!receiptRejected||session.snapshot()!=snapshot)throw std::runtime_error("Mismatched host receipt changed readiness");
   bool proposalRejected=false;try{session.proposePreparedStart(fixture);}catch(const std::exception&){proposalRejected=true;}
   if(!proposalRejected||session.snapshot()!=snapshot)throw std::runtime_error("Unprepared proposal changed owned lobby");
   bool inputRejected=false;try{session.prepareCommittedInputTransport(fixture);}catch(const std::exception&){inputRejected=true;}
   if(!inputRejected||session.snapshot()!=snapshot)throw std::runtime_error("Unacknowledged input preparation changed lobby");
   struct RefuseBinding {
    unsigned installs=0,detaches=0;
    bool installOriginalSession(const partyboard::net::RecoverySession&){++installs;return false;}
    bool detachStopped(){++detaches;return true;}
   } binding;
   if(session.deliverInstalledInputCommit(fixture,binding)||binding.installs||binding.detaches||session.snapshot()!=snapshot)
    throw std::runtime_error("Unacknowledged commit touched input binding or lobby");
   bool incompleteRefused=false;try{session.preparedReceipts();}catch(const std::exception&){incompleteRefused=true;}
   if(session.preparedReceiptsComplete()||!incompleteRefused)throw std::runtime_error("Unprepared lobby exposed complete receipts");
   if(snapshot.at("players").size()!=4||snapshot.at("canStart")!=false)throw std::runtime_error("Unexpected unprepared native lobby readiness");
   bool startRefused=false;try{session.freezeReadyLobby();}catch(const std::exception&){startRefused=true;}
   if(!startRefused||session.snapshot()!=snapshot)throw std::runtime_error("Unprepared native start mutated or froze lobby");
   bool refused=false;try{session.addGuest("Fifth player");}catch(const std::exception&){refused=true;}
   if(!refused)throw std::runtime_error("Fifth native player accepted");
   report({{"type","owned-lobby"},{"snapshot",snapshot}});
   session.close();session.close();
   if(session.tick()||session.prepared())throw std::runtime_error("Closed native lobby retained activity");
  }
  rtc::Cleanup().wait();return 0;
 }
 if(argc==3&&std::string(argv[1])=="--cancel-asset-preparation"){
  partyboard::net::NativeAssetPreparation preparation(argv[2],"Mario Party 4.ciso");
  preparation.close();
  if(!preparation.done()||preparation.result())throw std::runtime_error("Cancelled preparation retained work or result");
  preparation.close();
  report({{"type","cancelled"},{"done",preparation.done()}});return 0;
 }
 if(argc==3&&std::string(argv[1])=="--prepare-asset-only"){
  partyboard::net::NativeAssetPreparation preparation(argv[2],"Mario Party 4.ciso");
  while(!preparation.done())std::this_thread::sleep_for(std::chrono::milliseconds(10));
  const auto result=preparation.result();if(!result)throw std::runtime_error(preparation.error());
  report({{"type","prepared"},{"manifest",result->asset.manifest()}});
  preparation.close();if(preparation.result())throw std::runtime_error("Closed preparation retained result");
  return 0;
 }
 if(argc==3&&(std::string(argv[1])=="--initial-invitation"||std::string(argv[1])=="--initial-three-invitations"||std::string(argv[1])=="--initial-three-asset-lobby")){
  const bool lobbyMode=std::string(argv[1])=="--initial-three-asset-lobby";
  const unsigned count=std::string(argv[1])=="--initial-invitation"?1:3;
  partyboard::net::NativeAssetPreparation preparation(argv[2],"Mario Party 4.ciso");
  while(!preparation.done())std::this_thread::sleep_for(std::chrono::milliseconds(10));
  auto shared=preparation.result();
  if(!shared)throw std::runtime_error(preparation.error());
  preparation.close();
  {
   partyboard::net::NativeRtcSessionControl controls;
   partyboard::net::NativeLobbyState lobbyState(shared->asset.manifest().at("id"),shared->asset.manifest().at("size"));
   lobbyState.join("host","Host");
   partyboard::net::NativeHostLobbyControl lobby(lobbyState,controls);
   std::vector<std::unique_ptr<partyboard::net::NativeHostGuest>> guests;
   for(unsigned slot=1;slot<=count;++slot){
    const auto peer="guest"+std::to_string(slot);
    auto guest=std::make_unique<partyboard::net::NativeHostGuest>(peer,controls,shared);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    std::optional<std::string> offer;
    while(!(offer=guest->invitation())){
     if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Initial asset invitation gathering timeout");
     std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    report({{"type","initial-invitation"},{"peer",peer},{"code",*offer},{"manifest",shared->asset.manifest()}});
    std::string reply;if(!std::getline(std::cin,reply)||reply.size()>100000)throw std::runtime_error("Initial asset reply required");
    guest->acceptReply(reply);guests.push_back(std::move(guest));
    if(lobbyMode)lobby.attachAssigned(peer,"Guest "+std::to_string(slot));
   }
   if(lobbyMode)lobby.reportLocal("host",{{"assetId",shared->asset.manifest().at("id")},{"sequence",1},{"bytes",shared->asset.manifest().at("size")},{"verified",true},{"shaders",false},{"ready",false}});
   std::jthread observer([&](std::stop_token token){
    bool announced=false;
    while(!token.stop_requested()){
     if(lobbyMode&&!lobby.pump()){report({{"type","error"},{"message","Native asset lobby report failed"}});return;}
     bool connected=true,complete=true;
     for(unsigned slot=0;slot<count;++slot){
      connected=connected&&controls.connected("guest"+std::to_string(slot+1));
      complete=complete&&guests[slot]->assetsComplete();
      if(auto error=guests[slot]->assetError();!error.empty()){report({{"type","error"},{"message",error}});return;}
     }
     if(!announced&&connected){if(lobbyMode&&!lobby.broadcast()){report({{"type","error"},{"message","Native asset lobby broadcast failed"}});return;}report({{"type","open"}});announced=true;}
     if(lobbyMode){
      const auto snapshot=lobbyState.snapshot();
      for(const auto& player:snapshot.at("players"))complete=complete&&player.at("verified").get<bool>();
     }
     if(complete){
      Json sent=Json::array();
      for(const auto& guest:guests)sent.push_back(guest->assetSentBytes());
      Json result={{"type","complete"},{"guests",count},{"sentBytes",sent}};
      if(lobbyMode)result["lobbySnapshot"]=lobbyState.snapshot();
      report(result);return;
     }
     std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
   });
   std::string stop;if(!std::getline(std::cin,stop)||stop!="stop")throw std::runtime_error("Explicit asset acceptance stop required");
   observer.request_stop();observer.join();
   for(unsigned slot=0;slot<count;++slot){guests[slot]->close();if(controls.hasPeer("guest"+std::to_string(slot+1)))throw std::runtime_error("Guest routes retained after close");}
   controls.close();
  }
  rtc::Cleanup().wait();return 0;
 }
 if(argc!=2)throw std::runtime_error("Private asset path required");
 auto sharedAsset=std::make_shared<partyboard::net::NativeSharedAsset>(argv[1],"Mario Party 4.ciso");
 auto& asset=sharedAsset->asset;
 auto credential=partyboard::net::createRecoveryCredential(std::string(32,'1'),asset.manifest()["id"].get<std::string>(),"guest1");
 auto identity=std::make_shared<partyboard::net::RecoveryIdentity>(credential);
 auto nonce=partyboard::net::createRecoveryNonce();
 report({{"type","original-credential"},{"credential",credential},{"manifest",asset.manifest()}});
 auto pc=std::make_shared<rtc::PeerConnection>(rtc::Configuration{});
 pc->onGatheringStateChange([pc,identity,nonce](auto state){if(state==rtc::PeerConnection::GatheringState::Complete){auto d=pc->localDescription();if(d)report({{"type","offer"},{"packet",identity->sign(Json{{"type","offer"},{"sdp",std::string(*d)}},nonce)}});}});
 auto channel=pc->createDataChannel("partyboard-assets-v1");
 partyboard::net::NativeRtcAssetHost sender(sharedAsset,channel);
 // Acceptance observer reads status only; the production owner performs all
 // file access/control processing and owns callback lifetime.
 std::jthread observer([&](std::stop_token token){
  bool announced=false;
  while(!token.stop_requested()){
   if(!announced&&channel->isOpen()){report({{"type","open"}});announced=true;}
   if(sender.complete()){report({{"type","complete"},{"id",asset.manifest()["id"]},{"size",asset.manifest()["size"]}});return;}
   if(const auto error=sender.error();!error.empty()){report({{"type","error"},{"message",error}});return;}
   std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
 });
 std::string line;while(std::getline(std::cin,line)){if(line.size()>65536)throw std::runtime_error("Signaling command too large");auto command=Json::parse(line);if(command.value("type","")=="stop")break;if(command.value("type","")!="answer"||!identity->verify(command.at("packet"),"answer",nonce))throw std::runtime_error("Original bound signed answer required");pc->setRemoteDescription(rtc::Description(command.at("packet").at("description").at("sdp").get<std::string>(),"answer"));}
 observer.request_stop();observer.join();sender.close();pc->resetCallbacks();pc->close();channel.reset();pc.reset();rtc::Cleanup().wait();return 0;
 }catch(const std::exception&e){report({{"type","error"},{"message",e.what()}});return 1;}}



