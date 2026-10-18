#pragma once
#include <array>
#include <atomic>
#include <cstddef>

namespace aurora::input {
// SDL event watchers can run on other threads. Each press survives until one
// input sample, even when key-up was already processed in the same event pump.
template<std::size_t Count> class KeyLatch {
  std::array<std::atomic<bool>, Count> pressed{};
public:
  void press(std::size_t key) { if (key < Count) pressed[key].store(true); }
  void clear() { for (auto& key : pressed) key.store(false); }
  bool sample(std::size_t key, bool held) {
    if (key >= Count) return false;
    const bool tap = pressed[key].exchange(false);
    return held || tap;
  }
};
}
