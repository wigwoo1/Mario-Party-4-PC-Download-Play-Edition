#include "port/native_prepared_start.hpp"
#include <cassert>
#include <iostream>
using namespace partyboard::net;
bool PartyBoard_IsRunning=false;
static int phase=2,seedAvailable=1,captureAvailable=1;static unsigned frame=0;static double prepared=123;
static PartyBoardInputState state{0,123,4,5};
static std::vector<int> startup;static int startupFailure=0;
extern "C" int PartyBoardNativeOnlineInputRequired(){return 0;}
extern "C" int PartyBoard_GameConfigureSessionSeed(unsigned seed){assert(phase==0&&seed==0);startup.push_back(1);return startupFailure==1?-1:0;}
extern "C" void PartyBoard_GameRequestStop(){startup.push_back(2);PartyBoard_IsRunning=false;}
extern "C" int PartyBoard_GameInitialize(){assert(!PartyBoard_IsRunning);startup.push_back(3);phase=2;return startupFailure==2?-1:0;}
extern "C" int PartyBoard_GameAlignStartClock(){assert(phase==2&&!PartyBoard_IsRunning);startup.push_back(4);return startupFailure==3?-1:0;}
extern "C" int PartyBoard_GameLifecyclePhase(){return phase;}
extern "C" unsigned PartyBoard_GameFrameCount(){return frame;}
extern "C" double PartyBoard_GamePreparedCheckpoint(){return prepared;}
extern "C" int PartyBoardFixedSeed(u32* seed){*seed=0;return seedAvailable;}
extern "C" int PartyBoardCaptureInputState(PartyBoardInputState* value){*value=state;return captureAvailable;}
static int installs=0,installAccepted=1,glyphAccepted=1;
static int detached=0;
extern "C" int PartyBoardNativeClearOwnedInputExchange(PartyBoardNativeInputExchange callback,void* context){assert(callback&&context&&!PartyBoard_IsRunning&&phase==2);detached++;return 1;}
extern "C" int PartyBoardNativeInstallInputExchange(PartyBoardNativeInputExchange callback,void* context){assert(callback&&context);installs++;return installAccepted;}
extern "C" int PartyBoardNativeSetOwnedGlyphLanes(PartyBoardNativeInputExchange callback,void* context,const s8 lanes[4]){assert(callback&&context&&!PartyBoard_IsRunning&&lanes&&lanes[0]==0&&lanes[1]==-1&&lanes[2]==-1&&lanes[3]==-1);return glyphAccepted;}
struct IO {uint64_t now(){return 0;}PartyBoardInputState capture(){return state;}bool pump(){return false;}bool send(const Packet&){return false;}void wait(){}};
int main(){const auto asset=std::string(64,'a'),build=std::string(64,'b');
auto configuration=nativeSessionConfiguration(std::string(32,'1'),0,asset,build);
for(unsigned mode=0;mode<6;mode++){
 phase=0;startup.clear();auto bad=configuration;
 switch(mode){case 0:bad["assetId"]=std::string(64,'c');break;
 case 1:bad["buildId"]=std::string(64,'c');break;
 case 2:bad["seed"]=4294967296ULL;break;case 3:bad["profile"]="other";break;
 case 4:bad["runtimeArtifacts"]={"../escape"};break;case 5:bad["unexpected"]=true;break;}
 bool rejected=false;try{initializeNativeConfiguredEngine(bad,asset,build);}catch(const std::exception&){rejected=true;}
 assert(rejected&&startup.empty()&&phase==0);
}
phase=0;startup.clear();auto configured=initializeNativeConfiguredEngine(configuration,asset,build);
assert(configured.assetId==asset&&configured.buildId==build&&configured.seed==0);
assert(startup==std::vector<int>({1,2,3,4}));
for(unsigned fail=0;fail<4;fail++){phase=0;PartyBoard_IsRunning=true;startup.clear();startupFailure=fail;bool rejected=false;try{auto initialized=initializeNativePreparedEngine(0,asset,build);assert(initialized.seed==0&&!PartyBoard_IsRunning&&phase==2&&frame==0);}catch(const std::runtime_error&){rejected=true;}assert(rejected==(fail!=0));assert(startup.front()==1);if(fail==1)assert(startup.size()==1);else if(fail==2)assert(startup==std::vector<int>({1,2,3}));else assert(startup==std::vector<int>({1,2,3,4}));}
startupFailure=0;phase=2;PartyBoard_IsRunning=false;startup.clear();bool freshRejected=false;try{initializeNativePreparedEngine(0,asset,build);}catch(const std::runtime_error&){freshRejected=true;}assert(freshRejected&&startup.empty());
auto runtime=captureNativePreparedStart(asset,build);assert(runtime.idle&&runtime.seed==0&&runtime.checkpoint==123&&runtime.preparedCheckpoint==123);
for(unsigned mode=0;mode<9;mode++){PartyBoard_IsRunning=false;phase=2;frame=0;prepared=123;seedAvailable=captureAvailable=1;state={0,123,4,5};
switch(mode){case 0:PartyBoard_IsRunning=true;break;case 1:phase=3;break;case 2:frame=1;break;case 3:prepared=-1;break;case 4:prepared=123.5;break;case 5:seedAvailable=0;break;case 6:captureAvailable=0;break;case 7:state.frame=1;break;case 8:state.checkpoint++;break;}
bool rejected=false;try{captureNativePreparedStart(asset,build);}catch(const std::runtime_error&){rejected=true;}assert(rejected);}
PartyBoard_IsRunning=false;phase=2;frame=0;prepared=123;seedAvailable=captureAvailable=1;state={0,123,4,5};
PreparedStartLobby lobby{asset,50000,0,0,{{"host",0,true,true,true,50000},{"guest1",1,true,true,true,50000}}};
SessionStartBarrier barrier(lobby,"01"+std::string(30,'0'),0,build,123);
SessionId session{};session[0]=1;RecoverablePadExchange queue(session,{1,2,0,0},1,{0,-1,-1,-1});IO io;NativeInputPollBinding<IO> binding(queue,io);
auto rejects=[&]{bool rejected=false;try{prepareNativeInputCommit(barrier,lobby,asset,build,binding);}catch(const std::runtime_error&){rejected=true;}assert(rejected);};
rejects();assert(installs==0);auto ack=barrier.proposal();ack["type"]="session-start-ack";assert(barrier.acknowledge("host",ack,lobby));rejects();assert(installs==0);
assert(barrier.acknowledge("guest1",ack,lobby));rejects();assert(installs==0);
RecoverySession identity{"01"+std::string(30,'0'),build,asset,"unlocked-no-save-v1",0,0,0,{{"host",0},{"guest1",1}}};assert(queue.bindOriginalSession(identity,"host","host"));
state.checkpoint++;rejects();assert(installs==0);state.checkpoint=123;
installAccepted=0;rejects();assert(installs==1);installAccepted=1;
glyphAccepted=0;const auto rollbackBefore=detached;rejects();assert(installs==2&&detached==rollbackBefore+1&&!PartyBoard_IsRunning&&queue.nextFrame()==0);glyphAccepted=1;
auto commit=prepareNativeInputCommit(barrier,lobby,asset,build,binding);assert(commit["type"]=="session-start-commit"&&installs==3&&!PartyBoard_IsRunning&&queue.nextFrame()==0);
for(bool accepted:{false,true}){
 RecoverablePadExchange other(session,{1,2,0,0},1,{0,-1,-1,-1});assert(other.bindOriginalSession(identity,"host","host"));
 NativeInputPollBinding<IO> owned(other,io);
 auto config=nativeSessionConfiguration(identity.session,0,asset,build);
 auto captured=captureNativePreparedStart(asset,build);
 auto receipt=nativePreparedRuntimeReport(captured,config,true);
 NativeStartCoordinator coordinator(lobby,config);
 assert(coordinator.receivePrepared("host",receipt.dump())&&coordinator.receivePrepared("guest1",receipt.dump()));
 auto proposal=coordinator.begin(captured,receipt);proposal["type"]="session-start-ack";
 assert(coordinator.acknowledge("host",proposal.dump())&&coordinator.acknowledge("guest1",proposal.dump()));
 const auto prior=detached;
 assert(deliverNativeCoordinatedInputCommit(coordinator,asset,build,"host",owned,
  [&](const std::string& peer,const std::string& text){assert(peer=="guest1"&&nlohmann::ordered_json::parse(text)["type"]=="session-start-commit");return accepted;})==accepted);
 assert(detached==prior+(accepted?0:1)&&!PartyBoard_IsRunning&&other.nextFrame()==0);
 if(accepted)assert(owned.detachStopped());
}
std::cout<<"PASS native prepared capture and agreed pinned-session input install; synthetic game API providers\n";}
