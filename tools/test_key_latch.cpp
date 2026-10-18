#include "../extern/aurora/lib/key_latch.hpp"
#include <cassert>
int main() {
  aurora::input::KeyLatch<4> latch;
  latch.press(1);
  assert(latch.sample(1, false)); // down+up between polls still produces one tap
  assert(!latch.sample(1, false));
  latch.press(1);
  assert(latch.sample(1, true));
  assert(latch.sample(1, true)); // holding remains active
  assert(!latch.sample(1, false)); // releasing does not repeat the old tap
  latch.press(2); latch.clear();
  assert(!latch.sample(2, false)); // lost focus discards queued taps
  latch.press(99);
  assert(!latch.sample(99, true)); // invalid scancodes cannot index the array
}
