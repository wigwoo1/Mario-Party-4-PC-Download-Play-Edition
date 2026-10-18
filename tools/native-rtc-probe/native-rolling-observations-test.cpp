#include <port/native_input_observations.hpp>
#include <cassert>
#include <iostream>
int main(){
    partyboard::net::NativeInputObservations first,rolling;rolling.useRollingDiagnostics();PADStatus pads[4]{};
    for(unsigned frame=0;frame<1000;++frame){
        PartyBoardInputState state{frame,frame+100,4,5};pads[0].button=frame;
        first.record(state,pads);rolling.record(state,pads);
        const auto entries=rolling.entries();assert(entries.size()==(frame<256?frame+1:256));
        for(size_t index=0;index<entries.size();++index){
            const auto expected=frame+1-entries.size()+index;
            assert(entries[index].state.frame==expected&&entries[index].state.checkpoint==expected+100);
            assert(entries[index].pads[0]==(expected&255)&&entries[index].pads[1]==(expected>>8));
        }
    }
    assert(first.acceptedCount()==1000&&first.entries().size()==256&&first.entries().front().state.frame==0&&first.entries().back().state.frame==255);
    assert(rolling.acceptedCount()==1000&&rolling.entries().front().state.frame==744&&rolling.entries().back().state.frame==999);
    bool rejected=false;try{rolling.useRollingDiagnostics();}catch(const std::runtime_error&){rejected=true;}assert(rejected);
    std::cout<<"PASS rolling native observations: chronological wrap, fresh pads/checkpoints, legacy first polls and late configuration rejection\n";
}
