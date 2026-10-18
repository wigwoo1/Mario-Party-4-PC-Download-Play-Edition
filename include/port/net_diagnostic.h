#pragma once
#include <dolphin/pad.h>
#include "port/input_trace.h"
#ifdef __cplusplus
extern "C" {
#endif
int PartyBoardNetDiagnosticOpen(int host, unsigned short port, const char* config, u32 seed);
int PartyBoardNetDiagnosticSample(PADStatus pads[4]);
u32 PartyBoardNetDiagnosticRumble(u32 localCapability);
int PartyBoardNetDiagnosticClose(void);
#ifndef __EMSCRIPTEN__
/* Game-thread coordinator hook; install only at aligned prepared frame zero.
 * Callback waits for an authorized merged frame/recovery, without simulation.
 * Return 1 only after acceptance; failure never copies changed PAD state. */
typedef int (*PartyBoardNativeInputExchange)(void*, PADStatus[4], const PartyBoardInputState*);
int PartyBoardNativeInstallInputExchange(PartyBoardNativeInputExchange exchange, void* context);
int PartyBoardNativeClearInputExchange(void);
/* Clear only this exact callback/context owner, after the engine stops. */
int PartyBoardNativeClearOwnedInputExchange(PartyBoardNativeInputExchange exchange,void* context);
int PartyBoardNativeOnlineInputRequired(void);
int PartyBoardNativeSetOwnedGlyphLanes(PartyBoardNativeInputExchange exchange,void* context,const s8 lanes[4]);
int PartyBoardNativeGlyphLane(int logicalPort);
#endif
#ifdef __cplusplus
}
#endif
