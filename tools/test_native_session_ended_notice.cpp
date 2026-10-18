#include "port/native_restart_intent.hpp"
#include <cassert>
#include <iostream>
using namespace partyboard::net;
int main(){
    assert(SetEnvironmentVariableW(L"PARTYBOARD_NATIVE_SESSION_ENDED",nullptr));
    assert(!consumeNativeSessionEndedNotice());
    publishNativeSessionEndedNotice(false);assert(!consumeNativeSessionEndedNotice());
    publishNativeSessionEndedNotice(true);assert(consumeNativeSessionEndedNotice());
    assert(!consumeNativeSessionEndedNotice());
    publishNativeSessionEndedNotice(true);publishNativeSessionEndedNotice(false);
    assert(!consumeNativeSessionEndedNotice());
    for(const auto invalid:{L"0",L"2",L"false",L"1extra"}){
        assert(SetEnvironmentVariableW(L"PARTYBOARD_NATIVE_SESSION_ENDED",invalid));
        bool rejected=false;try{consumeNativeSessionEndedNotice();}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected);assert(!consumeNativeSessionEndedNotice());
    }
    assert(std::string_view(NativeSessionEndedNotice).find("reconnect or synchronize")!=std::string_view::npos);
    std::cout<<"PASS native single-use session-ended notice: publication, clear, consume-once, malformed rejection and fixed text\n";
}
