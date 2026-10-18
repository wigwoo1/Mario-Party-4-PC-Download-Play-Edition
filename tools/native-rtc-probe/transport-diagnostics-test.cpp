#include "port/native_transport_diagnostics.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <thread>
#include <array>
#include <iostream>
using namespace partyboard::net;
int main(){
    NativeTransportDiagnostics diagnostics;
    assert(diagnostics.snapshot().at("events").empty());
    std::array<std::thread,4> writers;
    for(auto& writer:writers)writer=std::thread([&]{
        for(unsigned i=0;i<1000;++i)diagnostics.record(std::string(100,'t'),std::string(1000,'d'));
    });
    for(auto& writer:writers)writer.join();
    const auto snapshot=diagnostics.snapshot();
    assert(snapshot.at("totalEvents")==4000&&snapshot.at("droppedEvents")==3968);
    const auto& events=snapshot.at("events");assert(events.size()==32);
    uint64_t sequence=3969;int64_t lastTime=0;
    for(const auto& event:events){
        assert(event.at("sequence")==sequence++);
        assert(event.at("elapsedMs").get<int64_t>()>=lastTime);lastTime=event.at("elapsedMs");
        assert(event.at("type").get<std::string>().size()==32);
        assert(event.at("detail").get<std::string>().size()==192);
    }
    diagnostics.record("owner-close");
    assert(snapshot.at("totalEvents")==4000); // Snapshots must own their data.
    const auto closed=diagnostics.snapshot();
    assert(closed.at("events").back().at("type")=="owner-close");
    assert(closed.at("totalEvents")==4001&&closed.at("droppedEvents")==3969);
    std::cout<<"PASS bounded concurrent transport history and independent snapshots\n";
}
