#include <port/native_engine_input_io.hpp>
#include <port/native_input_timing_json.hpp>
#include <cassert>
#include <iostream>
using namespace partyboard::net;
extern "C" int PartyBoardCaptureInputState(PartyBoardInputState* state){*state={7,123,4,5};return 1;}
extern "C" int PartyBoard_GameLifecyclePhase(){return 2;}
extern "C" unsigned PartyBoard_GameFrameCount(){return 0;}
int main(){
    double time=0;unsigned reads=0,pumps=0,sends=0,waits=0,accepted=0;
    auto clock=[&]{++reads;return time;};
    NativeEngineInputIO io([&]{++pumps;time+=3;return false;},
        [&](const auto&){++sends;time+=0.5;return true;},
        [&]{++waits;time+=16;},[&](const auto&,const auto*){++accepted;},NativeInputTimer(true,clock));
    PartyBoardInputState state{7,123,4,5};PADStatus pads[4]{};Packet packet{};
    io.prepareLocalPads(state,pads);assert(pads[0].button==0);
    assert(!io.pump());assert(io.send(packet));io.wait();
    io.accepted(state,pads);io.accepted(state,pads);
    const auto sample=io.timing();
    assert(sample.enabled&&sample.startedPolls==1&&sample.completedPolls==1);
    assert(sample.pump.calls==1&&sample.pump.totalMs==3&&sample.pump.buckets[1]==1);
    assert(sample.send.calls==1&&sample.send.totalMs==0.5&&sample.send.buckets[0]==1);
    assert(sample.wait.calls==1&&sample.wait.totalMs==16&&sample.wait.maxMs==16&&sample.wait.buckets[2]==1);
    assert(sample.poll.calls==1&&sample.poll.totalMs==19.5&&sample.poll.buckets[2]==1);
    assert(pumps==1&&sends==1&&waits==1&&accepted==2&&reads==8);
    auto json=nativeInputTimingJson(sample);assert(json["wait"]["totalMs"]==16);
    assert(json["bucketUpperBoundsMs"][3].is_null());
    // Disabled diagnostics never read even an explicitly supplied clock.
    reads=0;
    NativeEngineInputIO disabled([]{return true;},[](const auto&){return false;},[]{},{},NativeInputTimer(false,clock));
    disabled.prepareLocalPads(state,pads);assert(disabled.pump());assert(!disabled.send(packet));disabled.wait();disabled.accepted(state,pads);
    assert(reads==0&&!disabled.timing().enabled&&disabled.timing().wait.calls==0);
    // A throwing callback retains its original exception, with scope accounted.
    NativeEngineInputIO throwing([&]()->bool{time+=20;throw std::runtime_error("original");},[](const auto&){return true;},[]{},{},NativeInputTimer(true,clock));
    bool caught=false;try{throwing.pump();}catch(const std::runtime_error& error){caught=std::string(error.what())=="original";}
    assert(caught&&throwing.timing().pump.totalMs==20&&throwing.timing().pump.buckets[3]==1);
    // Boundary buckets and a pending unaccepted poll stay diagnostic only.
    NativeInputTimer timer(true,clock);
    for(const double delay:{1.0,2.0,8.0,20.0}){auto span=timer.wait();time+=delay;}
    assert((timer.snapshot().wait.buckets==std::array<uint64_t,4>{1,1,1,1}));
    timer.beginPoll();assert(timer.snapshot().startedPolls==1&&timer.snapshot().completedPolls==0);
    assert(state.frame==7&&state.checkpoint==123&&state.frand==4&&state.rand8==5);
    std::cout<<"PASS native input timing: optional clocks, callback returns/exceptions, exact spans/buckets, poll completion and JSON; gameplay state unchanged\n";
}
