#include "port/native_paused_poll_reconciliation.hpp"
#include "port/native_paused_state_agreement.hpp"
#include <iostream>
#include <chrono>
using namespace partyboard::net;
using Json=nlohmann::ordered_json;
int main(){try{
 SessionId session{};session.fill(1);
 RecoverySession identity{"01010101010101010101010101010101",std::string(64,'b'),std::string(64,'a'),"unlocked-no-save-v1",0,8,4,{{"host",0},{"guest1",1}}};
 RecoverablePadExchange q(session,{11,22,0,0},11,{0,-1,-1,-1});
 if(!q.bindOriginalSession(identity,"host","host"))throw std::runtime_error("Fixture session rejected");
 PADStatus pads[4]{};pads[0].button=7;
 if(!q.begin(pads,{42,3,5},0)||!q.disconnected(22,0))throw std::runtime_error("Fixture initial poll rejected");
 const auto started=std::chrono::steady_clock::now();
 auto now=[&]{return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count());};
 uint32_t frame=0;RecoverablePadExchange::State state{42,3,5};
 auto capture=[&]{return q.capture(22,frame,state,now());};
 auto send=[](const Json& packet){std::cout<<packet.dump()<<std::endl;return bool(std::cout);};
 NativePausedPollReconciliation reconciliation(q,identity,"host","guest1",std::string(32,'c'),
   [&]()->std::optional<PausedCapture>{return capture();},[]{return true;},now,60000,send);
 std::unique_ptr<NativePausedStateAgreement> agreement;
 std::string line;
 while(std::getline(std::cin,line)){
  const auto packet=Json::parse(line);
  if(packet.value("type","")=="fixture-tick"){
   if(agreement)agreement->tick();else reconciliation.tick();
  }else if(agreement)agreement->receive("guest1",packet);
  else reconciliation.receive("guest1",packet);
  // Synthetic engine boundary for cross-language protocol verification only.
  // The production callback advances the actual engine instead.
  if(!agreement&&q.consumePausedReplay(state,pads,now())==Exchange::Ready){
   if(pads[0].button!=7||pads[1].button!=42)throw std::runtime_error("Original PAD replay differs");
   frame=1;state.checkpoint=43;if(!q.begin(pads,state,now()))throw std::runtime_error("Fixture next boundary rejected");
   reconciliation.tick();
  }
  if(reconciliation.complete()&&!agreement)
   agreement=std::make_unique<NativePausedStateAgreement>(identity,"host","guest1",std::string(32,'c'),capture,[]{return true;},now,60000,send);
  if(agreement&&agreement->complete()){
   send({{"type","fixture-complete"},{"frame",frame},{"inputFrame",std::to_string(q.nextFrame())},
     {"paused",q.tick(now())==ReconnectPolicy::State::Paused},{"remainingMs",q.remainingRecoveryMs(now())}});return 0;
  }
 }
 throw std::runtime_error("Fixture peer closed before strict agreement");
}catch(const std::exception& error){std::cerr<<error.what()<<std::endl;return 1;}}
