#include "../include/port/reconnect_policy.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using namespace partyboard::net;
int main() {
    ReconnectPolicy p({11, 22, 22, 33});
    assert(!p.disconnected(0, 0));
    assert(!p.disconnected(44, 0));
    assert(p.state() == ReconnectPolicy::State::Running);
    assert(p.disconnected(22, 100));
    const auto first = p.epoch();
    assert(p.missingSlots()[1] && p.missingSlots()[2]);
    assert(p.remainingMs(100) == 60000);
    assert(p.disconnected(22, 1000));
    assert(p.disconnected(33, 2000));
    assert(p.remainingMs(2100) == 58000);
    assert(!p.markRecovered(22, first + 1, p.revision(), 2100));
    assert(p.markRecovered(22, first, p.revision(), 3000));
    assert(!p.readyToResume(3000));
    assert(p.markRecovered(33, first, p.revision(), 4000));
    assert(p.readyToResume(4000));
    assert(p.state() == ReconnectPolicy::State::Paused); // Separate synchronized barrier.
    const auto oldRevision = p.revision();
    assert(p.disconnected(22, 5000));
    assert(p.epoch() == first);
    assert(!p.markRecovered(22, first, oldRevision, 5000));
    assert(p.missingSlots()[1] && p.missingSlots()[2]);
    assert(!p.resume(first, p.revision(), 5000));
    assert(p.markRecovered(22, first, p.revision(), 60099));
    assert(!p.resume(first, oldRevision, 60099));
    assert(p.resume(first, p.revision(), 60099)); // One millisecond before deadline.
    assert(p.disconnected(11, 61000)); // Host loss follows the same policy.
    assert(p.epoch() != first);
    assert(!p.markRecovered(11, first, p.revision(), 61001));
    assert(!p.markRecovered(11, p.epoch(), p.revision(), 121000)); // Exact deadline loses.
    assert(p.state() == ReconnectPolicy::State::ReturnToMenu);
    assert(!p.resume(p.epoch(), p.revision(), 121001));
    assert(!p.disconnected(11, 121001));
    ReconnectPolicy cancelled({1, 0, 0, 0});
    cancelled.cancel();
    assert(cancelled.update(0) == ReconnectPolicy::State::ReturnToMenu);
    ReconnectPolicy backwards({1, 0, 0, 0});
    assert(backwards.disconnected(1, 100));
    assert(backwards.update(99) == ReconnectPolicy::State::ReturnToMenu);
    ReconnectPolicy unsafeClock({1, 0, 0, 0});
    assert(!unsafeClock.disconnected(1, std::numeric_limits<uint64_t>::max()));
    assert(unsafeClock.state() == ReconnectPolicy::State::ReturnToMenu);
    ReconnectPolicy largeClock({1, 0, 0, 0});
    const auto nearMax = 9007199254740990ULL; // One millisecond below JavaScript's maximum safe integer.
    assert(largeClock.disconnected(1, nearMax));
    assert(largeClock.remainingMs(nearMax + 1) == 59999);
    bool rejected = false;
    try { ReconnectPolicy empty({0, 0, 0, 0}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    ReconnectPolicy delayed({11,22,33,44});
    assert(delayed.update(20000)==ReconnectPolicy::State::Running);
    assert(delayed.disconnected(22,20340,0));
    assert(delayed.remainingMs(20340)==39660);
    assert(delayed.disconnected(33,25000,24000));
    assert(delayed.remainingMs(25000)==35000);
    assert(delayed.markRecovered(22,delayed.epoch(),delayed.revision(),59999));
    assert(delayed.markRecovered(33,delayed.epoch(),delayed.revision(),59999));
    assert(delayed.resume(delayed.epoch(),delayed.revision(),59999));
    assert(delayed.disconnected(22,80000,70000));
    assert(delayed.remainingMs(80000)==50000);
    assert(delayed.remainingMs(130000)==0);
    for(const auto observed:{60000ULL,60001ULL}) {
        ReconnectPolicy expired({11,22,0,0});
        assert(!expired.disconnected(22,observed,0));
        assert(expired.state()==ReconnectPolicy::State::ReturnToMenu);
    }
    ReconnectPolicy futureAnchor({11,22,0,0});
    assert(!futureAnchor.disconnected(22,100,101));
    assert(futureAnchor.state()==ReconnectPolicy::State::ReturnToMenu);
    std::cout << "Reconnect policy tests passed\n";
}

