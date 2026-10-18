#pragma once
#include "controller_routes.hpp"
#include "netplay_handshake.hpp"

namespace partyboard::net {
// Coordinator entry point: both barriers must describe the same lobby snapshot.
// Transport/session authentication and shader/asset preparation precede readiness.
inline bool startLobby(ReadyGate& lobby, const ControllerCapabilities& capabilities,
                       uint32_t caller, uint64_t revision) {
    if (capabilities.revision() != lobby.revision() || capabilities.owners() != lobby.owners()
            || !capabilities.ready()) return false;
    return lobby.start(caller, revision);
}
}
