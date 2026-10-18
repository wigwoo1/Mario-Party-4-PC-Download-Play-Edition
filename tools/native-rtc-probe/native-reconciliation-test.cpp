#include "port/native_paused_poll_reconciliation.hpp"
#include <iostream>
using namespace partyboard::net;
using Json=nlohmann::ordered_json;
static void require(bool value){if(!value)throw std::runtime_error("Reconciliation regression failed");}
int main(){
 for(unsigned mode=0;mode<4;++mode){
  SessionId session{};session.fill(0x11);
  RecoverySession identity{std::string(32,'1'),std::string(64,'b'),std::string(64,'a'),"unlocked-no-save-v1",0,8,4,{{"host",0},{"guest1",1}}};
  RecoverablePadExchange q(session,{1,2,0,0},1,{0,-1,-1,-1});
  require(q.bindOriginalSession(identity,"host","host"));
  PADStatus pads[4]{};pads[0].button=7;
  require(q.begin(pads,{42,4,5},100)&&q.disconnected(2,100));
  uint64_t now=101;uint32_t frame=0;RecoverablePadExchange::State state{42,4,5};bool executing=false;
  std::vector<Json> outgoing;
  NativePausedPollReconciliation round(q,identity,"host","guest1",std::string(32,'c'),
    [&]()->std::optional<PausedCapture>{if(executing)return std::nullopt;return q.capture(2,frame,state,now);},
    []{return true;},[&]{return now;},60100,[&](const Json& packet){outgoing.push_back(packet);return true;});
  round.tick();require(outgoing.size()==1&&outgoing[0]==round.report());
  if(mode==3){now=60100;bool rejected=false;try{round.tick();}catch(...){rejected=true;}require(rejected&&!round.complete()&&q.tick(now)==ReconnectPolicy::State::ReturnToMenu);continue;}
  auto remote=round.report();remote["peer"]="guest1";remote["report"]["reporter"]="guest1";
  if(mode==1){
   remote["report"]["frame"]=1;remote["report"]["inputFrame"]="1";remote["report"]["checkpoint"]=43;
   std::array<uint8_t,44> merged{};merged[0]=7;merged[11]=42;merged[32]=255;merged[43]=255;
   remote["receipt"]={{"frame","0"},{"checkpoint",42},{"frand",4},{"rand8",5},{"pads",merged}};
  }
  round.receive("guest1",remote);require(!round.complete());
  if(mode==1){
   require(q.consumePausedReplay(state,pads,now)==Exchange::Ready&&pads[1].button==42);
   executing=true;round.tick();require(!round.complete());
   frame=1;state.checkpoint=43;require(q.begin(pads,state,now));executing=false;round.tick();
  }
  const auto actual=nativePausedStateReport(identity,q.capture(2,frame,state,now),"guest1",std::string(32,'c'));
  Json acknowledgment{{"type","paused-poll-ack"},{"version",1},{"nonce",std::string(32,'c')},{"peer","guest1"},{"accepted",round.report()},{"state",actual}};
  if(mode==2){acknowledgment["accepted"]["receipt"]=1;bool rejected=false;try{round.receive("guest1",acknowledgment);}catch(...){rejected=true;}require(rejected&&!round.complete()&&q.tick(now)==ReconnectPolicy::State::ReturnToMenu);}
  else {round.receive("guest1",acknowledgment);require(round.complete()&&q.tick(now)==ReconnectPolicy::State::Paused&&q.remainingRecoveryMs(now)==59999);require(outgoing.back().at("accepted")==remote);}
 }
 std::cout<<"PASS native paused reconciliation: matching and one-frame replay, exact acknowledgments, preserved pause/deadline; fixture only\n";
}
