#include "port/native_paused_state_agreement.hpp"
#include <iostream>
using namespace partyboard::net;
using Json=nlohmann::ordered_json;
static void require(bool ok){if(!ok)throw std::runtime_error("Paused state agreement regression failed");}
int main(){
 for(unsigned mode=0;mode<5;++mode){
  RecoverySession identity{std::string(32,'1'),std::string(64,'b'),std::string(64,'a'),"unlocked-no-save-v1",0,8,4,{{"host",0},{"guest1",1}}};
  PausedCapture captured{2,1,1,1,1,7,7,7,42,4,5,42,4,5,true};uint64_t now=101;
  std::vector<Json> outgoing;
  NativePausedStateAgreement round(identity,"host","guest1",std::string(32,'c'),[&]{return captured;},[]{return true;},[&]{return now;},60100,
    [&](const Json& packet){outgoing.push_back(packet);return true;});
  round.tick();require(outgoing.size()==1&&!round.complete());
  auto remote=round.report();remote["reporter"]="guest1";
  Json ack{{"type","paused-state-ack"},{"version",1},{"nonce",std::string(32,'c')},{"peer","guest1"},{"accepted",round.report()}};
  if(mode==0){round.receive("guest1",ack);require(!round.complete());round.receive("guest1",remote);require(round.complete()&&outgoing.back().at("accepted")==remote);continue;}
  bool rejected=false;try{
   if(mode==1){remote["checkpoint"]=43;round.receive("guest1",remote);}
   if(mode==2){ack["accepted"]["rand8"]=6;round.receive("guest1",ack);}
   if(mode==3){now=60100;round.tick();}
   if(mode==4){captured.frand=6;captured.attemptFrand=6;round.tick();}
  }catch(...){rejected=true;}require(rejected&&!round.complete());
 }
 std::cout<<"PASS native strict paused state: both reports/acks required; checkpoint, RNG, drift, deadline rejected; fixture only\n";
}
