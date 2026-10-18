// Link with the actual entry.cpp. Fixture port_main proves process/argument
// handoff, not game shutdown or a production online session.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>
#include <string>
#include <cstdlib>
#include "port/native_restart_intent.hpp"
static bool restart=false;
static unsigned calls=0;
extern "C" int aurora_main(int,char**);
int main(int argc,char** argv){return aurora_main(argc,argv);}
extern "C" int PartyBoardNativeRestartPending(){return restart?1:0;}
extern "C" int PartyBoardNativeRestartOnlineAction(){const auto action=std::getenv("PARTYBOARD_RESTART_TEST_ACTION");return action&&std::string(action)=="join"?1:0;}
extern "C" int port_main(int argc,char** argv){
    if(++calls!=1||argc!=3||std::string(argv[2])!="argument with spaces and \"quotes\"")return 3;
    const auto stage=std::getenv("PARTYBOARD_RESTART_TEST_STAGE");
    const auto noticeSetting=std::getenv("PARTYBOARD_RESTART_TEST_NOTICE");
    const bool wantsNotice=noticeSetting&&std::string(noticeSetting)=="1";
    if(stage&&std::string(stage)=="child"){
        const auto intent=partyboard::net::consumeNativeRestartIntent();
        if(!intent||*intent!=PartyBoardNativeRestartOnlineAction()||partyboard::net::consumeNativeRestartIntent())return 6;
        if(partyboard::net::consumeNativeSessionEndedNotice()!=wantsNotice||partyboard::net::consumeNativeSessionEndedNotice())return 7;
        std::ofstream report(argv[1]);
        report<<"fresh-child "<<GetCurrentProcessId()<<" arguments-preserved "<<(*intent==0?"host":"join")<<"-intent-consumed-once"<<(wantsNotice?" notice-consumed-once":"")<<"\n";
        return report?0:4;
    }
    if(!SetEnvironmentVariableW(L"PARTYBOARD_RESTART_TEST_STAGE",L"child"))return 5;
    partyboard::net::publishNativeSessionEndedNotice(wantsNotice);
    std::ofstream parent(std::string(argv[1])+".parent");parent<<GetCurrentProcessId();
    restart=true;return 0;
}
