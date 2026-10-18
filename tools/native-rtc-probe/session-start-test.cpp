#include "port/session_start_barrier.hpp"
#include "port/native_session_configuration.hpp"
#include "port/native_prepared_report.hpp"
#include "port/native_start_coordinator.hpp"
#include <cassert>
#include <iostream>
using namespace partyboard::net;
int main(int argc,char**argv){PreparedStartLobby lobby{std::string(64,'a'),50000,5,3,{{"guest3",3,true,true,true,50000},{"host",0,true,true,true,50000},{"guest1",1,true,true,true,50000},{"guest2",2,true,true,true,50000}}};
SessionStartBarrier b(lobby,std::string(32,'1'),7,std::string(64,'b'),123);if(argc==2&&std::string(argv[1])=="--proposal"){std::cout<<b.proposal().dump()<<'\n';return 0;}
if(argc==2&&std::string(argv[1])=="--wire"){
 std::string line;for(auto peer:{"host","guest1","guest2","guest3"}){if(!std::getline(std::cin,line)||line.size()>65536)return 2;try{if(!b.acknowledge(peer,nlohmann::ordered_json::parse(line),lobby))return 3;}catch(...){return 4;}}
 if(b.state()!=SessionStartBarrier::State::Agreed)return 5;std::cout<<b.prepareCommit(lobby,{lobby.assetId,std::string(64,'b'),7,123,123,true}).dump()<<'\n';return 0;
}
auto ack=b.proposal();ack["type"]="session-start-ack";
assert(ack["players"][0]["peer"]=="host");assert(!b.acknowledge("stranger",ack,lobby));assert(b.acknowledge("host",ack,lobby));assert(b.acknowledge("host",ack,lobby));assert(b.state()==SessionStartBarrier::State::Awaiting);
for(auto peer:{"guest1","guest2","guest3"})assert(b.acknowledge(peer,ack,lobby));assert(b.state()==SessionStartBarrier::State::Agreed);
PreparedStartRuntime runtime{lobby.assetId,std::string(64,'b'),7,123,123,true};
auto configuration=nativeSessionConfiguration(std::string(32,'1'),7,lobby.assetId,runtime.buildId);
const auto receipt=nativePreparedRuntimeReport(runtime,configuration,true);
if(argc==2&&std::string(argv[1])=="--receipt"){std::cout<<nlohmann::ordered_json{{"configuration",configuration},{"receipt",receipt}}.dump()<<'\n';return 0;}
assert(receipt["frame"]==0&&receipt["checkpoint"]==123&&receipt["pipelineSeed"].is_null());
assert(receiveNativePreparedRuntimeReport(receipt.dump(),configuration)==receipt);
assert(nativePreparedRuntimeReportsMatch(receipt,receipt));
std::map<std::string,nlohmann::ordered_json> receipts;
for(const auto& player:lobby.players)receipts.emplace(player.peer,receipt);
{
 NativeStartCoordinator owner(lobby,configuration);
 for(const auto& player:lobby.players)assert(owner.receivePrepared(player.peer,receipt.dump()));
 auto proposal=owner.begin(runtime,receipt);proposal["type"]="session-start-ack";
 for(const auto& player:lobby.players)assert(owner.acknowledge(player.peer,proposal.dump()));
 unsigned sent=0;
 assert(!owner.deliverCommit(runtime,"host",[&](const std::string&,const std::string&){return ++sent<2;}));
 assert(sent==2&&!owner.commitDelivered());
 bool rejected=false;try{owner.prepareCommit(runtime);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
}
for(unsigned mode=0;mode<4;mode++){
 NativeStartCoordinator owner(lobby,configuration);
 assert(!owner.receivePrepared("stranger",receipt.dump()));
 for(const auto& player:lobby.players)assert(owner.receivePrepared(player.peer,receipt.dump()));
 assert(owner.receivePrepared("guest1",receipt.dump()));
 const auto proposal=owner.begin(runtime,receipt);assert(proposal==b.proposal());
 if(mode==0){auto changed=receipt;changed["checkpoint"]=124;assert(!owner.receivePrepared("guest1",changed.dump()));}
 else {auto changed=lobby;
  if(mode==1)changed.players[1]=changed.players[0];
  if(mode==2)changed.players[0].bytes--;
  if(mode==3)changed.membership++;
  assert(!owner.update(changed));
 }
 bool cancelled=false;try{owner.prepareCommit(runtime);}catch(const std::runtime_error&){cancelled=true;}assert(cancelled);
 assert(!owner.receivePrepared("guest1",receipt.dump()));
}
assert(nativeStartBarrierFromPreparedReceipts(lobby,configuration,runtime,receipt,receipts).proposal()==b.proposal());
for(unsigned mode=0;mode<5;mode++){
 auto bad=receipts;auto captured=runtime;auto snapshot=lobby;
 switch(mode){case 0:bad.erase("guest1");break;case 1:bad["stranger"]=receipt;break;
 case 2:bad["guest1"]["checkpoint"]=124;break;case 3:captured.checkpoint=captured.preparedCheckpoint=124;break;
 case 4:snapshot.players[0].shaders=false;break;}
 bool rejected=false;try{nativeStartBarrierFromPreparedReceipts(snapshot,configuration,captured,receipt,bad);}catch(const std::exception&){rejected=true;}
 assert(rejected);
}
auto different=receipt;different["checkpoint"]=124;
assert(!nativePreparedRuntimeReportsMatch(receipt,receiveNativePreparedRuntimeReport(different.dump(),configuration)));
for(const auto* field:{"session","assetId","buildId","seed","checkpoint","frame","shaderScope"}){
 auto bad=receipt;
 if(std::string(field)=="seed"||std::string(field)=="checkpoint")bad[field]=4294967296ULL;
 else if(std::string(field)=="frame")bad[field]=1;
 else bad[field]="invalid";
 bool rejected=false;try{receiveNativePreparedRuntimeReport(bad.dump(),configuration);}catch(const std::exception&){rejected=true;}
 assert(rejected);
}
bool oversized=false;try{receiveNativePreparedRuntimeReport(std::string(2049,' '),configuration);}catch(const std::exception&){oversized=true;}assert(oversized);
assert(nativePreparedRuntimeReport(runtime,configuration,true,std::string(64,'c'))["pipelineSeed"]==std::string(64,'c'));
for(unsigned mode=0;mode<5;mode++){
 auto bad=runtime;bool gpu=true;std::optional<std::string> pipeline;
 switch(mode){case 0:bad.idle=false;break;case 1:bad.seed++;break;case 2:bad.preparedCheckpoint++;break;
 case 3:gpu=false;break;case 4:pipeline="invalid";break;}
 bool rejected=false;try{nativePreparedRuntimeReport(bad,configuration,gpu,pipeline);}catch(const std::exception&){rejected=true;}
 assert(rejected);
}
assert(SessionStartBarrier::fromConfiguration(lobby,configuration,runtime).proposal()==b.proposal());
for(const auto* field:{"assetId","buildId","session","profile","type","version","seed"}){
 auto bad=configuration;
 if(std::string(field)=="seed")bad[field]=4294967296ULL;
 else if(std::string(field)=="version")bad[field]=2;
 else bad[field]="invalid";
 bool failed=false;try{SessionStartBarrier::fromConfiguration(lobby,bad,runtime);}catch(const std::exception&){failed=true;}
 assert(failed);
}
assert(b.prepareCommit(lobby,runtime)["type"]=="session-start-commit");
for(unsigned field=0;field<6;++field){auto bad=runtime;switch(field){case 0:bad.assetId=std::string(64,'c');break;case 1:bad.buildId=std::string(64,'c');break;case 2:bad.seed++;break;case 3:bad.checkpoint++;break;case 4:bad.preparedCheckpoint++;break;case 5:bad.idle=false;break;}bool rejected=false;try{b.prepareCommit(lobby,bad);}catch(const std::runtime_error&){rejected=true;}assert(rejected);}
SessionStartBarrier incomplete(lobby,std::string(32,'1'),7,std::string(64,'b'),123);bool rejected=false;try{incomplete.prepareCommit(lobby,runtime);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
SessionStartBarrier drift(lobby,std::string(32,'1'),7,std::string(64,'b'),123);auto wrong=ack;wrong["checkpoint"]=124;assert(!drift.acknowledge("host",wrong,lobby));assert(drift.state()==SessionStartBarrier::State::Mismatch);assert(!drift.acknowledge("host",ack,lobby));
SessionStartBarrier changed(lobby,std::string(32,'1'),7,std::string(64,'b'),123);lobby.players[0].shaders=false;assert(!changed.update(lobby));assert(changed.state()==SessionStartBarrier::State::Cancelled);
std::cout<<"PASS native prepared start all-player agreement and checkpoint/preparation rejection\n";}


