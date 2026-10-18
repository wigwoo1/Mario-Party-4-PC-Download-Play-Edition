#include <port/native_diagnostic_pad.hpp>
#include <cassert>
#include <iostream>
int main(){
    partyboard::net::NativeDiagnosticPad input;PADStatus pads[4]{};pads[1].button=42;
    nlohmann::ordered_json command={{"type","diagnostic-local-input"},{"buttons",256},{"stickX",-128},{"stickY",127},{"substickX",0},{"substickY",0},{"triggerLeft",0},{"triggerRight",255}};
    input.accept(command);input.sample(pads);
    assert(pads[0].button==256&&pads[0].stickX==-128&&pads[0].stickY==127&&pads[0].triggerRight==255&&pads[1].button==42);
    for(const auto& invalid:{nlohmann::ordered_json(128),nlohmann::ordered_json(-129),nlohmann::ordered_json(0.5),nlohmann::ordered_json("0")}){
        auto bad=command;bad["stickX"]=invalid;bool rejected=false;
        try{input.accept(bad);}catch(const std::exception&){rejected=true;}
        assert(rejected);input.sample(pads);assert(pads[0].stickX==-128&&pads[0].button==256);
    }
    command["buttons"]=0;command["stickX"]=0;input.accept(command);input.sample(pads);assert(pads[0].button==0&&pads[0].stickX==0);
    std::cout<<"PASS native diagnostic pad: bounds, atomic rejection, held input/release, untouched other lanes\n";
}
