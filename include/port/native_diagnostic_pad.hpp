#pragma once
#include <dolphin/pad.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace partyboard::net {
// A held physical-lane sample for the opt-in diagnostic runner. Remote lanes
// remain selected by the original authenticated exchange ownership mapping.
class NativeDiagnosticPad {
    PADStatus held_{};
    static int value(const nlohmann::ordered_json& packet,const char* key,int low,int high){
        const auto& field=packet.at(key);
        if(!field.is_number_integer())throw std::invalid_argument("Integer diagnostic pad field required");
        const auto number=field.get<int64_t>();
        if(number<low||number>high)throw std::invalid_argument("Diagnostic pad field out of range");
        return int(number);
    }
public:
    void accept(const nlohmann::ordered_json& packet){
        if(packet.at("type")!="diagnostic-local-input")throw std::invalid_argument("Diagnostic local input command required");
        PADStatus next{};
        next.button=value(packet,"buttons",0,65535);
        next.stickX=value(packet,"stickX",-128,127);
        next.stickY=value(packet,"stickY",-128,127);
        next.substickX=value(packet,"substickX",-128,127);
        next.substickY=value(packet,"substickY",-128,127);
        next.triggerLeft=value(packet,"triggerLeft",0,255);
        next.triggerRight=value(packet,"triggerRight",0,255);
        held_=next;
    }
    void sample(PADStatus* pads)const{pads[0]=held_;}
};
}
