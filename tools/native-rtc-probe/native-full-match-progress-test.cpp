#include <port/native_full_match_progress.hpp>
#include <cassert>
#include <iostream>
int main() {
    using partyboard::net::NativeFullMatchProgress;
    NativeFullMatchProgress progress(0,20,78);
    for(int turn=1;turn<=20;++turn)assert(!progress.observe(turn,0,turn,20,89,0));
    assert(progress.observedTurnCount()==20);
    assert(!progress.observe(100,0,20,20,78,0));
    assert(!progress.observe(200,0,20,20,78,1));
    assert(!progress.observe(799,0,20,20,78,1));
    assert(progress.observe(800,0,20,20,78,1));
    // Leaving rankings invalidates the settling interval.
    assert(!progress.observe(801,0,20,20,89,0));
    assert(!progress.observe(900,0,20,20,78,1));
    assert(!progress.observe(1499,0,20,20,78,1));
    assert(progress.observe(1500,0,20,20,78,1));
    // The frontend exit verdict records historical completion, even after
    // ordinary results-to-menu navigation invalidates the current interval.
    NativeFullMatchProgress returned(0,5,78);
    bool completed = false;
    for(int turn=1;turn<=5;++turn)
        completed |= returned.observe(turn,0,turn,5,89,0);
    completed |= returned.observe(100,0,5,5,78,0);
    completed |= returned.observe(200,0,5,5,78,1);
    completed |= returned.observe(799,0,5,5,78,1);
    assert(!completed);
    completed |= returned.observe(800,0,5,5,78,1);
    assert(completed);
    completed |= returned.observe(801,0,5,5,89,0);
    completed |= returned.observe(900,-1,0,0,0,0);
    assert(completed);
    assert(!returned.observe(901,0,5,5,78,1));
    NativeFullMatchProgress missingTurn(0,20,78);
    for(int turn=2;turn<=20;++turn)missingTurn.observe(turn,0,turn,20,89,0);
    assert(!missingTurn.observe(200,0,20,20,78,1));
    assert(!missingTurn.observe(800,0,20,20,78,1));
    assert(!progress.observe(1600,1,20,20,78,1));
    assert(!progress.observe(1600,0,20,10,78,1));
    assert(!progress.observe(1601,0,20,20,78,1));
    assert(!progress.observe(2200,0,20,20,78,1));
    assert(progress.observe(2201,0,20,20,78,1));
    NativeFullMatchProgress noAwards(0,20,78);
    for(int turn=1;turn<=20;++turn)noAwards.observe(turn,0,turn,20,89,0);
    assert(!noAwards.observe(100,0,20,20,78,1));
    assert(!noAwards.observe(700,0,20,20,78,1));
    std::cout << "PASS native full-match progress: all turns, continuous settled rankings, board/configuration guards\n";
}
