#pragma once
#include <set>
#include <cstdint>
#include <cstddef>

namespace partyboard::net {
// Diagnostic evidence only: callers must also verify normalized input agreement
// and inspect the rendered placements. This never advances game state.
class NativeFullMatchProgress {
    int board_, turns_, awardsOverlay_;
    std::set<int> observedTurns_;
    bool awards_ = false, ranking_ = false;
    uint64_t rankingFrame_ = 0;
public:
    NativeFullMatchProgress(int board, int turns, int awardsOverlay)
        : board_(board), turns_(turns), awardsOverlay_(awardsOverlay) {}
    bool observe(uint64_t frame, int board, int turn, int maxTurn, int overlay, int event) {
        if (board != board_ || maxTurn != turns_) { ranking_ = false; return false; }
        if (turn >= 1 && turn <= turns_) observedTurns_.insert(turn);
        if (overlay != awardsOverlay_) { ranking_ = false; return false; }
        if(turn == turns_ && event == 0)awards_ = true;
        if (event != 1) { ranking_ = false; return false; }
        if (!ranking_) { ranking_ = true; rankingFrame_ = frame; }
        return awards_ && observedTurns_.size() == static_cast<size_t>(turns_)
            && turn == turns_ && frame >= rankingFrame_ && frame - rankingFrame_ >= 600;
    }
    size_t observedTurnCount() const { return observedTurns_.size(); }
};
}
