#pragma once
#include <functional>
namespace partyboard::net {
// Explicit local diagnostic after real native engine preparation. Trusted stdin
// signaling bootstrap and fixed diagnostic identities, not production lobby.
int runNativeRtcEngineProbe(unsigned frames,std::function<bool()> pumpEvents,bool diagnosticButtons=false);
}
