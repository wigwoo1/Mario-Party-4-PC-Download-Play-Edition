#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

namespace partyboard::trace {
using Header = std::array<unsigned char, 16>;
using Sample = std::array<unsigned char, 68>;
inline void put32(unsigned char* p, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(v >> (8 * i));
}
inline uint32_t get32(const unsigned char* p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= uint32_t(p[i]) << (8 * i);
    return v;
}
inline Header header(uint32_t seed) {
    Header h{'P','B','T','R','A','C','E',3};
    put32(h.data() + 8, seed);
    return h;
}
inline uint32_t seed(const Header& h) {
    auto expected = header(get32(h.data() + 8));
    if (h != expected) throw std::runtime_error("Invalid or unsupported input trace header");
    return get32(h.data() + 8);
}
inline void stamp(Sample& s, uint64_t index, uint32_t rng, uint32_t rng8, uint32_t rumble) {
    put32(s.data(), uint32_t(index));
    put32(s.data() + 4, uint32_t(index >> 32));
    put32(s.data() + 8, rng);
    put32(s.data() + 12, rng8);
    put32(s.data() + 16, rumble);
}
inline void check(const Sample& s, uint64_t index, uint32_t rng, uint32_t rng8) {
    const auto actual = uint64_t(get32(s.data())) | (uint64_t(get32(s.data() + 4)) << 32);
    if (actual != index) throw std::runtime_error("Input trace sample sequence mismatch");
    if (get32(s.data() + 8) != rng || get32(s.data() + 12) != rng8)
        throw std::runtime_error("Input replay RNG divergence");
}
}
