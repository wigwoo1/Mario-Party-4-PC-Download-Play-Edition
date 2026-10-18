#ifdef NDEBUG
#undef NDEBUG
#endif
#include "port/native_rtc_control_channel.hpp"
#include "port/native_prepared_report.hpp"
#include "port/native_start_coordinator.hpp"
#include "port/native_rtc_session_control.hpp"
#include "port/native_host_start_control.hpp"
#include "port/native_invitation.hpp"
#include "port/native_host_invitation.hpp"
#include "port/native_rtc_host_input.hpp"
#include "port/native_host_lobby_control.hpp"
#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
using namespace partyboard::net;
static void negotiatedSessionChannels(){
 using Purpose=NativeRtcControlChannel::Purpose;
 const std::array purposes{Purpose::Preparation,Purpose::Start,Purpose::Lobby};
 auto host=std::make_shared<rtc::PeerConnection>(rtc::Configuration{});
 auto guest=std::make_shared<rtc::PeerConnection>(rtc::Configuration{});
 NativeRtcSessionControl hostOwner,guestOwner;
 std::mutex pendingMutex;std::vector<std::shared_ptr<rtc::DataChannel>> pending;
 guest->onDataChannel([&](std::shared_ptr<rtc::DataChannel> channel){std::lock_guard lock(pendingMutex);pending.push_back(std::move(channel));});
 host->onGatheringStateChange([host,guest](rtc::PeerConnection::GatheringState state){if(state==rtc::PeerConnection::GatheringState::Complete)guest->setRemoteDescription(*host->localDescription());});
 guest->onGatheringStateChange([host,guest](rtc::PeerConnection::GatheringState state){if(state==rtc::PeerConnection::GatheringState::Complete)host->setRemoteDescription(*guest->localDescription());});
 std::array<std::shared_ptr<rtc::DataChannel>,3> hostChannels;
 for(size_t i=0;i<purposes.size();++i){hostChannels[i]=host->createDataChannel(NativeRtcControlChannel::label(purposes[i]));hostOwner.attachAuthenticated("guest1",purposes[i],hostChannels[i]);}
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
 auto pause=[&]{if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Negotiated session channels timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));};
 while(!hostOwner.connected("guest1")||!guestOwner.connected("host")){
  std::vector<std::shared_ptr<rtc::DataChannel>> ready;
  {std::lock_guard lock(pendingMutex);ready.swap(pending);}
  for(auto& channel:ready){
   const auto found=std::find_if(purposes.begin(),purposes.end(),[&](auto purpose){return channel->label()==NativeRtcControlChannel::label(purpose);});
   assert(found!=purposes.end());guestOwner.attachAuthenticated("host",*found,std::move(channel));
  }
  pause();
 }
 // Interleave all protocols before consuming any of them. A shared peer route
 // mailbox would retire earlier protocols and lose these messages.
 for(size_t i=0;i<purposes.size();++i){
  assert(hostOwner.send("guest1",purposes[i],"host-"+std::to_string(i)));
  assert(guestOwner.send("host",purposes[i],"guest-"+std::to_string(i)));
 }
 for(size_t i=0;i<purposes.size();++i){
  std::optional<NativeControlMailbox::Message> atHost,atGuest;
  while(!atHost||!atGuest){if(!atHost)atHost=hostOwner.take(purposes[i]);if(!atGuest)atGuest=guestOwner.take(purposes[i]);if(!atHost||!atGuest)pause();}
  assert(atHost->route.peer=="guest1"&&atHost->text=="guest-"+std::to_string(i));
  assert(atGuest->route.peer=="host"&&atGuest->text=="host-"+std::to_string(i));
 }
 const auto asset=std::string(64,'a'),build=std::string(64,'b');
 {
  NativeLobbyState state(asset,50000);state.join("host","Host");
  NativeHostLobbyControl signaling(state,hostOwner);signaling.attachAssigned("guest1","Guest");
  bool receiptAvailable=false;
  signaling.setPreparationGate([&](const std::string& peer){assert(peer=="guest1");return receiptAvailable;});
  nlohmann::ordered_json report={{"assetId",asset},{"sequence",1},{"bytes",50000},{"verified",true},{"shaders",true},{"ready",true},{"membership",2}};
  assert(signaling.reportLocal("host",report));
  assert(guestOwner.send("host",Purpose::Lobby,nlohmann::ordered_json{{"type","lobby-report"},{"version",1},{"report",report}}.dump()));
  for(unsigned pump=0;pump<30;++pump){assert(signaling.pump());pause();}
  assert(state.snapshot().at("canStart")==false);
  const auto gatedSnapshot=state.snapshot();
  for(const auto& player:gatedSnapshot.at("players"))if(player.at("peer")=="guest1")assert(player.at("shaders")==false&&player.at("ready")==false);
  receiptAvailable=true;
  while(!state.snapshot().at("canStart").get<bool>()){assert(signaling.pump());pause();}
  bool received=false;
  while(!received){if(auto packet=guestOwner.take(Purpose::Lobby)){const auto envelope=nlohmann::ordered_json::parse(packet->text);assert(envelope.at("type")=="lobby-snapshot");received=envelope.at("snapshot").at("canStart").get<bool>();}if(!received)pause();}
  assert(signaling.freezeMembership()==state.snapshot());
  bool refused=false;try{signaling.reportLocal("host",report);}catch(const std::runtime_error&){refused=true;}assert(refused);
  NativeLobbyState invalidState(asset,50000);invalidState.join("host","Host");
  NativeHostLobbyControl invalidSignaling(invalidState,hostOwner);invalidSignaling.attachAssigned("guest1","Guest");
  assert(guestOwner.send("host",Purpose::Lobby,"{\"type\":\"lobby-report\",\"version\":2}"));
  while(invalidSignaling.pump())pause();
  assert(!invalidSignaling.pump()&&!invalidSignaling.broadcast());
  assert(invalidSignaling.error()=="Invalid native lobby report envelope");
  bool failedFreeze=false;try{invalidSignaling.freezeMembership();}catch(const std::runtime_error&){failedFreeze=true;}assert(failedFreeze);
 }
 const auto configuration=nativeSessionConfiguration(std::string(32,'1'),7,asset,build);
 const PreparedStartRuntime prepared{asset,build,7,123,123,true};
 const auto receipt=nativePreparedRuntimeReport(prepared,configuration,true);
 PreparedStartLobby lobby{asset,50000,0,0,{{"host",0,true,true,true,50000},{"guest1",1,true,true,true,50000}}};
 NativeStartCoordinator coordinator(lobby,configuration);NativeHostStartControl startControl(hostOwner,coordinator);
 assert(!coordinator.preparedReceiptsComplete());
 assert(coordinator.receivePrepared("host",receipt.dump()));
 assert(!coordinator.preparedReceiptsComplete());
 assert(guestOwner.send("host",Purpose::Preparation,receipt.dump()));
 // Production bridge drains authenticated preparation on the game thread.
 while(!coordinator.preparedReceiptsComplete()){assert(startControl.pumpPrepared());if(!coordinator.preparedReceiptsComplete())pause();}
 assert(startControl.propose(prepared,receipt,"host"));
 assert(!coordinator.preparedReceiptsComplete());
 std::optional<NativeControlMailbox::Message> proposal;
 while(!(proposal=guestOwner.take(Purpose::Start)))pause();
 auto ack=nlohmann::ordered_json::parse(proposal->text);ack["type"]="session-start-ack";
 assert(guestOwner.send("host",Purpose::Start,ack.dump()));
 for(;;){assert(startControl.pumpAcknowledgments());try{coordinator.prepareCommit(prepared);break;}catch(const std::runtime_error&){pause();}}
 assert(coordinator.deliverCommit(prepared,"host",[&](const std::string& peer,const std::string& packet){return startControl.sendCommit(peer,packet);}));
 std::optional<NativeControlMailbox::Message> commit;
 while(!(commit=guestOwner.take(Purpose::Start)))pause();
 assert(nlohmann::ordered_json::parse(commit->text).at("type")=="session-start-commit");
 // A remaining open start channel cannot authorize a commit after the peer
 // loses another required control channel.
 NativeLobbyState lossState(asset,50000);lossState.join("host","Host");
 NativeHostLobbyControl lossSignaling(lossState,hostOwner);lossSignaling.attachAssigned("guest1","Guest");
 assert(lossSignaling.pump()); // Observe this peer connected before loss.
 hostChannels[0]->close();
 while(hostOwner.connected("guest1"))pause();
 assert(!startControl.sendCommit("guest1",commit->text));
 assert(!lossSignaling.pump()&&!lossSignaling.broadcast());
 assert(lossSignaling.error()=="Native lobby control connection lost for guest1");
 bool lossFreeze=false;try{lossSignaling.freezeMembership();}catch(const std::runtime_error&){lossFreeze=true;}assert(lossFreeze);
 guestOwner.retireAuthenticated("host");assert(!guestOwner.connected("host"));
 for(auto purpose:purposes){assert(!guestOwner.take(purpose));assert(!guestOwner.send("host",purpose,"late"));}
 hostOwner.close();guestOwner.close();host->resetCallbacks();guest->resetCallbacks();host->close();guest->close();
 std::cout<<"PASS three negotiated production session channels: independent bidirectional routes and retired peer refusal\n";
}
int main(int argc,char** argv){try{
 if(argc==2&&(std::string(argv[1])=="--invitation-peer"||std::string(argv[1])=="--invitation-three-peer"||std::string(argv[1])=="--invitation-lobby-three-peer")){
  const bool lobbyMode=std::string(argv[1])=="--invitation-lobby-three-peer";
  const unsigned count=std::string(argv[1])=="--invitation-peer"?1:3;
  NativeRtcSessionControl controls;std::vector<std::unique_ptr<NativeHostInvitation>> owners;
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
  auto pause=[&]{if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Initial invitation negotiation timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));};
  for(unsigned slot=1;slot<=count;++slot){
   const auto peer="guest"+std::to_string(slot);auto pending=std::make_unique<NativeHostInvitation>(peer,controls);
   std::optional<std::string> code;while(!(code=pending->invitation()))pause();
   std::cout<<nlohmann::ordered_json({{"type","invitation"},{"peer",peer},{"code",*code}}).dump()<<std::endl;
   std::string reply;std::getline(std::cin,reply);pending->acceptReply(reply);
   while(!controls.connected(peer))pause();owners.push_back(std::move(pending));
  }
  for(auto purpose:{NativeRtcControlChannel::Purpose::Preparation,NativeRtcControlChannel::Purpose::Start,NativeRtcControlChannel::Purpose::Lobby}){
   for(unsigned slot=1;slot<=count;++slot)assert(controls.send("guest"+std::to_string(slot),purpose,NativeRtcControlChannel::label(purpose)));
   std::set<std::string> replies;
   while(replies.size()<count){std::optional<NativeControlMailbox::Message> received;while(!(received=controls.take(purpose)))pause();
    assert(received->text==std::string("reply:")+NativeRtcControlChannel::label(purpose));assert(replies.insert(received->route.peer).second);
   }
  }
  if(lobbyMode){
   const auto asset=std::string(64,'a');NativeLobbyState state(asset,50000);state.join("host","Host");
   NativeHostLobbyControl lobby(state,controls);
   for(unsigned slot=1;slot<=count;++slot)lobby.attachAssigned("guest"+std::to_string(slot),"Guest "+std::to_string(slot));
   assert(lobby.reportLocal("host",{{"assetId",asset},{"sequence",1},{"bytes",50000},{"verified",true},{"shaders",true},{"ready",true},{"membership",4}}));
   while(!state.snapshot().at("canStart").get<bool>()){assert(lobby.pump());pause();}
   const auto frozen=lobby.freezeMembership();assert(frozen.at("players").size()==4);
   std::cout<<nlohmann::ordered_json{{"type","lobby-passed"},{"snapshot",frozen}}.dump()<<std::endl;
   // Wait for actual browser clients to acknowledge their final snapshot before
   // closing, rather than accepting only native send-queue submission.
   std::string done;std::getline(std::cin,done);assert(done=="lobby-observed");
  }
  std::cout<<"{\"type\":\"passed\"}"<<std::endl;controls.close();owners.clear();return 0;
 }
 {NativeRtcSessionControl controls;NativeHostInvitation pending("guest1",controls);pending.close();assert(!pending.invitation());}
 if(argc==2&&std::string(argv[1])=="--invitation-codec"){
  std::string line;std::getline(std::cin,line);
  if(line.size()>150000)throw std::invalid_argument("Codec request exceeds bound");
  const auto request=nlohmann::ordered_json::parse(line);
  if(request.at("mode")=="encode")std::cout<<encodeNativeInvitation(request.at("packet"))<<'\n';
  else if(request.at("mode")=="decode")std::cout<<receiveNativeInvitation(request.at("code").get<std::string>(),request.at("type").get<std::string>(),request.value("peer",std::string{})).dump()<<'\n';
  else throw std::invalid_argument("Unknown invitation codec mode");
  return 0;
 }
 const nlohmann::ordered_json invitation={{"version",1},{"playerId","guest1"},{"description",{{"type","offer"},{"sdp","v=0\r\n"}}}};
 const auto code=encodeNativeInvitation(invitation);assert(receiveNativeInvitation(code,"offer","guest1")==invitation);
 bool wrongReply=false;try{receiveNativeInvitation(code,"offer","guest2");}catch(const std::invalid_argument&){wrongReply=true;}assert(wrongReply);
 negotiatedSessionChannels();
 auto mailbox=std::make_shared<NativeControlMailbox>();
 auto peer=std::make_shared<rtc::PeerConnection>(rtc::Configuration{});
 auto input=peer->createDataChannel("partyboard-input-v1");
 {
  SessionId inputSession{};inputSession[0]=1;
  NativeRtcHostInput inputOwner(inputSession,0,15);
  for(const auto label:{"partyboard-assets-v1","partyboard-preparation-v1","partyboard-start-v1","partyboard-lobby-v1"}){
   bool refused=false;
   try{inputOwner.attachAuthenticated(1,peer->createDataChannel(label));}catch(const std::invalid_argument&){refused=true;}
   assert(refused);
  }
  inputOwner.attachAuthenticated(1,input);
  inputOwner.closeStopped();
 }
 bool rejected=false;try{NativeRtcControlChannel wrong(mailbox,"guest1",input);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
 auto channel=peer->createDataChannel("partyboard-control-v1");
 NativeRtcControlChannel control(mailbox,"guest1",channel);
 assert(!control.connected()&&!control.send("{}"));control.close();control.close();
 assert(!control.connected()&&!control.send("{}")&&!mailbox->take());
 for(const auto purpose:{NativeRtcControlChannel::Purpose::Preparation,NativeRtcControlChannel::Purpose::Start,NativeRtcControlChannel::Purpose::Lobby}){
  auto purposeChannel=peer->createDataChannel(NativeRtcControlChannel::label(purpose));
  bool wrongPurpose=false;try{NativeRtcControlChannel wrong(mailbox,"guest1",purposeChannel);}catch(const std::invalid_argument&){wrongPurpose=true;}assert(wrongPurpose);
  NativeRtcControlChannel owned(std::make_shared<NativeControlMailbox>(),"guest1",purposeChannel,purpose);
  assert(!owned.connected());owned.close();
 }
 input->close();peer->close();
 {
  NativeRtcSessionControl session;
  auto connection=std::make_shared<rtc::PeerConnection>(rtc::Configuration{});
  for(const auto purpose:{NativeRtcControlChannel::Purpose::Preparation,NativeRtcControlChannel::Purpose::Start,NativeRtcControlChannel::Purpose::Lobby}){
   session.attachAuthenticated("guest1",purpose,connection->createDataChannel(NativeRtcControlChannel::label(purpose)));
   assert(!session.take(purpose));
   bool duplicate=false;try{session.attachAuthenticated("guest1",purpose,connection->createDataChannel(NativeRtcControlChannel::label(purpose)));}catch(const std::invalid_argument&){duplicate=true;}assert(duplicate);
  }
  assert(!session.connected("guest1"));
  session.retireAuthenticated("guest1");session.close();session.close();connection->close();
 }
 auto host=std::make_shared<rtc::PeerConnection>(rtc::Configuration{});
 auto guest=std::make_shared<rtc::PeerConnection>(rtc::Configuration{});
 host->onGatheringStateChange([host,guest](rtc::PeerConnection::GatheringState state){if(state==rtc::PeerConnection::GatheringState::Complete)guest->setRemoteDescription(*host->localDescription());});
 guest->onGatheringStateChange([host,guest](rtc::PeerConnection::GatheringState state){if(state==rtc::PeerConnection::GatheringState::Complete)host->setRemoteDescription(*guest->localDescription());});
 auto hostBox=std::make_shared<NativeControlMailbox>(),guestBox=std::make_shared<NativeControlMailbox>();
 std::mutex receivedMutex;std::unique_ptr<NativeRtcControlChannel> guestControl;
 guest->onDataChannel([&](std::shared_ptr<rtc::DataChannel> incoming){std::lock_guard lock(receivedMutex);guestControl=std::make_unique<NativeRtcControlChannel>(guestBox,"host",incoming);});
 auto outgoing=host->createDataChannel("partyboard-control-v1");
 NativeRtcControlChannel hostControl(hostBox,"guest1",outgoing);
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
 for(;;){
  bool guestReady=false;{std::lock_guard lock(receivedMutex);guestReady=guestControl&&guestControl->connected();}
  if(hostControl.connected()&&guestReady)break;
  if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Negotiated native control channel timeout");
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
 }
 assert(hostControl.send("{\"type\":\"runtime-prepared\"}"));
 std::optional<NativeControlMailbox::Message> delivered;
 while(!(delivered=guestBox->take())){
  if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Native control delivery timeout");
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
 }
 assert(delivered->route.peer=="host"&&delivered->text=="{\"type\":\"runtime-prepared\"}");
 {std::lock_guard lock(receivedMutex);assert(guestControl&&guestControl->send("ack"));}
 while(!(delivered=hostBox->take())){
  if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Native control response timeout");
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
 }
 assert(delivered->route.peer=="guest1"&&delivered->text=="ack");
 const auto next=[&](const std::shared_ptr<NativeControlMailbox>& box){
  for(;;){if(auto message=box->take())return *message;
   if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Native start-control exchange timeout");
   std::this_thread::sleep_for(std::chrono::milliseconds(10));}
 };
 // Protocol fixture state; actual transport, no engine preparation/resume.
 const auto asset=std::string(64,'a'),build=std::string(64,'b');
 const auto configuration=nativeSessionConfiguration(std::string(32,'1'),7,asset,build);
 const PreparedStartRuntime prepared{asset,build,7,123,123,true};
 const auto receipt=nativePreparedRuntimeReport(prepared,configuration,true);
 {std::lock_guard lock(receivedMutex);assert(guestControl->send(receipt.dump()));}
 const auto guestReceiptMessage=next(hostBox);assert(guestReceiptMessage.route.peer=="guest1");
 const auto guestReceipt=receiveNativePreparedRuntimeReport(guestReceiptMessage.text,configuration);
 PreparedStartLobby lobby{asset,50000,0,0,{{"host",0,true,true,true,50000},{"guest1",1,true,true,true,50000}}};
 NativeStartCoordinator coordinator(lobby,configuration);
 assert(coordinator.receivePrepared("host",receipt.dump()));
 assert(coordinator.receivePrepared(guestReceiptMessage.route.peer,guestReceipt.dump()));
 const auto proposal=coordinator.begin(prepared,receipt);
 assert(hostControl.send(proposal.dump()));
 const auto proposalMessage=next(guestBox);assert(proposalMessage.route.peer=="host");
 auto ack=nlohmann::ordered_json::parse(proposalMessage.text);assert(ack==proposal);ack["type"]="session-start-ack";
 assert(coordinator.acknowledge("host",ack.dump()));
 {std::lock_guard lock(receivedMutex);assert(guestControl->send(ack.dump()));}
 const auto ackMessage=next(hostBox);assert(coordinator.acknowledge(ackMessage.route.peer,ackMessage.text));
 const auto commit=coordinator.prepareCommit(prepared);
 assert(coordinator.deliverCommit(prepared,"host",[&](const std::string& boundPeer,const std::string& packet){assert(boundPeer=="guest1");return hostControl.send(packet);}));
 assert(coordinator.commitDelivered());
 const auto commitMessage=next(guestBox);assert(commitMessage.route.peer=="host"&&nlohmann::ordered_json::parse(commitMessage.text)==commit);
 auto changed=lobby;changed.revision++;assert(!coordinator.update(changed));
 bool cancelled=false;try{coordinator.prepareCommit(prepared);}catch(const std::runtime_error&){cancelled=true;}assert(cancelled);
 std::cout<<"PASS actual native RTC prepared receipt, all-player start acknowledgment and commit delivery; fixture engine state only\n";
 hostControl.close();{std::lock_guard lock(receivedMutex);guestControl->close();}
 host->resetCallbacks();guest->resetCallbacks();host->close();guest->close();
 std::cout<<"PASS negotiated actual native RTC control delivery and response with coordinator-bound peer identities\n";
 std::cout<<"PASS actual native RTC control owner: dedicated label, unopened send refusal, idempotent retirement\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
