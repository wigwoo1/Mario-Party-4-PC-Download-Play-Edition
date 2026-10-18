#include "../include/port/trace_codec.hpp"
#include <cassert>
#include <cstdio>
#include <functional>

static void rejects(const std::function<void()>& action) {
    bool threw = false;
    try { action(); } catch (const std::runtime_error&) { threw = true; }
    assert(threw);
}
int main() {
    using namespace partyboard::trace;
    for (uint32_t value : {0u, 1u, 0x80000000u, 0xffffffffu}) {
        auto h = header(value);
        assert(seed(h) == value);
        h[7] = 2; // Previous checksum schema must not be silently accepted.
        rejects([&] { seed(h); });
        h = header(value); h[15] = 1;
        rejects([&] { seed(h); });
    }
    Sample s{};
    stamp(s, 0x123456789abcdef0ull, 123, 456, 0x80000000);
    assert(s[0] == 0xf0 && s[7] == 0x12); // Explicit little-endian format.
    assert(get32(s.data() + 16) == 0x80000000);
    check(s, 0x123456789abcdef0ull, 123, 456);
    rejects([&] { check(s, 3, 123, 456); });
    rejects([&] { check(s, 0x123456789abcdef0ull, 124, 456); });
    rejects([&] { check(s, 0x123456789abcdef0ull, 123, 457); });
    puts("Trace codec tests passed");
}
