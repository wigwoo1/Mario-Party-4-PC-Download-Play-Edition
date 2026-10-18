#ifndef PARTYBOARD_INPUT_TRACE_H
#define PARTYBOARD_INPUT_TRACE_H
#include <dolphin/pad.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Development replay at PAD sampling boundaries, not network frame sync. */
int PartyBoardTraceOpen(const char *path, int replay, u32 seed);
/* Input-only stimulus for a NEW recording; never validates old checkpoints. */
int PartyBoardTraceScriptOpen(const char *path);
int PartyBoardTraceSample(PADStatus pads[4], u32 *rumble);
/* Prepare script/replay input, exchange it, then record the consumed merged input.
 * The callback must not advance simulation. Zero aborts without writing a sample. */
typedef int (*PartyBoardInputExchange)(PADStatus pads[4]);
int PartyBoardTraceSampleWithExchange(PADStatus pads[4], u32 *rumble, PartyBoardInputExchange exchange);
int PartyBoardTraceClose(void);
void PartyBoardTraceSetRecordLimit(u32 samples);
void PartyBoardTraceFastMode(void);
void PartyBoardRandomSeed(u32 seed);
u32 PartyBoardRandomState(void);
int PartyBoardFixedSeed(u32 *seed);
u32 PartyBoardStateCheckpoint(void);
/* Production global-state coverage; legacy trace-v3 checksum stays unchanged. */
u32 PartyBoardSessionCheckpoint(void);
/* Call only on the game thread at a PAD boundary or while that poll is
 * suspended. Captures current production state without advancing simulation. */
typedef struct PartyBoardInputState {
    u32 frame, checkpoint, frand, rand8;
} PartyBoardInputState;
int PartyBoardCaptureInputState(PartyBoardInputState *state);
#ifdef __cplusplus
}
#endif
#endif
