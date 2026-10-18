#include "port/native_connection_route.hpp"
#include <chrono>
#include <thread>
#include <iostream>
#include <stdexcept>
#include <source_location>
using namespace partyboard::net;
void check(bool condition,std::source_location location=std::source_location::current()){if(!condition)throw std::runtime_error("Connection route assertion failed at line "+std::to_string(location.line()));}
template<class Predicate>void wait(Predicate predicate){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);while(!predicate()){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("RTC route test timed out");std::this_thread::sleep_for(std::chrono::milliseconds(10));}}
int main(){try{
 using State=rtc::PeerConnection::State;using Type=rtc::Candidate::Type;using Route=NativeConnectionRoute;
 check(classifyNativeConnectionRoute(State::Connected)==Route::Unknown);
 for(auto local:{Type::Host,Type::ServerReflexive,Type::PeerReflexive})for(auto remote:{Type::Host,Type::ServerReflexive,Type::PeerReflexive})check(classifyNativeConnectionRoute(State::Connected,true,local,remote)==Route::Direct);
 check(classifyNativeConnectionRoute(State::Connected,true,Type::Unknown,Type::Relayed)==Route::Relayed);
 check(classifyNativeConnectionRoute(State::Connected,true,Type::Relayed,Type::Host)==Route::Relayed);
 check(classifyNativeConnectionRoute(State::Connected,true,Type::Host,Type::Unknown)==Route::Unknown);
 check(classifyNativeConnectionRoute(State::Failed,true,Type::Host,Type::Host)==Route::Unreachable);
 check(classifyNativeConnectionRoute(State::Disconnected,true,Type::Host,Type::Host)==Route::Interrupted);
 rtc::PeerConnection host,guest;auto channel=host.createDataChannel("partyboard-route-test");
 check(nativeConnectionRoute(host)==Route::Connecting);
 wait([&]{return host.gatheringState()==rtc::PeerConnection::GatheringState::Complete&&host.localDescription().has_value();});
 guest.setRemoteDescription(*host.localDescription());
 wait([&]{return guest.gatheringState()==rtc::PeerConnection::GatheringState::Complete&&guest.localDescription().has_value();});
 host.setRemoteDescription(*guest.localDescription());
 wait([&]{return host.state()==State::Connected&&guest.state()==State::Connected&&channel->isOpen();});
 check(nativeConnectionRoute(host)==Route::Direct);check(nativeConnectionRoute(guest)==Route::Direct);
 host.close();guest.close();wait([&]{return host.state()==State::Closed&&guest.state()==State::Closed;});check(nativeConnectionRoute(host)==Route::Closed);
 std::cout<<"PASS selected-candidate direct/relay/unknown/loss classification and actual local RTC direct/close\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
