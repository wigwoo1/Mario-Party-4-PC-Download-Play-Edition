#include <port/native_engine_input_io.hpp>
#include <cassert>
#include <iostream>
static int phase=2;
static unsigned frame=0;
extern "C" int PartyBoard_GameLifecyclePhase(){return phase;}
extern "C" unsigned PartyBoard_GameFrameCount(){return frame;}
int main(){
    partyboard::net::NativeEngineInputIO io([]{return true;},[](const auto&){return true;},[]{});
    PartyBoardInputState state{7,123,4,5}; PADStatus pads[4]{};
    io.prepareLocalPads(state,pads);assert(pads[0].button==0);
    io.setDiagnosticLocalInput([](const auto& captured,auto* local){assert(captured.frame==7&&captured.checkpoint==123);local[0].button=256;local[0].stickX=-64;});
    io.prepareLocalPads(state,pads);assert(pads[0].button==256&&pads[0].stickX==-64&&pads[1].button==0);
    for(int condition=0;condition<2;++condition){
        phase=condition?2:3;frame=condition?1:0;bool rejected=false;
        try{io.setDiagnosticLocalInput({});}catch(const std::runtime_error&){rejected=true;}
        assert(rejected);
    }
    io.prepareLocalPads(state,pads);assert(pads[0].button==256);
    std::cout<<"PASS diagnostic local input: opt-in, preserved captured state, stopped pre-frame configuration\n";
}
