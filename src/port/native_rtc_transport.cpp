#include "port/native_rtc_host_input.hpp"

namespace partyboard::net {
// Production runtime factory. The caller supplies accepted session membership;
// construction alone does not advertise readiness or authorize game execution.
std::unique_ptr<NativeRtcHostInput> createNativeRtcHostInput(SessionId session,
        uint8_t hostSlot,uint8_t activeSlots) {
    return std::make_unique<NativeRtcHostInput>(session,hostSlot,activeSlots);
}
}
