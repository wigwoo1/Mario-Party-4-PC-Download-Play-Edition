#pragma once
#include <filesystem>
#include "main.h"
#include <optional>

namespace partyboard::net {
// Game-thread owner: queue the in-game request and return from GameRun before
// releasing the old engine or entering the lobby. Keep alive until close().
// This does not reset an initialized engine or authorize network simulation.
class NativeMenuHandoff {
    bool installed_ = false;
    bool allowJoin_=true;
    std::optional<int> pending_;
    static int request(void* context, int action) {
        auto& owner = *static_cast<NativeMenuHandoff*>(context);
        if (!owner.installed_ || owner.pending_ || (action != 0 && action != 1)) return 0;
        if(action==1&&!owner.allowJoin_)return 0;
        owner.pending_ = action;
        PartyBoard_GameRequestStop();
        return 1;
    }
public:
    explicit NativeMenuHandoff(bool allowJoin=true):allowJoin_(allowJoin){}
    NativeMenuHandoff(const NativeMenuHandoff&) = delete;
    NativeMenuHandoff& operator=(const NativeMenuHandoff&) = delete;
    NativeMenuHandoff(NativeMenuHandoff&&) = delete;
    NativeMenuHandoff& operator=(NativeMenuHandoff&&) = delete;
    bool install() {
        if (installed_ || pending_) return false;
        installed_ = PartyBoardNativeSetOnlineMenuHandler(request, this) == 1;
        return installed_;
    }
    // Failure leaves ownership intact; caller must retain this object.
    bool close() {
        if (!installed_) return false;
        if (!PartyBoardNativeClearOwnedOnlineMenuHandler(request, this)) return false;
        installed_ = false;
        return true;
    }
    std::optional<int> takeStoppedRequest() {
        if (PartyBoard_GameLifecyclePhase() != 2 || installed_) return std::nullopt;
        auto result = pending_;
        pending_.reset();
        return result;
    }
};
}
