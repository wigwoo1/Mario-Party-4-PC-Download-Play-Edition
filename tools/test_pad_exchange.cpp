#include "../include/port/pad_exchange.hpp"
#include <cassert>
#include <cstdio>
#include <string_view>
using namespace partyboard::net;
int main() {
    SessionId session{1};
    PADStatus local[4]{};
    local[0].button = 0x1234; local[0].stickX = -128; local[0].stickY = 127;
    local[0].substickX = -64; local[0].substickY = 63;
    local[0].triggerLeft = 255; local[0].triggerRight = 129;
    local[0].analogA = 17; local[0].analogB = 99; local[0].err = -3;
    local[0].extButton = 123;
    const auto packed = packPad(local[0]);
    const auto restored = unpackPad(packed);
    assert(packPad(restored) == packed && restored.stickX == -128 && restored.err == -3);
    assert(restored.extButton == 0);
    PadExchange exchange(session, {11, 22, 0, 0}, 11);
    PADStatus destination[4]{};
    for (auto& pad : destination) { pad.button = 999; pad.extButton = 555; }
    assert(exchange.begin(local, 42));
    const auto sent = exchange.outgoing()[0];
    local[0].button = 0; // Subsequent device changes must not alter an in-flight sample.
    assert(!exchange.begin(local, 42) && exchange.outgoing()[0] == sent);
    for (unsigned i = 0; i < 100; ++i) {
        assert(exchange.poll(42, destination) == Exchange::Waiting);
        assert(destination[0].button == 999 && destination[0].extButton == 555);
        assert(exchange.nextFrame() == 0);
    }
    assert(exchange.receive(11, sent) == Receive::Unauthorized);
    Input remote{session, 0, 42, 1, {}}; remote.pad[0] = 7;
    assert(exchange.receive(22, encode(remote)) == Receive::Accepted);
    assert(exchange.poll(42, destination) == Exchange::Ready);
    assert(packPad(destination[0]) == packed && destination[1].button == 7);
    assert(destination[2].err == PAD_ERR_NO_CONTROLLER && destination[0].extButton == 0);
    assert(exchange.nextFrame() == 1 && exchange.outgoing().empty());
    assert(exchange.poll(42, destination) == Exchange::Stopped); // No double commit.
    assert(exchange.begin(local, 43));
    assert(exchange.poll(44, destination) == Exchange::Stopped); // Simulation advanced during wait.
    assert(packPad(destination[0]) == packed);
    assert(!exchange.begin(local, 43));
    assert(std::string_view(exchange.firstStopSite()) == "poll-checkpoint-drift");
    PadExchange prequeued(session, {11, 22, 0, 0}, 11);
    assert(prequeued.receive(22, encode(Input{session, 0, 41, 1, {}})) == Receive::Accepted);
    assert(!prequeued.begin(local, 42));
    assert(prequeued.firstStopState() == Stop::Divergence);
    assert(std::string_view(prequeued.firstStopSite()) == "begin-local-packet");
    assert(prequeued.receive(22, encode(remote)) == Receive::Halted);
    assert(std::string_view(prequeued.firstStopSite()) == "begin-local-packet");
    PadExchange cancelled(session, {11, 22, 0, 0}, 11);
    assert(cancelled.begin(local, 42)); cancelled.cancel();
    assert(cancelled.poll(42, destination) == Exchange::Stopped);
    puts("Pad exchange tests passed: freeze, wait without mutation, commit once, inactive pads and cancel");
}
