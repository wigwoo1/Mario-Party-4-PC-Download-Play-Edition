#include "../include/port/lockstep.hpp"
#include "../include/port/netplay_handshake.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

using namespace partyboard::net;
static SessionId session{1, 2, 3, 4};
static Input input(uint64_t frame, uint8_t slot, uint32_t hash) {
    Input i{session, frame, hash, slot, {}};
    for (unsigned j = 0; j < 11; ++j) i.pad[j] = uint8_t(frame + slot * 13 + j);
    return i;
}
static uint32_t step(uint32_t hash, const Frame& frame) {
    for (const auto& pad : frame.pads) for (auto byte : pad) hash = (hash ^ byte) * 16777619u;
    return hash;
}
int main() {
    Compatibility config;
    config.build[0] = 1; config.assets[0] = 2;
    config.save[0] = 3; config.settings[0] = 4; config.seed = 42;
    const auto hello = encodeHello(config);
    assert(decodeHello(hello) == config && hello[12] == 42 && hello[16] == 1);
    for (unsigned length = 0; length < hello.size(); ++length)
        assert(!decodeHello(std::span(hello).first(length)));
    auto longHello = std::vector<uint8_t>(hello.begin(), hello.end());
    longHello.push_back(0); assert(!decodeHello(longHello));
    for (auto offset : {0, 4, 5, 6, 7, 8}) {
        auto invalid = hello; invalid[offset] = 255; assert(!decodeHello(invalid));
    }
    for (unsigned field = 0; field < 4; ++field) {
        auto incomplete = config;
        std::array<Digest*, 4> digests{&incomplete.build, &incomplete.assets, &incomplete.save, &incomplete.settings};
        *digests[field] = {};
        assert(!hasFingerprints(incomplete));
        bool refused = false;
        try { ReadyGate bad(11, incomplete); } catch (const std::invalid_argument&) { refused = true; }
        assert(refused);
        refused = false;
        try { encodeHello(incomplete); } catch (const std::invalid_argument&) { refused = true; }
        assert(refused);
        auto invalid = hello;
        std::fill_n(invalid.begin() + 16 + field * 32, 32, uint8_t{0});
        assert(!decodeHello(invalid));
    }
    for (unsigned field = 0; field < 6; ++field) {
        auto other = config;
        switch (field) {
        case 0: ++other.protocol; break;
        case 1: ++other.build[31]; break;
        case 2: ++other.assets[31]; break;
        case 3: ++other.save[31]; break;
        case 4: ++other.settings[31]; break;
        case 5: ++other.seed; break;
        }
        assert(unsigned(compare(config, other)) == field + 1);
    }
    ReadyGate lobby(11, config);
    assert(!lobby.canStart() && !lobby.assign(22, 0, 22));
    assert(lobby.assign(11, 0, 11) && lobby.assign(11, 1, 22));
    auto revision = lobby.revision();
    assert(lobby.ready(11, revision, config, true));
    assert(!lobby.canStart() && !lobby.ready(33, revision, config, true));
    assert(lobby.ready(22, revision, config, true) && lobby.canStart());
    auto mismatch = config; ++mismatch.assets[0];
    assert(!lobby.ready(22, revision, mismatch, true) && !lobby.canStart());
    assert(lobby.assign(11, 2, 22));
    assert(!lobby.ready(22, revision, config, true)); // Stale acknowledgement.
    revision = lobby.revision();
    assert(lobby.ready(22, revision, config, true) && !lobby.canStart());
    assert(lobby.ready(11, revision, config, true) && lobby.canStart());
    assert(lobby.disconnected(22) && !lobby.canStart());
    assert(lobby.assign(11, 1, 22));
    ++config.seed;
    assert(lobby.configure(11, config));
    revision = lobby.revision();
    assert(lobby.ready(11, revision, config, true));
    assert(lobby.ready(22, revision, config, true));
    assert(!lobby.start(22, revision) && !lobby.start(11, revision - 1));
    assert(lobby.start(11, revision));
    assert(!lobby.disconnected(22) && !lobby.assign(11, 1, 33));
    assert(!lobby.start(11, revision));
    puts("Lobby readiness tests passed: compatibility, ownership, revisions and host-only start");
    const auto original = input(0x123456789abcdef0ULL, 3, 0xdeadbeef);
    const auto packet = encode(original);
    assert(packet.size() == 48 && packet[24] == 0xf0 && packet[31] == 0x12);
    assert(packet[32] == 0xef && packet[35] == 0xde);
    assert(decode(packet) == original);
    for (size_t length = 0; length < packet.size(); ++length)
        assert(!decode(std::span(packet).first(length)));
    auto extra = std::vector<uint8_t>(packet.begin(), packet.end()); extra.push_back(0);
    assert(!decode(extra));
    for (auto offset : {0, 4, 5, 6, 7, 47}) {
        auto bad = packet; bad[offset] = 255; assert(!decode(bad));
    }
    bool rejected = false;
    try { Lockstep bad({}, {1, 0, 0, 0}); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    Lockstep queue(session, {11, 22, 0, 0});
    assert(queue.missingSlots() == 3 && !queue.advance(7));
    assert(queue.receive(22, encode(input(0, 0, 7))) == Receive::Unauthorized);
    assert(queue.receive(0, encode(input(0, 2, 7))) == Receive::Unauthorized);
    auto wrongSession = input(0, 0, 7); wrongSession.session[0]++;
    assert(queue.receive(11, encode(wrongSession)) == Receive::WrongSession);
    assert(queue.receive(11, encode(input(UINT64_MAX, 0, 7))) == Receive::TooFar);
    assert(queue.receive(11, encode(input(120, 0, 7))) == Receive::TooFar);
    assert(queue.receive(11, encode(input(0, 0, 7))) == Receive::Accepted);
    assert(queue.receive(11, encode(input(0, 0, 7))) == Receive::Duplicate);
    assert(queue.missingSlots() == 2 && !queue.advance(7) && queue.nextFrame() == 0);
    assert(queue.receive(22, encode(input(0, 1, 7))) == Receive::Accepted);
    const auto ready = queue.advance(7);
    assert(ready && ready->index == 0 && ready->pads[2][10] == 255);
    assert(queue.receive(11, encode(input(0, 0, 7))) == Receive::Stale);
    assert(!queue.advance(7) && queue.nextFrame() == 1);
    queue.cancel();
    assert(queue.stopped() == Stop::Cancelled);
    assert(queue.receive(11, encode(input(1, 0, 7))) == Receive::Halted);

    Lockstep conflict(session, {11, 0, 0, 0});
    assert(conflict.receive(11, encode(input(0, 0, 7))) == Receive::Accepted);
    auto changed = input(0, 0, 7); changed.pad[0] ^= 1;
    assert(conflict.receive(11, encode(changed)) == Receive::Conflict);
    assert(!conflict.advance(7) && conflict.stopped() == Stop::Conflict);
    Lockstep divergence(session, {11, 22, 0, 0});
    assert(divergence.receive(11, encode(input(0, 0, 7))) == Receive::Accepted);
    assert(divergence.receive(22, encode(input(0, 1, 8))) == Receive::Divergence);
    assert(!divergence.advance(7));
    Lockstep localMismatch(session, {11, 0, 0, 0});
    assert(localMismatch.receive(11, encode(input(0, 0, 7))) == Receive::Accepted);
    assert(!localMismatch.advance(8) && localMismatch.stopped() == Stop::Divergence);

    // Two independent peers see different packet orders and duplicate delivery.
    // An oracle supplies future checkpoints for this synthetic test only; real
    // callers must compute checkpoints at the actual simulation boundary.
    Lockstep a(session, {11, 22, 33, 44}), b(session, {11, 22, 33, 44});
    uint32_t hashA = 42, hashB = 42, oracle = 42;
    std::mt19937 random(12345);
    for (uint64_t base = 0; base < 600; base += 30) {
        std::vector<Input> inputs;
        for (uint64_t f = base; f < base + 30; ++f) {
            Frame frame{f, oracle, {}};
            for (uint8_t slot = 0; slot < 4; ++slot) {
                inputs.push_back(input(f, slot, oracle));
                frame.pads[slot] = inputs.back().pad;
            }
            oracle = step(oracle, frame);
        }
        for (auto* peer : {&a, &b}) {
            std::shuffle(inputs.begin(), inputs.end(), random);
            for (const auto& i : inputs) {
                const auto p = encode(i);
                assert(peer->receive((i.slot + 1) * 11, p) == Receive::Accepted);
                assert(peer->receive((i.slot + 1) * 11, p) == Receive::Duplicate);
            }
        }
        for (unsigned j = 0; j < 30; ++j) {
            const auto fa = a.advance(hashA), fb = b.advance(hashB);
            assert(fa && fb && fa->index == base + j && fa->pads == fb->pads);
            hashA = step(hashA, *fa); hashB = step(hashB, *fb);
        }
        assert(hashA == oracle && hashB == oracle && a.missingSlots() == 15);
    }
    assert(a.nextFrame() == 600 && b.nextFrame() == 600);
    puts("Lockstep tests passed: 600 frames, four slots, reordered/duplicate packets, stalls and divergence");
}
