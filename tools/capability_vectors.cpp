#include "../include/port/controller_routes.hpp"
#include <cstdio>
using namespace partyboard::net;
int main() {
    for (bool ack : {false, true}) for (bool large : {false, true}) {
        const CapabilityOffer value{SessionId{9}, large ? UINT64_MAX : 7,
                                    large ? 0u : 0xa0000000u};
        const auto packet = ack ? encodeCapabilityAck(value) : encodeCapabilities(value);
        for (auto byte : packet) printf("%02x", unsigned(byte));
        puts("");
    }
    for (bool large : {false, true}) {
        const Input value{SessionId{9}, large ? UINT64_MAX : 0,
                          0x87654321u, uint8_t(large ? 3 : 0),
                          Pad{0, 1, 128, 127, 192, 63, 255, 129, 17, 99, 253}};
        const auto packet = encode(value);
        for (auto byte : packet) printf("%02x", unsigned(byte));
        puts("");
    }
}
