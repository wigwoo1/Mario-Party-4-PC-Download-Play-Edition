#include "port/recovery_barrier.hpp"
#include "port/paused_state.hpp"
#include <iostream>
using namespace partyboard::net;
using Json=RecoveryBarrier::Json;
static void require(bool value){if(!value)throw std::runtime_error("Recovery barrier test failed");}
template<class F> static void rejects(F call){bool rejected=false;try{call();}catch(const std::exception&){rejected=true;}require(rejected);}
int main(int argc,char** argv){
    RecoverySession identity{std::string(32,'1'),std::string(64,'b'),std::string(64,'a'),"unlocked-no-save-v1",0,8,4,
        {{"host",0},{"g1",1},{"g2",2},{"g3",3}}};
    PausedCapture capture{1,1,1,1,1,7,7,10,42,0,5,42,0,5,true};
    const auto original=recoveryProposal(identity,capture,"host",std::string(32,'c'));
    const auto paused=nativePausedStateReport(identity,capture,"host",std::string(32,'c'));
    require(paused.at("type")=="paused-state-report"&&paused.at("reporter")=="host"&&paused.at("inputFrame")=="7"&&!paused.contains("host")&&!paused.contains("epoch"));
    rejects([&]{nativePausedStateReport(identity,capture,"unassigned",std::string(32,'c'));});
    if(argc==2&&std::string(argv[1])=="--paused-wire"){std::cout<<paused.dump()<<'\n';return 0;}
    if(argc==2&&std::string(argv[1])=="--accept") {
        std::string line;if(!std::getline(std::cin,line))return 2;
        return acceptRecoveryCommit(Json::parse(line),original,identity,capture,"g1","host")?0:2;
    }
    if(argc==2&&std::string(argv[1])=="--wire") {
        RecoveryBarrier wire([&]{return original;},[]{return true;},[]{return uint64_t(0);},60000);
        std::cout<<original.dump()<<'\n';
        for(const auto* peer:{"g1","g2","g3"}) {
            std::string line;if(!std::getline(std::cin,line))throw std::runtime_error("Browser acknowledgment missing");
            wire.acknowledge(peer,Json::parse(line));
        }
        std::cout<<wire.commit().dump()<<'\n';return 0;
    }
    for(unsigned scenario=0;scenario<8;++scenario){
        auto badIdentity=identity;auto badCapture=capture;
        if(scenario==0)badIdentity.profile="saved-game";
        if(scenario==1)badIdentity.players[1].slot=0;
        if(scenario==2)badIdentity.players[1].peer="host";
        if(scenario==3)badCapture.inputFrame++;
        if(scenario==4)badCapture.checkpoint++;
        if(scenario==5)badCapture.frand++;
        if(scenario==6)badCapture.inputRevision++;
        if(scenario==7)badCapture.paused=false;
        rejects([&]{recoveryProposal(badIdentity,badCapture,"host",std::string(32,'c'));});
    }
    auto live=original;uint64_t now=0;bool current=true;
    RecoveryBarrier barrier([&]{return live;},[&]{return current;},[&]{return now;},60000);
    rejects([&]{barrier.commit();});
    for(const auto* peer:{"g1","g2","g3"}){
        Json ack={{"type","session-recovery-ack"},{"version",1},{"peer",peer},{"accepted",original}};
        rejects([&]{barrier.acknowledge("host",ack);});
        auto tampered=ack;tampered["accepted"]["checkpoint"]=43;
        rejects([&]{barrier.acknowledge(peer,tampered);});
        barrier.acknowledge(peer,ack);
        barrier.acknowledge(peer,ack); // Duplicate acknowledgment adds no player.
        if(std::string(peer)!="g3")rejects([&]{barrier.commit();});
    }
    const auto committed=barrier.commit();
    require(committed["acknowledged"]==Json::array({"host","g1","g2","g3"}));
    require(committed["inputFrame"]=="7"&&committed["checkpoint"]==42);
    require(acceptRecoveryCommit(committed,original,identity,capture,"g1","host"));
    require(!acceptRecoveryCommit(committed,original,identity,capture,"stranger","host"));
    require(!acceptRecoveryCommit(committed,original,identity,capture,"g1","g2"));
    auto changed=capture;changed.frame++;
    require(!acceptRecoveryCommit(committed,original,identity,changed,"g1","host"));
    auto changedCommit=committed;changedCommit["acknowledged"].erase(3);
    require(!acceptRecoveryCommit(changedCommit,original,identity,capture,"g1","host"));
    rejects([&]{barrier.commit();});
    for(unsigned scenario=0;scenario<4;++scenario){
        live=original;now=0;current=true;
        RecoveryBarrier round([&]{return live;},[&]{return current;},[&]{return now;},60000);
        if(scenario==0)live["frame"]=11;
        if(scenario==1)current=false;
        if(scenario==2)now=60000;
        if(scenario==3)round.cancel();
        rejects([&]{round.commit();});
    }
    std::cout<<"PASS native all-player recovery barrier, bound identity, drift, deadline and one-use commit\n";
}
