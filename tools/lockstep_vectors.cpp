#include "../include/port/lockstep.hpp"
#include <cstdio>
using namespace partyboard::net;
int main() {
    const SessionId session{9};
    const std::array<uint32_t, 4> owners{11, 22, 11, 22};
    Lockstep queue(session, owners);
    uint32_t hash = 42;
    for (uint64_t frame = 0; frame < 600; ++frame) {
        for (uint8_t slot : std::array<uint8_t, 4>{3, 1, 0, 2}) {
            Pad pad{};
            for (unsigned i = 0; i < 11; ++i) pad[i] = uint8_t(frame + slot * 13 + i);
            const auto packet = encode(Input{session, frame, hash, slot, pad});
            if (queue.receive(owners[slot], packet) != Receive::Accepted ||
                queue.receive(owners[slot], packet) != Receive::Duplicate) return 2;
        }
        const auto result = queue.advance(hash);
        if (!result) return 2;
        for (const auto& pad : result->pads) for (auto byte : pad) hash = (hash ^ byte) * 16777619u;
        printf("%llu %u\n", (unsigned long long)result->index, hash);
    }
    const auto emit = [](Receive result) { printf("RECEIVE %u\n", unsigned(result)); };
    Lockstep negative(session, {11, 22, 0, 0});
    auto packet = encode(Input{session, 0, 42, 0, {}});
    emit(negative.receive(22, packet));
    auto bad = packet; bad[8] = 8; emit(negative.receive(11, bad));
    emit(negative.receive(11, std::span(packet).first(47)));
    emit(negative.receive(11, encode(Input{session, 120, 42, 0, {}})));
    emit(negative.receive(11, packet)); emit(negative.receive(11, packet));
    bad = packet; bad[36] = 1; emit(negative.receive(11, bad));
    emit(negative.receive(11, packet));
    Lockstep diverged(session, {11, 22, 0, 0});
    emit(diverged.receive(11, packet));
    emit(diverged.receive(22, encode(Input{session, 0, 43, 1, {}})));
    Lockstep stale(session, {11, 0, 0, 0});
    stale.receive(11, packet); stale.advance(42); emit(stale.receive(11, packet));
    Lockstep localMismatch(session, {11, 0, 0, 0});
    localMismatch.receive(11, packet);
    const auto mismatchedFrame = localMismatch.advance(43);
    printf("LOCAL %u %u\n", unsigned(bool(mismatchedFrame)), unsigned(localMismatch.stopped()));
}
