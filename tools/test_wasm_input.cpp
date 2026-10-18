#include <port/net_diagnostic.h>
#include <port/wasm_input.h>
#include <cassert>
#include <cstdio>
#include <cstring>
extern "C" {s32 rnd_seed=1;u32 PartyBoardRandomState(void){return 1;}}
// This isolated adapter contract supplies the session checkpoint used by the
// production input exchange; checkpoint semantics have separate engine tests.
extern "C" u32 PartyBoardSessionCheckpoint(void) {return 42;}
int main() {
    PADStatus pads[4]{};
    assert(PartyBoardBrowserOnlineInputRequired()==0);
    pads[0].button=0x100; pads[1].stickX=-80; pads[2].triggerLeft=200;
    PADStatus original[4]; std::memcpy(original,pads,sizeof(pads));
    assert(PartyBoardNetDiagnosticSample(pads));
    assert(std::memcmp(original,pads,sizeof(pads))==0);
    assert(PartyBoardNetDiagnosticRumble(0xf0000000)==0xf0000000);
    PartyBoardBrowserRequireOnlineInput(1);
    assert(PartyBoardBrowserOnlineInputRequired()==1);
    assert(!PartyBoardNetDiagnosticSample(pads));
    assert(PartyBoardNetDiagnosticRumble(0xf0000000)==0);
    assert(std::memcmp(original,pads,sizeof(pads))==0);
    PartyBoardBrowserRequireOnlineInput(0);
    assert(PartyBoardNetDiagnosticSample(pads));
    assert(!PartyBoardNetDiagnosticSample(nullptr));
    puts("Browser input mode passed: local pads preserved; unwired online input rejected.");
}
