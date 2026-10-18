#include "game/data.h"
#ifdef TARGET_PC
#include "game/audio.h"
#endif
#include "game/dvd.h"
#include "game/gamework.h"
#include "game/gamework_data.h"
#include "game/hsfformat.h"
#include "game/hu3d.h"
#include "game/init.h"
#include "game/minigame_seq.h"
#include "game/msm.h"
#include "game/object.h"
#include "game/pad.h"
#include "game/perf.h"
#include "game/printfunc.h"
#include "game/process.h"
#include "game/sprite.h"
#include "game/sreset.h"
#include "game/wipe.h"
#include "version.h"

#ifdef TARGET_PC
SHARED_SYM void PartyBoardTraceScrollIntegrity(const char *phase);
#define TraceFrameScrollIntegrity(phase) PartyBoardTraceScrollIntegrity(phase)
#else
#define TraceFrameScrollIntegrity(phase) ((void)0)
#endif

#ifdef TARGET_PC
#include "game/disp.h"
#include "port/settings.h"
#include "port/imgui.h"
#include "port/main.h"
#include "port/input_trace.h"
#include "port/session_checkpoint.h"
#include "port/dolassets.h"
#include "port/ui.h"
#include "aurora/dvd.h"
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <stdlib.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define PARTYBOARD_BROWSER_API EMSCRIPTEN_KEEPALIVE
#else
#define PARTYBOARD_BROWSER_API
#endif

const char *__asan_default_options()
{
    return "new_delete_type_mismatch=0,sleep_before_dying=5,allocator_may_return_null=1";
}

bool PartyBoard_IsRunning = TRUE;
bool PartyBoard_IsShuttingDown = FALSE;
bool PartyBoard_IsGameLaunched = FALSE;
bool PartyBoard_RestartRequested = FALSE;

bool disableFrameLimiter = FALSE;
/* A held UI turbo key must not overwrite explicit diagnostic pacing. */
static bool partyBoardTurboHeld;
/* 0=new, 1=initializing, 2=initialized/idle, 3=running. */
static unsigned char partyBoardEnginePhase;
SHARED_SYM extern u32 GlobalCounter;
/* Snapshot after completed simulation frames so async browser observation
   cannot compare mid-frame state against native pre-frame state. */
static double partyBoardSequenceTrace[256][24];
static unsigned int partyBoardSequenceTraceCount, partyBoardSequenceTraceNext;
static void PartyBoardRecordSequenceTrace(void) {
    double *record;
    int field;
    if (GlobalCounter == 1) partyBoardSequenceTraceCount = partyBoardSequenceTraceNext = 0;
    record = partyBoardSequenceTrace[partyBoardSequenceTraceNext];
    record[0] = GlobalCounter;
    record[1] = omcurovl;
    for (field = 0; field < 12; ++field) record[2 + field] = MGSeqDiagnosticFieldGet(field);
    record[14] = wipeData.mode;
    record[15] = wipeData.stat;
    record[16] = wipeData.time;
    record[17] = wipeData.duration;
    record[18] = Hu3DPauseF;
    for (field = 0; field < 4; ++field) record[19 + field] = GWPlayer[field].coin_win;
    record[23] = minimumVcount;
    partyBoardSequenceTraceNext = (partyBoardSequenceTraceNext + 1) % 256;
    if (partyBoardSequenceTraceCount < 256) ++partyBoardSequenceTraceCount;
}
PARTYBOARD_BROWSER_API double PartyBoard_GameSequenceTrace(int row, int field) {
    unsigned int index;
    if (row == -1) return partyBoardSequenceTraceCount;
    if (field == -1) return 24;
    if (row < 0 || (unsigned int)row >= partyBoardSequenceTraceCount || field < 0 || field >= 24) return -1;
    index = (partyBoardSequenceTraceNext + 256 - partyBoardSequenceTraceCount + row) % 256;
    return partyBoardSequenceTrace[index][field];
}
#ifdef __EMSCRIPTEN__
/* Presentation diagnostics only. These wall times include asynchronous waits
   and recovery pauses, and never participate in deterministic state. */
static void PartyBoardRecordFrameTiming(const double times[8]) {
    static unsigned int completedFrames;
    static double totals[7];
    for (int stage = 0; stage < 7; ++stage) totals[stage] += times[stage + 1] - times[stage];
    if (++completedFrames % 120 == 0) {
        EM_ASM({globalThis.partyBoardFrameStageTiming = ({
            completedFrames: $0, eventsMs: $1, inputMs: $2,
            beginFrameMs: $3, simulationMs: $4, drawMs: $5,
            submitMs: $6, pacingMs: $7
        });}, completedFrames, totals[0], totals[1], totals[2],
            totals[3], totals[4], totals[5], totals[6]);
    }
}
/* Opt-in current-stage observation survives asynchronous suspension. It is
   excluded from simulation state and never invokes a game export to read it. */
static void PartyBoardObserveFrameStage(int stage, unsigned int frame) {
    EM_ASM({
        if (globalThis.testFrameStageDiagnostics === true)
            globalThis.partyBoardActiveFrameStage = ({stage: $0, frame: $1});
    }, stage, frame);
}
EM_JS(int, partyBoardOnlineMenuRequest, (int action), {
    try {
        return typeof Module.partyBoardOnlineMenuRequest === 'function' &&
            Module.partyBoardOnlineMenuRequest(action) === true ? 1 : 0;
    } catch (_) { return 0; }
});
#else
#define PartyBoardObserveFrameStage(stage, frame) ((void)0)
#endif
#ifndef __EMSCRIPTEN__
static PartyBoardNativeOnlineMenuHandler partyBoardNativeMenuHandler;
static void* partyBoardNativeMenuContext;
int PartyBoardNativeSetOnlineMenuHandler(PartyBoardNativeOnlineMenuHandler handler,void* context)
{
    if (partyBoardEnginePhase == 3) return 0;
    if (handler && partyBoardNativeMenuHandler) return 0;
    partyBoardNativeMenuHandler = handler;
    partyBoardNativeMenuContext = handler ? context : NULL;
    return 1;
}
int PartyBoardNativeClearOwnedOnlineMenuHandler(PartyBoardNativeOnlineMenuHandler handler,void* context)
{
    if (partyBoardEnginePhase == 3 || !handler ||
        partyBoardNativeMenuHandler != handler || partyBoardNativeMenuContext != context) return 0;
    partyBoardNativeMenuHandler = NULL;
    partyBoardNativeMenuContext = NULL;
    return 1;
}
#endif
PARTYBOARD_BROWSER_API int PartyBoard_OnlineMenuRequest(int action)
{
    if (action != 0 && action != 1) return 0;
#ifdef __EMSCRIPTEN__
    return partyBoardOnlineMenuRequest(action);
#else
    return partyBoardNativeMenuHandler &&
        partyBoardNativeMenuHandler(partyBoardNativeMenuContext, action) == 1 ? 1 : 0;
#endif
}
PARTYBOARD_BROWSER_API int PartyBoard_GameLifecyclePhase(void) { return partyBoardEnginePhase; }
static bool partyBoardWindowExitRequested;
int PartyBoard_GameWindowExitRequested(void) { return partyBoardWindowExitRequested ? 1 : 0; }
static bool partyBoardStartClockAligned;
static bool partyBoardSessionSeedConfigured;
static bool partyBoardStateDiagnosticsEnabled;
void PartyBoardStateSnapshot(unsigned long long sample);
#endif

extern FileListEntry _ovltbl[];
SHARED_SYM u32 GlobalCounter;
#ifdef TARGET_PC
PARTYBOARD_BROWSER_API unsigned int PartyBoard_GameFrameCount(void) { return GlobalCounter; }
/* Read-only progress evidence for full-board validation; this getter never
   advances play or changes the deterministic checkpoint. */
static int partyBoardMenuBoard = -1, partyBoardMenuPort = -1;
static int partyBoardInteractionStage;
static int partyBoardSetupTurns = -1, partyBoardSetupRow = -1, partyBoardSetupPort = -1;
void PartyBoard_ObserveBoardInteraction(int stage) { partyBoardInteractionStage = stage; }
void PartyBoard_ObserveBoardSelection(int board, int port) {
    partyBoardMenuBoard = board;
    partyBoardMenuPort = port;
    partyBoardSetupTurns = partyBoardSetupRow = partyBoardSetupPort = -1;
}
void PartyBoard_ObservePartySetup(int turns, int row, int port) {
    partyBoardSetupTurns = turns;
    partyBoardSetupRow = row;
    partyBoardSetupPort = port;
}
PARTYBOARD_BROWSER_API int PartyBoard_GameProgress(int field) {
    switch (field) {
        case 0: return GWSystem.turn;
        case 1: return GWSystem.max_turn;
        case 2: return GWSystem.board;
        case 3: return GWSystem.player_curr;
        case 4: return omcurovl;
        case 5: return omovlevtno;
        case 6: return omcurovl == DLL_mentdll ? partyBoardMenuBoard : -1;
        case 7: return omcurovl == DLL_mentdll ? partyBoardMenuPort : -1;
        case 8: return omcurovl == DLL_w02dll ? partyBoardInteractionStage : -1;
        case 9: return omcurovl == DLL_mentdll ? partyBoardSetupTurns : -1;
        case 10: return omcurovl == DLL_mentdll ? partyBoardSetupRow : -1;
        case 11: return omcurovl == DLL_mentdll ? partyBoardSetupPort : -1;
        default: return -1;
    }
}
#endif
static u32 vcheck;
static u32 vmiss;
static u32 vstall;
static u32 top_pixels_in;
static u32 top_pixels_out;
static u32 bot_pixels_in;
static u32 bot_pixels_out;
static u32 clr_pixels_in;
static u32 total_copy_clks;
static u32 cp_req;
static u32 tc_req;
static u32 cpu_rd_req;
static u32 cpu_wr_req;
static u32 dsp_req;
static u32 io_req;
static u32 vi_req;
static u32 pe_req;
static u32 rf_req;
static u32 fi_req;
s32 HuDvdErrWait;
SHARED_SYM s32 SystemInitF;

#ifdef TARGET_PC
void PartyBoard_RequestRestart(void)
{
    PartyBoard_RestartRequested = SUPPORTS_PROCESS_RESTART;
    PartyBoard_IsRunning = FALSE;
}
#endif

#ifdef TARGET_PC
PARTYBOARD_BROWSER_API int PartyBoard_GameInitialize(void)
#else
static void PartyBoard_GameInitialize(void)
#endif
{
    s16 i;
#ifdef TARGET_PC
    if (partyBoardEnginePhase != 0) return -1;
    partyBoardEnginePhase = 1;
#endif
    HuDvdErrWait = 0;
    SystemInitF = 0;
#if VERSION_NTSC
    HuSysInit(&GXNtsc480IntDf);
#else
    HuSysInit(&GXPal528IntDf);
#endif
    HuPrcInit();
    HuPadInit();
    GWInit();
#ifdef TARGET_PC
    /* Four-player online sessions own all four logical ports. Local device
     * presence must not choose human/CPU configuration in shared simulation.
     * Ordinary offline initialization still uses GWInit's device detection. */
    if (partyBoardSessionSeedConfigured) {
        for (i = 0; i < 4; ++i) GWPlayerCfg[i].iscom = 0;
    }
#endif
    pfInit();
    GlobalCounter = 0;
    HuSprInit();
    Hu3DInit();
    HuDataInit();
    HuPerfInit();
    HuPerfCreate("USR0", 0xFF, 0xFF, 0xFF, 0xFF);
    HuPerfCreate("USR1", 0, 0xFF, 0xFF, 0xFF);
    WipeInit(RenderMode);

    for (i = 0; i < 4; i++) {
        GWPlayerCfg[i].character = -1;
    }
    
    omMasterInit(0, _ovltbl, DLL_MAX, DLL_bootdll);
    VIWaitForRetrace();

    if (VIGetNextField() == 0) {
        OSReport("VI_FIELD_BELOW\n");
        VIWaitForRetrace();
    }
#ifdef TARGET_PC
    partyBoardEnginePhase = 2;
    {
#ifdef __EMSCRIPTEN__
        const bool diagnostic = EM_ASM_INT({return globalThis.testTraceState ? 1 : 0;});
#else
        const char *setting = getenv("PARTYBOARD_TRACE_STATE_DIAGNOSTICS");
        const bool diagnostic = setting && setting[0] == '1' && setting[1] == 0;
#endif
        partyBoardStateDiagnosticsEnabled = diagnostic;
        if (diagnostic) {
            extern u32 PartyBoardRandomState(void);
            extern s32 rnd_seed;
            printf("Prepared engine RNG: %u %u\n", PartyBoardRandomState(), (u32)rnd_seed);
            PartyBoardStateSnapshot(0xffffffffULL);
        }
    }
    return 0;
#endif
}

#ifdef TARGET_PC
PARTYBOARD_BROWSER_API int PartyBoard_GameRun(void)
#else
static void PartyBoard_GameRun(void)
#endif
{
    u32 met0;
    u32 met1;
    s32 retrace;
#ifdef TARGET_PC
    static int progressTrace = -1;
    static int lastProgress[6] = { -2, -2, -2, -2, -2, -2 };
#endif
#if VERSION_PAL
    s16 temp = 0;
#endif

#ifdef TARGET_PC
    if (partyBoardEnginePhase != 2) return -1;
    partyBoardEnginePhase = 3;
    partyBoardWindowExitRequested = false;
    while (PartyBoard_IsRunning) {
#else
    while (1) {
#endif
#ifdef __EMSCRIPTEN__
        double frameTimes[8];
        frameTimes[0] = emscripten_get_now();
        PartyBoardObserveFrameStage(0, GlobalCounter); /* events */
#endif
#ifdef TARGET_PC
        const AuroraEvent *event = aurora_update();
        bool exiting = false;
        while (event != NULL && event->type != AURORA_NONE) {
            if (event->type == AURORA_EXIT) {
                partyBoardWindowExitRequested = true;
                exiting = true;
                break;
            }
            if (event->type == AURORA_SDL_EVENT) {
                ui_handle_sdl_event(&event->sdl);
                if (event->sdl.type == SDL_EVENT_WINDOW_FOCUS_LOST) partyBoardTurboHeld = FALSE;
                if (partyboard_settings_enableTurboKeybind()) {
                    if (event->sdl.type == SDL_EVENT_KEY_DOWN) {
                        if (event->sdl.key.scancode == SDL_SCANCODE_TAB) {
                            partyBoardTurboHeld = TRUE;
                        }
                    } else if (event->sdl.type == SDL_EVENT_KEY_UP) {
                        if (event->sdl.key.scancode == SDL_SCANCODE_TAB) {
                            partyBoardTurboHeld = FALSE;
                        }
                    }
                }
            }
            ++event;
        }
        if (exiting) {
            break;
        }
#endif
        retrace = VIGetRetraceCount();
        if (HuSoftResetButtonCheck() != 0 || HuDvdErrWait != 0) {
            continue;
        }
#ifdef __EMSCRIPTEN__
        frameTimes[1] = emscripten_get_now();
        PartyBoardObserveFrameStage(1, GlobalCounter); /* online input */
        // Never enter a simulation/render frame without accepted online input.
        // Retrace callbacks skip online sampling so each frame owns one poll.
        if (!HuPadReadOnlineFrame()) break;
        frameTimes[2] = emscripten_get_now();
        PartyBoardObserveFrameStage(2, GlobalCounter); /* renderer begin */
#endif
#if defined(TARGET_PC) && !defined(__EMSCRIPTEN__)
        // Native online exchange must complete before any simulation/render work.
        if (!HuPadReadOnlineFrame()) break;
#endif
        HuPerfZero();

        HuPerfBegin(2);
#ifdef TARGET_PC
        aurora_begin_frame();
#endif
#ifdef __EMSCRIPTEN__
        frameTimes[3] = emscripten_get_now();
        PartyBoardObserveFrameStage(3, GlobalCounter); /* simulation */
#endif
        PartyBoardObserveFrameStage(30, GlobalCounter); /* pre-render system update */
        TraceFrameScrollIntegrity("frame-before-render");
        HuSysBeforeRender();
        PartyBoardObserveFrameStage(31, GlobalCounter);
        GXSetGPMetric(GX_PERF0_CLIP_VTX, GX_PERF1_VERTICES);
        GXClearGPMetric();
        GXSetVCacheMetric(GX_VC_ALL);
        GXClearVCacheMetric();
        GXClearPixMetric();
        GXClearMemMetric();

        HuPerfBegin(0);
        Hu3DPreProc();
        PartyBoardObserveFrameStage(32, GlobalCounter);
        HuPadRead();
        PartyBoardObserveFrameStage(33, GlobalCounter);
        pfClsScr();

        TraceFrameScrollIntegrity("frame-before-processes");
        PartyBoardObserveFrameStage(34, GlobalCounter); /* game processes */
        HuPrcCall(1);
        PartyBoardObserveFrameStage(35, GlobalCounter);
        TraceFrameScrollIntegrity("frame-after-processes");
        MGSeqMain();
        PartyBoardObserveFrameStage(36, GlobalCounter);
        TraceFrameScrollIntegrity("frame-after-sequences");
#ifdef __EMSCRIPTEN__
        frameTimes[4] = emscripten_get_now();
        PartyBoardObserveFrameStage(4, GlobalCounter); /* drawing */
#endif
        HuPerfBegin(1);
        Hu3DExec();
        TraceFrameScrollIntegrity("frame-after-drawing");
        HuDvdErrorWatch();
        WipeExecAlways();
        HuPerfEnd(0);

        pfDrawFonts();
        HuPerfEnd(1);

        msmMusFdoutEnd();
#ifdef TARGET_PC
        HuAudPortStreamTick();
#endif
        HuSysDoneRender(retrace);
        TraceFrameScrollIntegrity("frame-after-submission");
        GXReadGPMetric(&met0, &met1);
        GXReadVCacheMetric(&vcheck, &vmiss, &vstall);
        GXReadPixMetric(&top_pixels_in, &top_pixels_out, &bot_pixels_in, &bot_pixels_out, &clr_pixels_in, &total_copy_clks);
        GXReadMemMetric(&cp_req, &tc_req, &cpu_rd_req, &cpu_wr_req, &dsp_req, &io_req, &vi_req, &pe_req, &rf_req, &fi_req);
        HuPerfEnd(2);
        GlobalCounter++;
#ifdef TARGET_PC
        PartyBoardRecordSequenceTrace();
        if (progressTrace < 0) {
            const char *setting = getenv("PARTYBOARD_TRACE_GAME_PROGRESS");
            progressTrace = setting && setting[0] == '1' && setting[1] == '\0';
        }
        if (progressTrace) {
            s32 currentProgress[6];
            s32 field;
            for (field = 0; field < 6; field++) {
                currentProgress[field] = PartyBoard_GameProgress(field);
            }
            if (memcmp(currentProgress, lastProgress, sizeof(currentProgress)) != 0) {
                OSReport("Party native progress: frame=%u turn=%d max=%d board=%d player=%d overlay=%d event=%d\n",
                    GlobalCounter, currentProgress[0], currentProgress[1], currentProgress[2],
                    currentProgress[3], currentProgress[4], currentProgress[5]);
                memcpy(lastProgress, currentProgress, sizeof(lastProgress));
            }
        }
#endif
#ifdef __EMSCRIPTEN__
        frameTimes[5] = emscripten_get_now();
        PartyBoardObserveFrameStage(5, GlobalCounter); /* submission */
#endif

#ifdef TARGET_PC
        ui_update();
        aurora_end_frame();
#ifdef __EMSCRIPTEN__
        frameTimes[6] = emscripten_get_now();
        PartyBoardObserveFrameStage(6, GlobalCounter); /* pacing */
#endif
        if (!disableFrameLimiter && !partyBoardTurboHeld) {
            frame_limiter();
#ifdef __EMSCRIPTEN__
        } else {
            extern unsigned int PartyBoardBrowserTakeRetraces(void);
            PartyBoardBrowserTakeRetraces();
#endif
        }
#endif
#ifdef __EMSCRIPTEN__
        frameTimes[7] = emscripten_get_now();
        PartyBoardRecordFrameTiming(frameTimes);
        PartyBoardObserveFrameStage(7, GlobalCounter); /* complete */
#endif
    }

#ifdef TARGET_PC
    partyBoardEnginePhase = 2;
    if (partyBoardStateDiagnosticsEnabled) PartyBoardStateSnapshot(0xfffffffdULL);
    return 0;
#endif
}

#ifdef TARGET_PC
PARTYBOARD_BROWSER_API double PartyBoard_GamePausedState(int field)
{
    extern s32 rnd_seed;
    if (partyBoardEnginePhase != 2) return -1;
    switch (field) {
        case 0: return (double)GlobalCounter;
        case 1: return (double)PartyBoardSessionCheckpoint();
        case 2: return (double)PartyBoardRandomState();
        case 3: return (double)(u32)rnd_seed;
        default: return -1;
    }
}

PARTYBOARD_BROWSER_API double PartyBoard_GamePreparedCheckpoint(void)
{
    if (partyBoardEnginePhase != 2 || GlobalCounter != 0 || !partyBoardStartClockAligned) return -1;
    return (double)PartyBoardSessionCheckpoint();
}

PARTYBOARD_BROWSER_API int PartyBoard_GameConfigureSessionSeed(unsigned int seed)
{
    if (partyBoardEnginePhase != 0 || partyBoardSessionSeedConfigured) return -1;
    PartyBoardRandomSeed(seed);
    partyBoardSessionSeedConfigured = TRUE;
    return 0;
}

PARTYBOARD_BROWSER_API int PartyBoard_GameAlignStartClock(void)
{
    u32 seed;
    if (partyBoardEnginePhase != 2 || GlobalCounter != 0 || partyBoardStartClockAligned ||
        !PartyBoardFixedSeed(&seed)) return -1;
    VCounter = 0;
    partyBoardStartClockAligned = TRUE;
    if (partyBoardStateDiagnosticsEnabled) PartyBoardStateSnapshot(0xfffffffeULL);
    return 0;
}

PARTYBOARD_BROWSER_API int PartyBoard_GameResume(void)
{
    if (partyBoardEnginePhase != 2) return -1;
    PartyBoard_IsRunning = TRUE;
    return PartyBoard_GameRun();
}

PARTYBOARD_BROWSER_API void PartyBoard_GameRequestStop(void)
{
    PartyBoard_IsRunning = FALSE;
}
#endif

#ifdef TARGET_PC
int game_main(void)
{
    if (PartyBoard_GameInitialize() != 0) return -1;
    return PartyBoard_GameRun();
}
#else
void main(void)
{
    PartyBoard_GameInitialize();
    PartyBoard_GameRun();
}
#endif

void HuSysVWaitSet(s16 vcount)
{
    minimumVcount = vcount;
    minimumVcountf = vcount;
}

s16 HuSysVWaitGet(s16 param)
{
    return (s16)minimumVcount;
}

s32 rnd_seed = 0x0000D9ED;

#ifdef TARGET_PC
/* Diagnostic subset only: deliberately excludes pointers and structure padding. */
static u32 TraceField(u32 hash, u32 value) {
    int byte;
    for (byte = 0; byte < 4; ++byte) {
        hash = (hash ^ (value & 255)) * 16777619u;
        value >>= 8;
    }
    return hash;
}
u32 PartyBoardStateCheckpoint(void) {
    extern u32 boardRandSeed;
    u32 hash = 2166136261u;
    int i, item;
#define TRACE_FIELD(value) hash = TraceField(hash, (u32)(value))
    TRACE_FIELD(GlobalCounter); TRACE_FIELD(VCounter);
    TRACE_FIELD(boardRandSeed);
    TRACE_FIELD(omCurrentOvlGet()); TRACE_FIELD(omovlevtno); TRACE_FIELD(omovlstat);
    TRACE_FIELD(GWSystem.turn); TRACE_FIELD(GWSystem.max_turn);
    TRACE_FIELD(GWSystem.board); TRACE_FIELD(GWSystem.player_curr);
    TRACE_FIELD(GWSystem.mg_next); TRACE_FIELD(GWSystem.mg_type);
    for (i = 0; i < 4; ++i) {
        TRACE_FIELD(GWPlayerCfg[i].character); TRACE_FIELD(GWPlayerCfg[i].pad_idx);
        TRACE_FIELD(GWPlayerCfg[i].iscom); TRACE_FIELD(GWPlayerCfg[i].diff);
        TRACE_FIELD(GWPlayer[i].character); TRACE_FIELD(GWPlayer[i].space_curr);
        TRACE_FIELD(GWPlayer[i].space_next); TRACE_FIELD(GWPlayer[i].coins);
        TRACE_FIELD(GWPlayer[i].stars); TRACE_FIELD(GWPlayer[i].roll);
        for (item = 0; item < 3; ++item) TRACE_FIELD(GWPlayer[i].items[item]);
    }
#undef TRACE_FIELD
    return hash;
}
u32 PartyBoardSessionCheckpoint(void) {
    u32 hash = PartyBoardGlobalStateCheckpoint(PartyBoardStateCheckpoint(),
        &GWSystem, GWPlayerCfg, GWPlayer);
    hash = PartyBoardGameStatCheckpoint(hash, &GWGameStat);
    hash = TraceField(hash, PartyBoardRandomState());
    return TraceField(hash, (u32)rnd_seed);
}
PARTYBOARD_BROWSER_API double PartyBoard_GameCheckpointComponent(int field) {
    u32 hash;
    if (field < 0 || field > 3) return -1;
    /* Match PartyBoardCaptureInputState: lobby-owned gameplay also uses these
       globals, independently of the local-play lifecycle marker. */
    hash = PartyBoardStateCheckpoint();
    if (field == 0) return (double)hash;
    hash = PartyBoardGlobalStateCheckpoint(hash, &GWSystem, GWPlayerCfg, GWPlayer);
    if (field == 1) return (double)hash;
    hash = PartyBoardGameStatCheckpoint(hash, &GWGameStat);
    if (field == 2) return (double)hash;
    return (double)TraceField(TraceField(hash, PartyBoardRandomState()), (u32)rnd_seed);
}
typedef struct { int requested, count; double value; } PartyBoardGlobalFieldQuery;
extern s32 BoardMushroomDiagnosticStageGet(void);
extern s32 BoardHappeningDiagnosticStageGet(void);
extern double BoardLotteryDiagnosticFieldGet(int field);
PARTYBOARD_BROWSER_API double PartyBoard_GameSequenceField(int field) {
    return MGSeqDiagnosticFieldGet(field);
}
PARTYBOARD_BROWSER_API double PartyBoard_GameLotteryField(int field) {
    return BoardLotteryDiagnosticFieldGet(field);
}
PARTYBOARD_BROWSER_API int PartyBoard_GameItemEventStage(void) {
    /* Low nibble: mushroom stage; bit 4: happening callback still active. */
    return BoardMushroomDiagnosticStageGet() | (BoardHappeningDiagnosticStageGet() << 4);
}
static void PartyBoardQueryGlobalField(void *context, const char *name, u32 value) {
    PartyBoardGlobalFieldQuery *query = (PartyBoardGlobalFieldQuery *)context;
    (void)name;
    if (query->count == query->requested) query->value = (double)value;
    ++query->count;
}
PARTYBOARD_BROWSER_API double PartyBoard_GameGlobalCheckpointField(int field) {
    PartyBoardGlobalFieldQuery query = { field, 0, -1 };
    if (field < -1 || field > 1024) return -1;
    PartyBoardGlobalStateCheckpointFields(2166136261u, &GWSystem, GWPlayerCfg, GWPlayer,
        PartyBoardQueryGlobalField, &query);
    return field == -1 ? (double)query.count : query.value;
}
int PartyBoardCaptureInputState(PartyBoardInputState *state) {
    if (!state) return 0;
    state->frame = GlobalCounter;
    state->checkpoint = PartyBoardSessionCheckpoint();
    state->frand = PartyBoardRandomState();
    state->rand8 = (u32)rnd_seed;
    return 1;
}
static void PartyBoardPrintGlobalField(void *context, const char *name, u32 value) {
    int *index = (int *)context;
    printf("%s[\"%s\",%u]", (*index)++ ? "," : "", name, value);
}
void PartyBoardStateSnapshot(unsigned long long sample) {
    extern u32 boardRandSeed;
    int i;
    printf("State snapshot: {\"sample\":%llu,\"frame\":%u,\"vCounter\":%d,\"boardSeed\":%u,\"overlay\":%d,\"event\":%d,\"status\":%d,\"system\":[%d,%d,%d,%d,%d,%d],\"players\":[",
        sample, GlobalCounter, VCounter, boardRandSeed, omCurrentOvlGet(), omovlevtno, omovlstat,
        GWSystem.turn, GWSystem.max_turn, GWSystem.board, GWSystem.player_curr, GWSystem.mg_next, GWSystem.mg_type);
    for (i = 0; i < 4; ++i) {
        printf("%s[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]", i ? "," : "",
            GWPlayerCfg[i].character, GWPlayerCfg[i].pad_idx, GWPlayerCfg[i].iscom, GWPlayerCfg[i].diff,
            GWPlayer[i].character, GWPlayer[i].space_curr, GWPlayer[i].space_next, GWPlayer[i].coins,
            GWPlayer[i].stars, GWPlayer[i].roll, GWPlayer[i].items[0], GWPlayer[i].items[1], GWPlayer[i].items[2]);
    }
    puts("]}");
    {
        int index = 0;
        printf("Global state snapshot: {\"sample\":%llu,\"frame\":%u,\"fields\":[", sample, GlobalCounter);
        PartyBoardGlobalStateCheckpointFields(2166136261u, &GWSystem, GWPlayerCfg, GWPlayer,
            PartyBoardPrintGlobalField, &index);
        puts("]}");
    }
}
void PartyBoardTraceFastMode(void) {
    disableFrameLimiter = TRUE;
}
#endif

s32 rand8(void)
{
#ifdef TARGET_PC
    /* Defined modulo-2^32 arithmetic, including the addition before shifting. */
    u32 state = (u32)rnd_seed * 0x41C64E6Du + 0x3039u;
    rnd_seed = (s32)state;
    return (u8)((state + 1u) >> 16);
#else
    rnd_seed = (rnd_seed * 0x41C64E6D) + 0x3039;
    return (u8)(((rnd_seed + 1) >> 16) & 0xFF);
#endif
}
