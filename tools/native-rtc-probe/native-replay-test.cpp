#include "port/native_recovery_poll_replay.hpp"
#include "port/recoverable_pad_exchange.hpp"
#include <iostream>
using namespace partyboard::net;
using Json=nlohmann::ordered_json;
static void require(bool ok){if(!ok)throw std::runtime_error("Native replay regression failed");}
template<class F>void rejects(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}require(rejected);}
int main(){
 for(bool drift:{false,true}){
  SessionId sid{};sid.fill(0x11);
  RecoverySession originalIdentity{std::string(32,'1'),std::string(64,'b'),std::string(64,'a'),"unlocked-no-save-v1",0,8,4,{{"host",0},{"guest1",1}}};
  RecoverablePadExchange queue(sid,{1,2,0,0},1,{0,-1,-1,-1});
  require(queue.bindOriginalSession(originalIdentity,"host","host"));
  PADStatus sample[4]{};sample[0].button=7;
  require(queue.begin(sample,{42,4,5},100));require(queue.disconnected(2,100));
  auto target=nativePausedStateReport(originalIdentity,queue.capture(2,0,{42,4,5},101),"guest1",std::string(32,'c'));
  target["frame"]=1;target["inputFrame"]="1";target["checkpoint"]=43;
  std::array<uint8_t,44> merged{};merged[0]=7;merged[11]=42;merged[32]=255;merged[43]=255;
  Json accepted{{"frame","0"},{"checkpoint",42},{"frand",4},{"rand8",5},{"pads",merged}};
  const auto epoch=queue.epoch(),revision=queue.revision();
  queue.preparePausedReplay("guest1",std::string(32,'c'),target,accepted,2,0,{42,4,5},101);
  require(queue.consumePausedReplay({42,4,5},sample,102)==Exchange::Ready);
  require(queue.nextFrame()==1&&sample[1].button==42&&queue.completedPoll()->pads==merged);
  require(queue.tick(102)==ReconnectPolicy::State::Paused&&queue.epoch()==epoch&&queue.revision()==revision&&queue.remainingRecoveryMs(102)==59998);
  require(queue.consumePausedReplay({42,4,5},sample,102)==Exchange::Waiting);
  require(queue.begin(sample,{drift?44u:43u,4,5},103)==!drift);
  if(drift)require(queue.tick(103)==ReconnectPolicy::State::ReturnToMenu);
  else {require(queue.poll({43,4,5},sample,103)==Exchange::Waiting);require(queue.capture(2,1,{43,4,5},103).inputFrame==1);require(queue.remainingRecoveryMs(103)==59997);
   auto second=nativePausedStateReport(originalIdentity,queue.capture(2,1,{43,4,5},103),"guest1",std::string(32,'d'));
   second["frame"]=2;second["inputFrame"]="2";second["checkpoint"]=44;
   auto secondReceipt=accepted;secondReceipt["frame"]="1";secondReceipt["checkpoint"]=43;
   rejects([&]{queue.preparePausedReplay("guest1",std::string(32,'d'),second,secondReceipt,2,1,{43,4,5},103);});
   require(queue.nextFrame()==1&&queue.tick(103)==ReconnectPolicy::State::Paused);require(queue.tick(60100)==ReconnectPolicy::State::ReturnToMenu);}
 }
 RecoverySession identity{std::string(32,'1'),std::string(64,'b'),std::string(64,'a'),"unlocked-no-save-v1",0,8,4,{{"host",0},{"guest1",1}}};
 PausedCapture captured{2,1,1,1,1,7,7,7,42,4,5,42,4,5,true};
 SessionId session{};session.fill(0x11);Pad local{};local[0]=7;
 std::array<Packet,1> original{encode({session,7,42,0,local})};
 std::array<uint32_t,4> owners{1,2,0,0};
 auto report=nativePausedStateReport(identity,captured,"host",std::string(32,'c'));
 report["reporter"]="guest1";report["frame"]=8;report["inputFrame"]="8";report["checkpoint"]=43;
 std::array<uint8_t,44> pads{};pads[0]=7;pads[11]=42;pads[32]=255;pads[43]=255;
 Json receipt{{"frame","7"},{"checkpoint",42},{"frand",4},{"rand8",5},{"pads",pads}};
 const auto replay=validateNativePollReplay(identity,captured,"host","guest1",std::string(32,'c'),report,receipt,owners,original);
 require(replay.packets.size()==2&&replay.packets[0].second==original[0]&&decode(replay.packets[1].second)->pad[0]==42);
 auto changed=receipt;changed["pads"][0]=8;
 rejects([&]{validateNativePollReplay(identity,captured,"host","guest1",std::string(32,'c'),report,changed,owners,original);});
 changed=receipt;changed["pads"][22]=1;
 rejects([&]{validateNativePollReplay(identity,captured,"host","guest1",std::string(32,'c'),report,changed,owners,original);});
 changed=report;changed["frame"]=9;
 rejects([&]{validateNativePollReplay(identity,captured,"host","guest1",std::string(32,'c'),changed,receipt,owners,original);});
 changed=report;changed["assetId"]=std::string(64,'d');
 rejects([&]{validateNativePollReplay(identity,captured,"host","guest1",std::string(32,'c'),changed,receipt,owners,original);});
 changed=receipt;changed["rand8"]=6;
 rejects([&]{validateNativePollReplay(identity,captured,"host","guest1",std::string(32,'c'),report,changed,owners,original);});
 std::cout<<"PASS native replay evidence: retained local packets, exact one-frame identity/state, inactive PAD rejection; fixture only\n";
}
