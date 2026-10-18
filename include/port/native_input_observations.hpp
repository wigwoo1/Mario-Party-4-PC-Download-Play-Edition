#pragma once
#include "input_trace.h"
#include "pad_exchange.hpp"
#include <array>
#include <span>
#include <stdexcept>

namespace partyboard::net {
struct NativeInputObservation {
    PartyBoardInputState state{};
    std::array<uint8_t,44> pads{};
};
// Fixed storage keeps the game-thread observer bounded and allocation-free.
// Preserve the first accepted polls for comparison with browser observations.
class NativeInputObservations {
    std::array<NativeInputObservation,512> entries_{};
    size_t count_=0;
    size_t next_=0;
    bool rolling_=false;
    uint64_t accepted_=0;
public:
    void useRollingDiagnostics(){
        if(accepted_)throw std::runtime_error("Rolling observations require a pre-frame diagnostic owner");
        rolling_=true;
    }
    void record(const PartyBoardInputState& state,const PADStatus* pads) noexcept {
        ++accepted_;
        if(!rolling_&&count_==256)return;
        const auto index=rolling_?next_:count_;
        auto& entry=entries_[index];entry.state=state;
        for(size_t slot=0;slot<4;++slot){
            const auto packed=packPad(pads[slot]);
            for(size_t byte=0;byte<packed.size();++byte)entry.pads[slot*11+byte]=packed[byte];
        }
        if(rolling_){entries_[index+256]=entry;next_=(next_+1)%256;}
        if(count_<256)++count_;
    }
    std::span<const NativeInputObservation> entries() const noexcept {return {entries_.data()+(rolling_&&count_==256?next_:0),count_};}
    uint64_t acceptedCount() const noexcept {return accepted_;}
};
}
