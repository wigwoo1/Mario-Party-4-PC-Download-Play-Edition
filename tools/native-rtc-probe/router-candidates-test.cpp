#include "port/native_router_candidates.hpp"
#include <chrono>
#include <thread>
#include <source_location>
#include <iostream>
using namespace partyboard::net;
void check(bool ok,std::source_location location=std::source_location::current()){
    if(!ok)throw std::runtime_error("router candidate assertion at line "+std::to_string(location.line()));
}
int main(){try{
    rtc::PeerConnection connection;auto channel=connection.createDataChannel("partyboard-router-port-test");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(connection.gatheringState()!=rtc::PeerConnection::GatheringState::Complete||!connection.localDescription()){
        if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Actual ICE gathering timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const auto description=*connection.localDescription();unsigned matched=0;
    for(auto candidate:description.candidates()){
        if(candidate.type()!=rtc::Candidate::Type::Host||candidate.transportType()!=rtc::Candidate::TransportType::Udp||!candidate.resolve(rtc::Candidate::ResolveMode::Simple)||candidate.family()!=rtc::Candidate::Family::Ipv4)continue;
        const auto requests=nativeRouterPortRequests(description,*candidate.address());
        check(!requests.empty());bool found=false;
        for(const auto& request:requests){check(request.internalAddress==candidate.address());check(request.externalPort==request.internalPort);found|=request.internalPort==candidate.port();}
        check(found);++matched;
    }
    check(matched>0);check(nativeRouterPortRequests(description,"192.0.2.251").empty());
    auto fixture=description;fixture.extractCandidates();
    for(const char* candidate:{"candidate:lan 1 UDP 2122260223 192.168.1.23 45678 typ host","candidate:duplicate 1 UDP 2122260223 192.168.1.23 45678 typ host","candidate:other 1 UDP 2122260223 192.168.2.23 45679 typ host","candidate:tcp 1 TCP 2122260223 192.168.1.23 45680 typ host tcptype passive","candidate:srflx 1 UDP 1694498815 192.168.1.23 45681 typ srflx raddr 192.168.1.23 rport 45678","candidate:relay 1 UDP 16777215 192.168.1.23 45682 typ relay raddr 192.168.1.23 rport 45678","candidate:ipv6 1 UDP 2122260223 ::1 45683 typ host"})fixture.addCandidate(rtc::Candidate(candidate,description.bundleMid()));
    const auto requests=nativeRouterPortRequests(fixture,"192.168.1.23");
    check(requests.size()==1&&requests[0].internalPort==45678&&requests[0].externalPort==45678);
    connection.close();
    std::cout<<"PASS actual gathered ICE IPv4 UDP ports selected without allocation; interface, transport, type and duplicate filters; gathered host candidates="<<matched<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
