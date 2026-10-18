from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1];work=root.parents[1]/'work'
source=(root/'src/port/net_diagnostic.cpp').read_text().split('#ifdef _WIN32')[0]
fixture=work/'native-input-hook-test.cpp'
poll_fixture=(root/'tools/native-rtc-probe/native-input-poll-test.cpp').read_text()
poll_io=poll_fixture.split('struct IO',1)[1].split('int main()',1)[0]
fixture.write_text(source+'\n#include "port/native_prepared_start.hpp"\nusing namespace partyboard::net;\nstruct IO'+poll_io+'''
#include <cassert>
#include <iostream>
bool PartyBoard_IsRunning=false;static double prepared=-1;static int accepted=0,calls=0,drift=0;static PartyBoardInputState actual={0,123,4,5};
extern "C" double PartyBoard_GamePreparedCheckpoint(){return prepared;}
extern "C" int PartyBoardCaptureInputState(PartyBoardInputState*s){*s=actual;return 1;}
extern "C" int PartyBoard_GameLifecyclePhase(){return PartyBoard_IsRunning?3:2;}
extern "C" unsigned PartyBoard_GameFrameCount(){return actual.frame;}
extern "C" int PartyBoardFixedSeed(u32* seed){*seed=0;return 1;}
int exchange(void*context,PADStatus pads[4],const PartyBoardInputState*state){assert(context==&calls&&state->checkpoint==123&&state->frand==4&&state->rand8==5);++calls;pads[0].button=99;if(drift==1)actual.frame++;if(drift==2)actual.checkpoint++;if(drift==3)actual.frand++;if(drift==4)actual.rand8++;return accepted;}
int main(){verifyAdvancingPumpClock();for(int port=0;port<4;port++)assert(PartyBoardNativeGlyphLane(port)==port);assert(PartyBoardNativeGlyphLane(-1)==-1&&PartyBoardNativeGlyphLane(4)==-1);PADStatus pads[4]{};pads[0].button=7;assert(!PartyBoardNativeInstallInputExchange(exchange,&calls));prepared=123;assert(PartyBoardNativeInstallInputExchange(exchange,&calls));assert(!PartyBoardNativeInstallInputExchange(exchange,&calls));assert(PartyBoardNativeOnlineInputRequired());for(int port=0;port<4;port++)assert(PartyBoardNativeGlyphLane(port)==-1);
s8 lanes[4]={-1,-1,0,-1};assert(!PartyBoardNativeSetOwnedGlyphLanes(exchange,&accepted,lanes));assert(PartyBoardNativeSetOwnedGlyphLanes(exchange,&calls,lanes));assert(PartyBoardNativeGlyphLane(2)==0&&PartyBoardNativeGlyphLane(0)==-1);
for(const auto& invalid: {std::array<s8,4>{0,0,-1,-1},std::array<s8,4>{-1,-1,-1,-1},std::array<s8,4>{4,-1,-1,-1},std::array<s8,4>{-2,-1,-1,-1}}){assert(!PartyBoardNativeSetOwnedGlyphLanes(exchange,&calls,invalid.data()));assert(PartyBoardNativeGlyphLane(2)==0);}

PartyBoard_IsRunning=true;assert(!PartyBoardNativeSetOwnedGlyphLanes(exchange,&calls,lanes));assert(!PartyBoardNativeClearInputExchange());assert(!sampleNativeInput(pads)&&pads[0].button==7&&calls==1);accepted=1;assert(sampleNativeInput(pads)&&pads[0].button==99&&calls==2);for(drift=1;drift<=4;++drift){actual={0,123,4,5};pads[0].button=7;assert(!sampleNativeInput(pads)&&pads[0].button==7);}actual={0,123,4,5};drift=0;PartyBoard_IsRunning=false;assert(PartyBoardNativeClearInputExchange()&&!PartyBoardNativeOnlineInputRequired());
PartyBoard_IsRunning=true;assert(!PartyBoardNativeInstallInputExchange(exchange,&calls));PartyBoard_IsRunning=false;
for(unsigned mode=0;mode<4;mode++){
SessionId session{};session[0]=1;RecoverablePadExchange q(session,{1,2,0,0},1,{0,-1,-1,-1});
RecoverySession identity{"01"+std::string(30,'0'),std::string(64,'b'),std::string(64,'a'),"unlocked-no-save-v1",0,0,0,{{"host",0},{"guest1",1}}};assert(q.bindOriginalSession(identity,"host","host"));
IO io{q,session,mode};NativeInputPollBinding<IO> binding(q,io);actual={0,123,4,5};pads[0].button=7;pads[1].button=0;
PreparedStartLobby lobby{identity.assetId,50000,0,0,{{"host",0,true,true,true,50000},{"guest1",1,true,true,true,50000}}};
SessionStartBarrier barrier(lobby,identity.session,0,identity.buildId,123);
auto rejectStart=[&]{bool rejected=false;try{prepareNativeInputCommit(barrier,lobby,identity.assetId,identity.buildId,binding);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&!PartyBoardNativeOnlineInputRequired()&&q.nextFrame()==0&&io.step==0);};
rejectStart();auto ack=barrier.proposal();ack["type"]="session-start-ack";assert(barrier.acknowledge("host",ack,lobby));rejectStart();assert(barrier.acknowledge("guest1",ack,lobby));
actual.checkpoint++;rejectStart();actual.checkpoint=123;
for(unsigned failure=0;failure<2;failure++){
SessionStartBarrier delivery(lobby,identity.session,0,identity.buildId,123);assert(delivery.acknowledge("host",ack,lobby)&&delivery.acknowledge("guest1",ack,lobby));
assert(!deliverNativeInputCommit(delivery,lobby,identity.assetId,identity.buildId,binding,[&](const auto& value){assert(value["type"]=="session-start-commit"&&PartyBoardNativeOnlineInputRequired()&&!PartyBoard_IsRunning);if(failure)throw std::runtime_error("Transport send failed");return false;}));
assert(delivery.state()==SessionStartBarrier::State::Cancelled&&!PartyBoardNativeOnlineInputRequired()&&q.nextFrame()==0&&io.step==0);
}
nlohmann::ordered_json commit;unsigned sends=0;
assert(deliverNativeInputCommit(barrier,lobby,identity.assetId,identity.buildId,binding,[&](const auto& value){assert(PartyBoardNativeOnlineInputRequired()&&!PartyBoard_IsRunning&&q.nextFrame()==0&&io.step==0);commit=value;sends++;return true;}));assert(sends==1);assert(PartyBoardNativeGlyphLane(0)==0);for(int port=1;port<4;port++)assert(PartyBoardNativeGlyphLane(port)==-1);
assert(commit["type"]=="session-start-commit"&&PartyBoardNativeOnlineInputRequired()&&!PartyBoard_IsRunning&&q.nextFrame()==0&&io.step==0);
NativeInputPollBinding<IO> stranger(q,io);assert(!stranger.detachStopped()&&PartyBoardNativeOnlineInputRequired());
bool duplicateRejected=false;try{prepareNativeInputCommit(barrier,lobby,identity.assetId,identity.buildId,binding);}catch(const std::runtime_error&){duplicateRejected=true;}assert(duplicateRejected);
PartyBoard_IsRunning=true;
assert(!binding.detachStopped()&&PartyBoardNativeOnlineInputRequired());
assert(!PartyBoardNativeClearInputExchange());bool ok=sampleNativeInput(pads);
if(mode<2){assert(ok&&pads[1].button==42&&q.nextFrame()==1);assert(io.sends==(mode==1?2:1));}
else assert(!ok&&pads[0].button==7&&pads[1].button==0&&q.nextFrame()==0);
PartyBoard_IsRunning=false;assert(binding.detachStopped()&&!PartyBoardNativeOnlineInputRequired());assert(!binding.detachStopped());for(int port=0;port<4;port++)assert(PartyBoardNativeGlyphLane(port)==port);
}
for(unsigned owned=0;owned<4;owned++){
SessionId session{};session[0]=1;std::array<s8,4> lanes{-1,-1,-1,-1};lanes[owned]=0;
RecoverablePadExchange q(session,{1,2,3,4},owned+1,lanes);
RecoverySession identity{"01"+std::string(30,'0'),std::string(64,'b'),std::string(64,'a'),"unlocked-no-save-v1",0,0,0,{{"host",0},{"guest1",1},{"guest2",2},{"guest3",3}}};
const auto local=owned?"guest"+std::to_string(owned):std::string("host");assert(q.bindOriginalSession(identity,local,"host"));IO io{q,session,0};NativeInputPollBinding<IO> binding(q,io);
assert(binding.installOriginalSession(identity));for(unsigned slot=0;slot<4;slot++)assert(PartyBoardNativeGlyphLane(slot)==(slot==owned?0:-1));
NativeInputPollBinding<IO> stranger(q,io);assert(!stranger.detachStopped());assert(PartyBoardNativeGlyphLane(owned)==0);
assert(binding.detachStopped());for(int slot=0;slot<4;slot++)assert(PartyBoardNativeGlyphLane(slot)==slot);
}
std::cout<<"PASS extracted native hook and retained callback queue: installation, staged failure, recovery, deadline, drift, stopped cleanup\\n";}
''')
subprocess.run(['cl','/nologo','/std:c++20','/EHsc','/DTARGET_PC','/I'+str(root/'include'),'/I'+str(root/'extern/aurora/include'),'/I'+str(work/'b/_deps/json-src/include'),str(fixture),'/Fe'+str(work/'native-input-hook-test.exe'),'/Fo'+str(work/'native-input-hook-test.obj')],check=True)
subprocess.run([str(work/'native-input-hook-test.exe')],check=True)

