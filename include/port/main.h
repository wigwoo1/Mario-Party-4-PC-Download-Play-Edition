#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include "version.h"

extern bool PartyBoard_IsRunning;
extern bool PartyBoard_IsShuttingDown;
extern bool PartyBoard_IsGameLaunched;
extern bool PartyBoard_RestartRequested;

#if defined(__ANDROID__) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS) || \
  (defined(TARGET_OS_TV) && TARGET_OS_TV)
#define SUPPORTS_PROCESS_RESTART FALSE
#else
#define SUPPORTS_PROCESS_RESTART TRUE
#endif

// Initialize game systems and the boot overlay without entering the frame loop.
// Call once per runtime, after disc/renderer setup; this is not a netplay reset.
// Returns -1 on duplicate/reentrant initialization, 0 on success.
int PartyBoard_GameInitialize(void);
// Read-only lifecycle: 0=new, 1=initializing, 2=idle, 3=running.
int PartyBoard_GameLifecyclePhase(void);
int PartyBoard_GameWindowExitRequested(void);
// Read-only simulation frame count, for runtime/UI progress.
unsigned int PartyBoard_GameFrameCount(void);
// Read-only: turn, maximum turns, board, current player, overlay, event.
int PartyBoard_GameProgress(int field);
// 0=Host, 1=Join. Accepted requests are handed to the runtime owner.
// Returns zero when no frontend can handle this action.
SHARED_SYM int PartyBoard_OnlineMenuRequest(int action);
#ifndef __EMSCRIPTEN__
// Register before entering GameRun; clear only after it returns. A frontend
// returns 1 only when it owns the request. Callback/context must remain alive.
typedef int (*PartyBoardNativeOnlineMenuHandler)(void* context,int action);
int PartyBoardNativeSetOnlineMenuHandler(PartyBoardNativeOnlineMenuHandler handler,void* context);
// Runtime owners detach only their own callback after GameRun returns.
int PartyBoardNativeClearOwnedOnlineMenuHandler(PartyBoardNativeOnlineMenuHandler handler,void* context);
#endif
// Configure agreed RNG seed once BEFORE initialization, including seed zero.
// Returns -1 after initialization or on repeat; does not authorize frames.
int PartyBoard_GameConfigureSessionSeed(unsigned int seed);
// Run initialized systems until IsRunning is false; caller must serialize runs.
// Does not initialize, reset state, or authorize network simulation frames.
// Returns -1 before initialization or during an overlapping run.
int PartyBoard_GameRun(void);
// Request loop exit at a frame boundary; await GameRun before releasing assets.
void PartyBoard_GameRequestStop(void);
// Explicit caller authorization: enable and run an initialized idle engine.
int PartyBoard_GameResume(void);
// Opt-in fixed-seed session clock epoch. Once, idle, before any game frame.
// Does not align input/process/audio/full state or reset RNG; -1 if unavailable.
int PartyBoard_GameAlignStartClock(void);
// Selected-state checkpoint only for aligned, initialized, pre-frame idle state.
// Double preserves every uint32 hash and the distinct -1 unavailable sentinel.
double PartyBoard_GamePreparedCheckpoint(void);
// Read-only idle state: 0=frame,1=selected checkpoint,2=frand,3=rand8 RNG.
// Returns -1 outside initialized idle phase or for an invalid field.
double PartyBoard_GamePausedState(int field);
// Read-only idle checkpoint prefixes: 0=legacy,1=global,2=progression,3=final.
// For tracing cross-platform disagreement, not a substitute start checkpoint.
double PartyBoard_GameCheckpointComponent(int field);
// Same ordered semantic global schema as checkpoint hashing; -1 queries count.
double PartyBoard_GameGlobalCheckpointField(int field);
int PartyBoard_GameItemEventStage(void);
double PartyBoard_GameLotteryField(int field);
double PartyBoard_GameSequenceField(int field);
// Completed-frame diagnostic history; row=-1 gives count, field=-1 width.
double PartyBoard_GameSequenceTrace(int row, int field);
// Read-only menu telemetry; board -1 means selection is not accepting input.
SHARED_SYM void PartyBoard_ObserveBoardSelection(int board, int port);
// Read-only party setup telemetry: selected turn length, active row, input port.
SHARED_SYM void PartyBoard_ObservePartySetup(int turns, int row, int port);
// Last entered board-1 interaction function; diagnostic only, excluded from checkpoints.
SHARED_SYM void PartyBoard_ObserveBoardInteraction(int stage);

void PartyBoard_RequestRestart(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" std::filesystem::path PartyBoard_ConfigPath;
#endif
