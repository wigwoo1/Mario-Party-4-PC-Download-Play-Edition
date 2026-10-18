#define main disc_contract_main
#include "test_wasm_asset_mount.cpp"
#undef main
#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <emscripten.h>
extern "C" {
#include <game/audio.h>
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardTestAudioTick(void) {
    HuAudPortStreamTick();
    return HuAudPortStreamQueuedBytes();
}
// Bounded read-only handoff probe; no graphics or game initialization required.
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardTestReadMountedDiscHeader(void) {
    FILE* file = std::fopen("/assets/verified-disc.ciso", "rb");
    if (!file) return -1;
    unsigned char header[4]{};
    const size_t read = std::fread(header, 1, sizeof(header), file);
    const int closed = std::fclose(file);
    return read == sizeof(header) && closed == 0 && std::memcmp(header, "CISO", 4) == 0 ? 0 : -1;
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardTestInspectMountedGame(void) {
    char program[] = "downloaded-disc-probe";
    char path[] = "/assets/verified-disc.ciso";
    char inspect[] = "--inspect-game";
    char* arguments[] = {program, path, inspect, nullptr};
    return disc_contract_main(3, arguments);
}
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_keyboard.h>
#include <imgui.h>
#include <port/settings.h>
#include <port/config.hpp>
#include <port/wasm_language.h>
#include <port/main.h>
#include <port/input_trace.h>
#ifdef PARTYBOARD_BOOT_SETUP_PROBE
#include <dlfcn.h>
#endif
extern "C" {
#include <game/init.h>
#include <game/memory.h>
#ifdef PARTYBOARD_BOOT_SETUP_PROBE
#include <game/object.h>
int PartyBoardOverlayStateCapture(void *);
int PartyBoardOverlayStateRestore(void *);
#include <game/wipe.h>
#include <game/hu3d.h>
#include <game/pad.h>
#include <game/minigame_seq.h>
#include <game/printfunc.h>
#include <game/card.h>
#include <game/gamework_data.h>
#include <game/objsub.h>
#include <game/saveload.h>
#include <game/sprite.h>
#include <game/window.h>
#include <port/imgui.h>
#endif
int game_main(void);
extern u32 GlobalCounter;
}

#ifdef PARTYBOARD_BOOT_SETUP_PROBE
// Run only on a prepared, frame-zero engine. Uses the actual sprite allocator,
// resolver and ownership implementation; it does not alter game progression.
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardTestControlGlyphLifetime(int port) {
    if (GlobalCounter != 0 || port < 0 || port >= 4) return -1;
    ANIMDATA* masked = HuWinControlGlyphForMask(2, 1 << port);
    if (!masked) return -10;
    HuSprAnimKill(masked);
    masked = HuWinControlGlyphForMask(2, 0xf);
    if (!masked) return -11;
    HuSprAnimKill(masked);
    masked = HuWinControlGlyphForMask(2, 0xf & ~(1 << port));
    if (masked) { HuSprAnimKill(masked); return -12; }
    if (HuWinControlGlyphForMask(2, 0)) return -13;
    for (int cycle = 0; cycle < 64; ++cycle) {
        ANIMDATA* original = HuSprAnimMake(30, 30, ANIM_BMP_RGBA8);
        if (!original) return -2;
        original->bmp->data = HuMemDirectMalloc(HEAP_DATA, 30 * 30 * 4);
        if (!original->bmp->data) { HuSprAnimKill(original); return -3; }
        std::memset(original->bmp->data, 255, 30 * 30 * 4);
        HuSprAnimLock(original);
        const s16 sprite = HuSprCreate(original, 0, 0);
        if (sprite < 0) { HuSprAnimKill(original); return -4; }
        int result = 0;
        if (!HuSprControlGlyphSet(sprite, 2, port, 0, FALSE) ||
            HuSprData[sprite].data == original || original->useNum != 2 ||
            HuSprData[sprite].data->useNum != 1) result = -5;
        if (!result && (!HuSprControlGlyphSet(sprite, 2, port, 0, TRUE) ||
            HuSprData[sprite].r != 127 || HuSprData[sprite].g != 127 ||
            HuSprData[sprite].scale.x * HuSprData[sprite].data->pat[0].sizeX != 30)) result = -6;
        // Alternate restored-original destruction and destruction with an
        // active replacement, covering both retained-reference paths.
        if (!result && cycle % 2 == 0 && (HuSprControlGlyphSet(sprite, -1, port, 0, FALSE) ||
            HuSprData[sprite].data != original || original->useNum != 2 ||
            HuSprData[sprite].r != 255 || HuSprData[sprite].scale.x != 1)) result = -7;
        HuSprKill(sprite);
        if (!result && original->useNum != 1) result = -8;
        HuSprAnimKill(original);
        if (result) return result;
    }
    return GlobalCounter == 0 ? 0 : -9;
}
#endif

#ifdef PARTYBOARD_BOOT_SETUP_PROBE
extern "C" EMSCRIPTEN_KEEPALIVE void PartyBoardTestBeginSessionTrace(void) {
    assert(GlobalCounter == 0);
    assert(PartyBoardTraceOpen("/startup.trace", 0, 1));
    assert(PartyBoardTraceScriptOpen("/stimulus.pbtrace"));
    const unsigned count = EM_ASM_INT({return globalThis.testSampleLimit || 8;});
    assert(count >= 8 && count <= 30004);
    PartyBoardTraceSetRecordLimit(count);
}
extern "C" EMSCRIPTEN_KEEPALIVE unsigned PartyBoardTestExternalFrames(void) { return GlobalCounter; }
extern "C" EMSCRIPTEN_KEEPALIVE void PartyBoardTestFinishExternalEngine(void) {
    const unsigned limit = EM_ASM_INT({return globalThis.testSampleLimit || 8;});
    assert(!PartyBoard_IsRunning && GlobalCounter <= limit + 1);
    printf("External engine final overlay: %d\n", omCurrentOvlGet());
    EM_ASM({globalThis.bootProbeImage = document.getElementById("canvas").toDataURL("image/png");});
    printf("External engine cleanup frame: %u\n", GlobalCounter);
    assert(PartyBoardTraceClose() == 0);
    assert(omwatchproc);
    const s32 loadedDll = omcurdll;
    const bool loaded = loadedDll >= 0 && loadedDll < OM_DLL_MAX && omDLLinfoTbl[loadedDll];
    if (!omwatchproc->child) puts("External cleanup: startup stopped before overlay child creation.");
    omOvlKill(1);
    if (loaded) omDLLEnd(loadedDll, 1);
    assert(!omwatchproc->child && (!loaded || !omDLLinfoTbl[loadedDll]));
    aurora_dvd_close(); aurora_shutdown();
    puts("External engine lifecycle passed: JavaScript Asyncify initialize/run and cleanup.");
    puts("Disc renderer passed: external lifecycle complete.");
}
static bool watcherKeyboard;
static ANIMDATA* keyboardGlyph(s16 entry) {
    PADButton action;
    switch(entry) {
        case 2: action=PAD_BUTTON_A;break;
        case 3: action=PAD_BUTTON_B;break;
        case 4: action=PAD_BUTTON_X;break;
        case 5: action=PAD_BUTTON_Y;break;
        case 6: action=PAD_TRIGGER_R;break;
        case 8: action=PAD_TRIGGER_L;break;
        case 11: action=PAD_TRIGGER_Z;break;
        default:return nullptr;
    }
    u32 count=0;
    auto* bindings=PADGetKeyButtonBindings(0,&count);
    const char* label=nullptr;
    for(u32 index=0;bindings && index<count;++index) {
        if(bindings[index].padButton==action) {label=SDL_GetScancodeName(static_cast<SDL_Scancode>(bindings[index].scancode));break;}
    }
    if(!label || !EM_ASM_INT({return globalThis.testKeyboardGlyphs?.[UTF8ToString($0)]?1:0;},label))return nullptr;
    auto* glyph=HuSprAnimMake(64,64,ANIM_BMP_RGBA8);
    glyph->bmp->data=HuMemDirectMalloc(HEAP_DATA,16384);
    assert(glyph->bmp->data);
    EM_ASM({HEAPU8.set(globalThis.testKeyboardGlyphs[UTF8ToString($1)],$0);},glyph->bmp->data,label);
    printf("Window keyboard glyph installed: bank %d uses %s key texture.\n",entry,label);
    HuSprAnimLock(glyph);
    return glyph;
}
static void browserKeyboardPrompt(void) {
    while (GlobalCounter < 950) HuPrcVSleep();
    EM_ASM({ globalThis.watcherKeyboardReady = true; });
    for (;;) HuPrcVSleep();
}
static void browserMentTransition(void) {
    HuPrcSleep(120);
    puts("Game setup probe: requesting Party setup through normal overlay watcher.");
    omOvlCallEx(DLL_mentdll, 1, 0, 0);
    for (;;) HuPrcVSleep();
}
static void browserBoardStateObserver(void) {
    for (;;) {
        EM_ASM({
            (globalThis.boardSnapshots ||= []).push({frame:$0,turn:$1,player:$2,
                spaces:[$3,$4,$5,$6],characters:[$7,$8,$9,$10]});
        }, GlobalCounter, GWSystem.turn, GWSystem.player_curr,
           GWPlayer[0].space_curr, GWPlayer[1].space_curr, GWPlayer[2].space_curr, GWPlayer[3].space_curr,
           GWPlayer[0].character, GWPlayer[1].character, GWPlayer[2].character, GWPlayer[3].character);
        HuPrcSleep(120);
    }
}
static const char* browserBoardModulePath(s16 overlay) {
    switch (overlay) {
        case DLL_w04dll: return "/w04Dll.wasm";
#ifdef PARTYBOARD_BOARD_LOAD_AUDIT
        case DLL_w01dll: return "/w01Dll.wasm";
        case DLL_w02dll: return "/w02Dll.wasm";
        case DLL_w03dll: return "/w03Dll.wasm";
        case DLL_w05dll: return "/w05Dll.wasm";
        case DLL_w06dll: return "/w06Dll.wasm";
        case DLL_w10dll: return "/w10Dll.wasm";
#endif
        default: return nullptr;
    }
}
static const char* browserMinigameModulePath(s16 overlay) {
    switch (overlay) {
        case DLL_m419dll: return "/m419Dll.wasm";
#ifdef PARTYBOARD_THREE_THROW_LOAD_PROBE
        case DLL_m410dll: return "/m410Dll.wasm";
#endif
#ifdef PARTYBOARD_FOUR_PLAYER_LOAD_AUDIT
        case DLL_m401dll: return "/m401Dll.wasm";
        case DLL_m402dll: return "/m402Dll.wasm";
        case DLL_m403dll: return "/m403Dll.wasm";
        case DLL_m404dll: return "/m404Dll.wasm";
        case DLL_m405dll: return "/m405Dll.wasm";
        case DLL_m406dll: return "/m406Dll.wasm";
        case DLL_m407dll: return "/m407Dll.wasm";
        case DLL_m408dll: return "/m408Dll.wasm";
        case DLL_m409dll: return "/m409Dll.wasm";
        case DLL_m411dll: return "/m411Dll.wasm";
        case DLL_m412dll: return "/m412Dll.wasm";
        case DLL_m413dll: return "/m413Dll.wasm";
        case DLL_m414dll: return "/m414Dll.wasm";
        case DLL_m415dll: return "/m415Dll.wasm";
#endif
#ifdef PARTYBOARD_MINIGAME_LOAD_AUDIT
        case DLL_m416dll: return "/m416Dll.wasm";
        case DLL_m417dll: return "/m417Dll.wasm";
        case DLL_m418dll: return "/m418Dll.wasm";
        case DLL_m420dll: return "/m420Dll.wasm";
        case DLL_m421dll: return "/m421Dll.wasm";
        case DLL_m422dll: return "/m422Dll.wasm";
        case DLL_m423dll: return "/m423Dll.wasm";
        case DLL_m424dll: return "/m424Dll.wasm";
#endif
#ifdef PARTYBOARD_COMPETITIVE_LOAD_AUDIT
        case DLL_m425dll: return "/m425Dll.wasm";
        case DLL_m426dll: return "/m426Dll.wasm";
        case DLL_m427dll: return "/m427Dll.wasm";
        case DLL_m428dll: return "/m428Dll.wasm";
        case DLL_m429dll: return "/m429Dll.wasm";
        case DLL_m430dll: return "/m430Dll.wasm";
        case DLL_m431dll: return "/m431Dll.wasm";
        case DLL_m432dll: return "/m432Dll.wasm";
        case DLL_m433dll: return "/m433Dll.wasm";
        case DLL_m434dll: return "/m434Dll.wasm";
        case DLL_m435dll: return "/m435Dll.wasm";
        case DLL_m436dll: return "/m436Dll.wasm";
        case DLL_m437dll: return "/m437Dll.wasm";
        case DLL_m438dll: return "/m438Dll.wasm";
        case DLL_m439dll: return "/m439Dll.wasm";
        case DLL_m440dll: return "/m440Dll.wasm";
        case DLL_m441dll: return "/m441Dll.wasm";
        case DLL_m442dll: return "/m442Dll.wasm";
        case DLL_m443dll: return "/m443Dll.wasm";
        case DLL_m444dll: return "/m444Dll.wasm";
        case DLL_m445dll: return "/m445Dll.wasm";
        case DLL_m446dll: return "/m446Dll.wasm";
        case DLL_m447dll: return "/m447Dll.wasm";
        case DLL_m448dll: return "/m448Dll.wasm";
        case DLL_m449dll: return "/m449Dll.wasm";
#endif
#ifdef PARTYBOARD_REMAINING_MINIGAME_AUDIT
        case DLL_m450dll: return "/m450Dll.wasm";
        case DLL_m451dll: return "/m451Dll.wasm";
        case DLL_m453dll: return "/m453Dll.wasm";
        case DLL_m455dll: return "/m455Dll.wasm";
        case DLL_m456dll: return "/m456Dll.wasm";
        case DLL_m457dll: return "/m457Dll.wasm";
        case DLL_m458dll: return "/m458Dll.wasm";
        case DLL_m459dll: return "/m459Dll.wasm";
        case DLL_m460dll: return "/m460Dll.wasm";
        case DLL_m461dll: return "/m461Dll.wasm";
        case DLL_m462dll: return "/m462Dll.wasm";
        case DLL_m463dll: return "/m463Dll.wasm";
#endif
#if defined(PARTYBOARD_FOUR_PLAYER_LOAD_AUDIT) && !defined(PARTYBOARD_THREE_THROW_LOAD_PROBE)
        case DLL_m410dll: return "/m410Dll.wasm";
#endif
        default: return nullptr;
    }
}
static const char* browserModeMenuPath(s16 overlay) {
#ifdef PARTYBOARD_MODE_MENU_AUDIT
    switch (overlay) {
        case DLL_mgmodedll: return "/mgmodedll.wasm";
        case DLL_mpexDll: return "/mpexDll.wasm";
        case DLL_option: return "/option.wasm";
        case DLL_present: return "/present.wasm";
        case DLL_mstoryDll: return "/mstoryDll.wasm";
        case DLL_mstory2Dll: return "/mstory2Dll.wasm";
        case DLL_mstory3Dll: return "/mstory3Dll.wasm";
        case DLL_mstory4Dll: return "/mstory4Dll.wasm";
        case DLL_selmenuDll: return "/selmenuDll.wasm";
        case DLL_staffDll: return "/staffDll.wasm";
        case DLL_subchrselDll: return "/subchrselDll.wasm";
        case DLL_ztardll: return "/ztardll.wasm";
        default: break;
    }
#endif
    return nullptr;
}
static void* browserOverlayHandles[DLL_MAX]{};
// Capture simulation visits independently of the final overlay. Reaching a
// results screen alone does not identify the minigame that ran before it.
static void browserCaptureSceneObserver(void) {
    const s16 overlay = omCurrentOvlGet();
    const u32 first = GlobalCounter;
    for (;;) {
        EM_ASM({
            const scenes = globalThis.pipelineSceneVisits ||= {};
            const visit = scenes[$0] ||= ({overlay:$0, firstFrame:$1, lastFrame:$1, samples:0});
            visit.lastFrame=$2; visit.samples++;
        }, overlay, first, GlobalCounter);
        HuPrcVSleep();
    }
}
static bool browserOverlayStateWasCaptured(void* module) {
    for (auto handle : browserOverlayHandles) if (handle == module) return true;
    return false;
}
static s32 browserOverlayBackend(s16 overlay, s32 start) {
    auto& handles = browserOverlayHandles;
    if (overlay < 0 || overlay >= DLL_MAX) return 0;
    const char* boardPath = browserBoardModulePath(overlay);
    const char* minigamePath = browserMinigameModulePath(overlay);
    const char* menuPath = browserModeMenuPath(overlay);
    const char* path = menuPath ? menuPath : minigamePath ? minigamePath : boardPath ? boardPath : overlay == DLL_bootDll ? "/bootDll.wasm" : overlay == DLL_modeseldll ? "/modeseldll.wasm" : overlay == DLL_mentdll ? "/mentDll.wasm" : overlay == DLL_instdll ? "/instDll.wasm" : overlay == DLL_resultdll ? "/resultDll.wasm" : nullptr;
    if (!path) return 0;
    if (!start) return handles[overlay] && PartyBoardOverlayStateRestore(handles[overlay]);
    if (!handles[overlay]) {
        void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!handle || !PartyBoardOverlayStateCapture(handle)) return 0;
        handles[overlay] = handle;
    }
    auto setup = reinterpret_cast<void (*)(void)>(dlsym(handles[overlay], "ObjectSetup"));
    if (!setup) return 0;
    if (overlay == DLL_instdll) {
        // Isolated startup diagnosis; never linked into the production game.
        if (EM_ASM_INT({return globalThis.testForceM411 ? 1 : 0;})) {
            int selected = -1;
            for (int index = 0; index < 60; ++index)
                if (mgInfoTbl[index].ovl == DLL_m411dll) selected = index;
            assert(selected >= 0);
            GWSystem.mg_next = selected;
            GWSystem.mg_type = -1;
            puts("DIAGNOSTIC OVERRIDE: selected M411 before instructions; not natural Party selection evidence.");
        }
        assert(GWSystem.mg_next >= 0 && GWSystem.mg_next < 60);
        const auto& minigame = mgInfoTbl[GWSystem.mg_next];
        printf("Instruction selected minigame: index %d overlay %u type %u.\n", GWSystem.mg_next, minigame.ovl, minigame.type);
        EM_ASM({globalThis.selectedMinigame=({index:$0,overlay:$1,type:$2});}, GWSystem.mg_next, minigame.ovl, minigame.type);
    }
    setup();
    if (EM_ASM_INT({return globalThis.testCapturePipelines ? 1 : 0;}))
        HuPrcChildCreate(browserCaptureSceneObserver, 90, 8192, 0, omwatchproc);
    if (boardPath)
        HuPrcChildCreate(browserBoardStateObserver, 90, 8192, 0, omwatchproc);
    if (overlay == DLL_modeseldll && EM_ASM_INT({return globalThis.testMentFrames ? 1 : 0;}))
        HuPrcChildCreate(browserMentTransition, 100, 8192, 0, omwatchproc);
    if (watcherKeyboard && overlay == DLL_bootDll)
        HuPrcChildCreate(browserKeyboardPrompt, 100, 8192, 0, omwatchproc);
    return 1;
}
#endif
static void render(bool after) {
    for(int i=0;i<3;++i) {
        aurora_update();
        assert(aurora_begin_frame());
        ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(20,20),ImVec2(180,180),
            after ? IM_COL32(0,255,0,255) : IM_COL32(255,0,0,255));
        aurora_end_frame();
        if(i<2) emscripten_sleep(20);
    }
    int pixel = emscripten_run_script_int(R"JS((()=>{
      const source=document.getElementById('canvas'), copy=document.createElement('canvas');
      copy.width=source.width; copy.height=source.height;
      const ctx=copy.getContext('2d'); ctx.drawImage(source,0,0);
      const p=ctx.getImageData(80,80,1,1).data;
      return p[3]===255 && p[2]===0 ? (p[0]===255 && p[1]===0 ? 1 : p[0]===0 && p[1]===255 ? 2 : 0) : 0;
    })())JS");
    assert(pixel == (after ? 2 : 1));
}
// Diagnostic renderer-only drain bridge, guarded against simulation progress.
extern "C" EMSCRIPTEN_KEEPALIVE unsigned PartyBoardTestPipelineCount(int created) {
    const AuroraStats* stats = aurora_get_stats();
    return created ? stats->createdPipelines : stats->queuedPipelines;
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardTestAdvancePreparedRenderer(void) {
    if (!EM_ASM_INT({return globalThis.testExternalEngine ? 1 : 0;}) || GlobalCounter != 0) return -1;
    render(false);
    return GlobalCounter == 0 ? 0 : -1;
}
int main(int argc, char** argv) {
    const bool startup = argc > 2 && std::strcmp(argv[2], "--startup-game") == 0;
    const bool watcherStart = startup && argc > 3 && std::strcmp(argv[3], "--watcher-start") == 0;
    const bool watcherTitle = watcherStart || (startup && argc > 3 && std::strcmp(argv[3], "--watcher-title") == 0);
    const bool watcher = watcherTitle || (startup && argc > 3 && std::strcmp(argv[3], "--watcher") == 0);
    const unsigned sampleLimit = watcherStart ? EM_ASM_INT({return globalThis.testSampleLimit || 2004;}) : watcherTitle ? 1004 : EM_ASM_INT({return globalThis.testSampleLimit || 8;});
    assert(sampleLimit >= 8 && sampleLimit <= 30004);
    const bool frames = watcher || (startup && argc > 3 && std::strcmp(argv[3], "--frames") == 0);
    partyboard::registerSettings();
    partyboard::config::FinishRegistration();
    AuroraConfig config{};
    config.appName="Disc and renderer integration";
    config.userPath="/"; config.cachePath="/"; config.resourcesPath="/";
    config.windowWidth=640; config.windowHeight=480;
    config.mem1Size=64*1024*1024; config.mem2Size=24*1024*1024;
    config.desiredBackend=BACKEND_WEBGPU;
    config.vsync=true; config.allowCpuAdapter=true; config.logLevel=LOG_INFO;
    config.logCallback=[](AuroraLogLevel,const char* module,const char* message,unsigned int len) {
        printf("%s: %.*s\n",module,(int)len,message);
    };
    assert(aurora_initialize(argc,argv,&config).window);
    aurora_set_pause_on_focus_lost(false);
    if (watcherStart) {
        PADInit();
        PADStatus initial[4]{}; PADRead(initial);
        PADSetKeyboardActive(0, TRUE);
        PADSetKeyButtonBinding(0, {SDL_SCANCODE_RETURN, PAD_BUTTON_START});
        PADSetKeyButtonBinding(0, {SDL_SCANCODE_Z, PAD_BUTTON_A});
        PADSetKeyButtonBinding(0, {SDL_SCANCODE_X, PAD_BUTTON_B});
        PADSetKeyButtonBinding(0, {SDL_SCANCODE_C, PAD_BUTTON_X});
        PADSetKeyButtonBinding(0, {SDL_SCANCODE_V, PAD_BUTTON_Y});
        PADSetKeyButtonBinding(0, {SDL_SCANCODE_E, PAD_TRIGGER_R});
        PADSetKeyButtonBinding(0, {SDL_SCANCODE_Q, PAD_TRIGGER_L});
        PADSetKeyButtonBinding(0, {SDL_SCANCODE_R, PAD_TRIGGER_Z});
        PADSetKeyAxisBinding(0, {SDL_SCANCODE_D, PAD_AXIS_LEFT_X_POS, 0});
        PADSetKeyAxisBinding(0, {SDL_SCANCODE_A, PAD_AXIS_LEFT_X_NEG, 0});
        PADSetKeyAxisBinding(0, {SDL_SCANCODE_W, PAD_AXIS_LEFT_Y_POS, 0});
        PADSetKeyAxisBinding(0, {SDL_SCANCODE_S, PAD_AXIS_LEFT_Y_NEG, 0});
#ifdef PARTYBOARD_BOOT_SETUP_PROBE
        if(EM_ASM_INT({return globalThis.testKeyboardGlyphs?1:0;})) HuWinControlGlyphResolverSet(keyboardGlyph);
#endif
    }
    render(false);
    if(startup) {
        assert(aurora_dvd_open(argv[1]));
        partyboard::version::init();
        PartyBoardBrowserLanguageInit();
        InitializeDol();
#ifdef PARTYBOARD_BOOT_SETUP_PROBE
        watcherKeyboard = watcherStart;
        if (watcher) omDLLBrowserSetBackend(browserOverlayBackend);
#endif
        if (EM_ASM_INT({return globalThis.testPostInitTrace ? 1 : 0;})) {
            PartyBoardRandomSeed(1);
        } else if(frames) {
            assert(PartyBoardTraceOpen("/startup.trace", 0, 1));
            if (EM_ASM_INT({return globalThis.testInputScript ? 1 : 0;}))
                assert(PartyBoardTraceScriptOpen("/stimulus.pbtrace"));
            PartyBoardTraceSetRecordLimit(sampleLimit);
        }
        if (EM_ASM_INT({return globalThis.testExternalEngine ? 1 : 0;})) {
            puts("External engine platform preparation passed.");
            return 0;
        }
        PartyBoard_IsRunning = frames;
        assert(PartyBoard_GameRun() == -1);
        assert(PartyBoard_GameInitialize() == 0);
        assert(EM_ASM_INT({return Module.ccall("PartyBoard_GameInitialize", "number", [], []);}) == -1);
        puts("Engine lifecycle guards passed: premature run and duplicate initialization rejected.");
        assert(GlobalCounter == 0);
        EM_ASM({
            for (const name of ["PartyBoard_GameInitialize", "PartyBoard_GameRun", "PartyBoard_GameRequestStop"])
                if (typeof Module["_" + name] !== "function") throw Error("Missing engine export: " + name);
            if (typeof Module.ccall !== "function") throw Error("Missing Asyncify invocation API");
            Module._PartyBoard_GameRequestStop();
        });
        assert(!PartyBoard_IsRunning);
        assert(PartyBoard_GameRun() == 0 && GlobalCounter == 0);
        puts("Initialized engine idle run passed: no simulation frames.");
        PartyBoard_IsRunning = frames;
        assert(PartyBoard_GameRun() == 0);
        if(frames) {
            assert(PartyBoardTraceClose() == 0);
            assert(GlobalCounter > 0 && GlobalCounter <= sampleLimit);
            if (watcherTitle) assert(GlobalCounter >= 900);
            printf("Bounded game loop passed: %u frames; watcher backend %s.\n", GlobalCounter, watcher ? "enabled" : "disabled");
        } else assert(GlobalCounter == 0);
#ifdef PARTYBOARD_BOOT_SETUP_PROBE
        if (watcher) {
            if (watcherTitle) EM_ASM({ globalThis.bootProbeImage = document.getElementById('canvas').toDataURL('image/png'); });
            const bool mentFrames = EM_ASM_INT({return globalThis.testMentFrames ? 1 : 0;});
            const bool boardFrames = EM_ASM_INT({return globalThis.testBoardRun ? 1 : 0;});
            const bool returnFrames = EM_ASM_INT({return globalThis.testBoardReturn ? 1 : 0;});
            const int expectedBoardId = EM_ASM_INT({return globalThis.testExpectedBoard || 4;});
            const int expectedMinigameId = EM_ASM_INT({return globalThis.testExpectedMinigame || 419;});
            const s16 expectedMinigame = expectedMinigameId == 410 ? DLL_m410dll : expectedMinigameId == 411 ? DLL_m411dll : expectedMinigameId == 419 ? DLL_m419dll : -1;
            assert(expectedMinigame >= 0);
            const s16 expectedBoard = expectedBoardId == 1 ? DLL_w01dll : expectedBoardId == 2 ? DLL_w02dll : expectedBoardId == 3 ? DLL_w03dll : expectedBoardId == 4 ? DLL_w04dll : expectedBoardId == 5 ? DLL_w05dll : expectedBoardId == 6 ? DLL_w06dll : expectedBoardId == 10 ? DLL_w10dll : -1;
            assert(expectedBoard >= 0);
            const bool resultFrames = EM_ASM_INT({return globalThis.testResultRun ? 1 : 0;});
            const bool mgFrames = EM_ASM_INT({return globalThis.testMinigameRun ? 1 : 0;});
            const bool instFrames = EM_ASM_INT({return globalThis.testInstRun ? 1 : 0;});
            printf("Watcher final overlay: %d\n", omcurovl);
            const bool modeEntry = EM_ASM_INT({return globalThis.testModeEntry ? 1 : 0;});
            assert(omcurovl == (modeEntry ? DLL_mgmodedll : returnFrames ? expectedBoard : resultFrames ? DLL_resultdll : mgFrames ? expectedMinigame : instFrames ? DLL_instdll : boardFrames ? expectedBoard : mentFrames ? DLL_mentdll : watcherStart ? DLL_modeseldll : DLL_bootDll) && omwatchproc->child);
            assert(omcurdll >= 0 && omDLLinfoTbl[omcurdll]);
            omOvlKill(1);
            omDLLEnd(omcurdll, 1);
            assert(!omwatchproc->child && !omDLLinfoTbl[omcurdll]);
            if (returnFrames) printf("Board return frames passed: watcher returned to w%02d and cleaned up its child.\n", expectedBoardId);
            else if (resultFrames) puts("Results frames passed: normal watcher reached resultDll and cleaned up its child.");
            else if (mgFrames) printf("Minigame frames passed: normal watcher reached m%d and cleaned up its child.\n", expectedMinigameId);
            else if (instFrames) puts("Instruction frames passed: normal watcher reached instDll and cleaned up its child.");
            else if (boardFrames) printf("Board frames passed: normal watcher reached w%02d and cleaned up its child.\n", expectedBoardId);
            else if (modeEntry) puts("Minigame Mode entry passed: normal menu input reached mgmodedll and cleaned up its child.");
            else if (mentFrames) puts("Game setup frames passed: normal overlay watcher reached Party setup, rendered and cleaned up its child.");
#ifdef PARTYBOARD_MENT_LOAD_PROBE
            // The execution path already captured and restored this module through
            // the backend. Capture is a one-time operation, not a reload check.
            if (!mentFrames) {
            void* ment = dlopen("/mentDll.wasm", RTLD_NOW | RTLD_LOCAL);
            if (!ment) fprintf(stderr, "Game setup module load failed: %s\n", dlerror());
            assert(ment && dlsym(ment, "ObjectSetup"));
            assert(PartyBoardOverlayStateCapture(ment));
            assert(PartyBoardOverlayStateRestore(ment));
            puts("Game setup module load passed: imports resolved and private state restored; ObjectSetup not executed.");
            }
#endif
            omDLLBrowserSetBackend(nullptr);
#ifdef PARTYBOARD_W04_LOAD_PROBE
            if (!boardFrames) {
            void* board = dlopen("/w04Dll.wasm", RTLD_NOW | RTLD_LOCAL);
            if (!board) fprintf(stderr, "Board module load failed: %s\n", dlerror());
            assert(board && dlsym(board, "ObjectSetup"));
            assert(PartyBoardOverlayStateCapture(board));
            assert(PartyBoardOverlayStateRestore(board));
            puts("Board module load passed: w04 imports resolved and private state restored; ObjectSetup not executed.");
            }
#endif
#ifdef PARTYBOARD_INST_LOAD_PROBE
            if (!instFrames) {
            void* instructions = dlopen("/instDll.wasm", RTLD_NOW | RTLD_LOCAL);
            if (!instructions) fprintf(stderr, "Instruction module load failed: %s\n", dlerror());
            assert(instructions && dlsym(instructions, "ObjectSetup"));
            assert(PartyBoardOverlayStateCapture(instructions));
            assert(PartyBoardOverlayStateRestore(instructions));
            puts("Instruction module load passed: imports resolved and private state restored; ObjectSetup not executed.");
            }
#endif
#ifdef PARTYBOARD_THREE_THROW_LOAD_PROBE
            void* threeThrow = dlopen("/m410Dll.wasm", RTLD_NOW | RTLD_LOCAL);
            if (!threeThrow) fprintf(stderr, "Three Throw load failed: %s\n", dlerror());
            assert(threeThrow && dlsym(threeThrow, "ObjectSetup"));
            if (!browserOverlayHandles[DLL_m410dll]) assert(PartyBoardOverlayStateCapture(threeThrow));
            assert(PartyBoardOverlayStateRestore(threeThrow));
            puts("Three Throw module load passed: imports resolved and state restored; ObjectSetup not executed by audit.");
#endif
#ifdef PARTYBOARD_FOUR_PLAYER_LOAD_AUDIT
            for (int id = 401; id <= 415; ++id) {
                char path[32];
                snprintf(path, sizeof(path), "/m%dDll.wasm", id);
                void* module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
                if (!module) fprintf(stderr, "Four-player module load failed (%d): %s\n", id, dlerror());
                assert(module && dlsym(module, "ObjectSetup"));
                bool captured = false;
#ifdef PARTYBOARD_THREE_THROW_LOAD_PROBE
                captured = id == 410;
#endif

                for (auto handle : browserOverlayHandles) if (handle == module) captured = true;
                if (!captured) assert(PartyBoardOverlayStateCapture(module));
                assert(PartyBoardOverlayStateRestore(module));
                printf("Four-player module load passed: m%d imports resolved and state restored; ObjectSetup not executed by audit.\n", id);
            }
#endif
#ifdef PARTYBOARD_MODE_MENU_AUDIT
            for (const char* path : {"/mgmodedll.wasm", "/mpexDll.wasm", "/option.wasm", "/present.wasm", "/mstoryDll.wasm", "/mstory2Dll.wasm", "/mstory3Dll.wasm", "/mstory4Dll.wasm", "/selmenuDll.wasm", "/staffDll.wasm", "/subchrselDll.wasm", "/ztardll.wasm"}) {
                void* module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
                if (!module) fprintf(stderr, "Mode menu load failed (%s): %s\n", path, dlerror());
                assert(module && dlsym(module, "ObjectSetup"));
                if (!browserOverlayStateWasCaptured(module)) assert(PartyBoardOverlayStateCapture(module));
                assert(PartyBoardOverlayStateRestore(module));
                printf("Mode menu load passed: %s imports resolved and state restored; ObjectSetup not executed by audit.\n", path);
            }
#endif
#ifdef PARTYBOARD_REMAINING_MINIGAME_AUDIT
            for (int id : {450, 451, 453, 455, 456, 457, 458, 459, 460, 461, 462, 463}) {
                char path[32];
                snprintf(path, sizeof(path), "/m%dDll.wasm", id);
                void* module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
                if (!module) fprintf(stderr, "Remaining minigame load failed (%d): %s\n", id, dlerror());
                assert(module && dlsym(module, "ObjectSetup"));
                if (!browserOverlayStateWasCaptured(module)) assert(PartyBoardOverlayStateCapture(module));
                assert(PartyBoardOverlayStateRestore(module));
                printf("Remaining minigame load passed: m%d imports resolved and state restored; ObjectSetup not executed.\n", id);
            }
#endif
#ifdef PARTYBOARD_COMPETITIVE_LOAD_AUDIT
            for (int id = 425; id <= 449; ++id) {
                char path[32];
                snprintf(path, sizeof(path), "/m%dDll.wasm", id);
                void* module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
                if (!module) fprintf(stderr, "Competitive module load failed (%d): %s\n", id, dlerror());
                assert(module && dlsym(module, "ObjectSetup"));
                if (!browserOverlayStateWasCaptured(module)) assert(PartyBoardOverlayStateCapture(module));
                assert(PartyBoardOverlayStateRestore(module));
                printf("Competitive module load passed: m%d imports resolved and state restored; ObjectSetup not executed.\n", id);
            }
#endif
#ifdef PARTYBOARD_BOARD_LOAD_AUDIT
            for (int id : {1, 2, 3, 5, 6, 10}) {
                char path[32];
                snprintf(path, sizeof(path), "/w%02dDll.wasm", id);
                void* module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
                if (!module) fprintf(stderr, "Board module load failed (%d): %s\n", id, dlerror());
                assert(module && dlsym(module, "ObjectSetup"));
                bool captured = false;
                for (auto handle : browserOverlayHandles) if (handle == module) captured = true;
                if (!captured) assert(PartyBoardOverlayStateCapture(module));
                assert(PartyBoardOverlayStateRestore(module));
                printf("Board module load passed: w%02d imports resolved and state restored; ObjectSetup not executed.\n", id);
            }
#endif
#ifdef PARTYBOARD_MINIGAME_LOAD_AUDIT
            for (int id = 416; id <= 424; ++id) {
                if (mgFrames && id == 419) continue;
                char path[32];
                snprintf(path, sizeof(path), "/m%dDll.wasm", id);
                void* module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
                if (!module) fprintf(stderr, "Minigame module load failed (%d): %s\n", id, dlerror());
                assert(module && dlsym(module, "ObjectSetup"));
                if (!browserOverlayStateWasCaptured(module)) assert(PartyBoardOverlayStateCapture(module));
                assert(PartyBoardOverlayStateRestore(module));
                printf("Minigame module load passed: m%d imports resolved and state restored; ObjectSetup not executed.\n", id);
            }
#endif
#ifdef PARTYBOARD_RESULT_LOAD_PROBE
            if (!resultFrames && !returnFrames) {
            void* results = dlopen("/resultDll.wasm", RTLD_NOW | RTLD_LOCAL);
            if (!results) fprintf(stderr, "Results module load failed: %s\n", dlerror());
            assert(results && dlsym(results, "ObjectSetup"));
            assert(PartyBoardOverlayStateCapture(results));
            assert(PartyBoardOverlayStateRestore(results));
            puts("Results module load passed: imports resolved and state restored; ObjectSetup not executed.");
            }
#endif
            puts("Watcher boot passed: real game loop loaded boot and cleanup reset its resident state.");
            if (watcherStart) puts("Watcher keyboard transition passed: browser Enter input reached mode-select through normal watcher.");
        } else if (argc > 3 && std::strcmp(argv[3], "--reload-save") == 0) {
            assert(HuCardMount(0) == CARD_RESULT_READY);
            CARDFileInfo restoredFile{};
            assert(HuCardOpen(0, SaveFileNameTbl[0], &restoredFile) == CARD_RESULT_READY);
            std::memset(saveBuf.buf, 0xa5, SAVE_BUF_SIZE);
            assert(HuCardRead(&restoredFile, saveBuf.buf, SAVE_BUF_SIZE, 0) == CARD_RESULT_READY);
            assert(HuCardClose(&restoredFile) == CARD_RESULT_READY);
            assert(SLCheckSumCheck());
            SLLoadGameStat();
            HuCardUnMount(0);
            puts("Fresh save reload passed: new runtime opened exported card, validated payload and loaded game stats.");
        } else {
        assert(!frames && omwatchproc && !omwatchproc->child);
        void* boot = dlopen("/bootDll.wasm", RTLD_NOW | RTLD_LOCAL);
        if (!boot) fprintf(stderr, "Boot setup load failed: %s\n", dlerror());
        assert(boot);
        assert(PartyBoardOverlayStateCapture(boot) == 1);
        auto setup = reinterpret_cast<void (*)(void)>(dlsym(boot, "ObjectSetup"));
        assert(setup);
        const bool menuSlot = argc > 3 && std::strcmp(argv[3], "--menu-slot") == 0;
        const bool menuChoose = menuSlot || (argc > 3 && std::strcmp(argv[3], "--menu-choose") == 0);
        const bool menuFrames = menuChoose || (argc > 3 && std::strcmp(argv[3], "--menu-frames") == 0);
        const bool transition = menuFrames || (argc > 3 && std::strcmp(argv[3], "--boot-transition") == 0);
        if (transition) omcurovl = DLL_bootDll;
        setup();
        assert(omwatchproc->child);
        assert(GlobalCounter == 0);
        // Processes retain overlay function pointers. Keep the module loaded
        // until this test page is destroyed. Restore is permitted only after
        // the actual cleanup path has removed its child processes below.
        puts("Boot setup passed: actual ObjectSetup returned and created an object-manager child; no processes scheduled.");
        const bool bootStart = transition || (argc > 3 && std::strcmp(argv[3], "--boot-start") == 0);
        const bool bootTitle = bootStart || (argc > 3 && std::strcmp(argv[3], "--render-boot-title") == 0);
        const bool bootAssets = bootTitle || (argc > 3 && std::strcmp(argv[3], "--render-boot-assets") == 0);
        const bool renderBoot = bootAssets || (argc > 3 && std::strcmp(argv[3], "--render-boot") == 0);
        if (renderBoot || (argc > 3 && std::strcmp(argv[3], "--schedule") == 0)) {
            // Setup was invoked manually in this probe. Keep the default
            // overlay watcher from attempting a second setup via its gate.
            HuPrcSetStat(omwatchproc, PROCESS_STAT_PAUSE);
            for (int tick=0; tick<3; ++tick) {
                HuPrcCall(1);
                printf("Boot scheduler tick returned: %d\n", tick+1);
            }
            assert(omwatchproc->child && WipeStatGet());
            assert(GlobalCounter == 0);
            puts("Boot scheduler passed: three real scheduler ticks returned; boot waits on its initial wipe, no game frames rendered.");
            if (renderBoot) {
                const float initialWipeTime = wipeData.time;
                const int frameCount = bootStart ? 1000 : bootTitle ? 900 : bootAssets ? 36 : 4;
                for (int frame=0; frame<frameCount; ++frame) {
                    aurora_update();
                    const u32 retrace = VIGetRetraceCount();
                    assert(aurora_begin_frame());
                    HuSysBeforeRender();
                    Hu3DPreProc();
                    HuPadRead();
                    if (bootStart && frame == 900) {
                        assert(omnextovl != DLL_modeseldll);
                        HuPadBtnDown[0] |= PAD_BUTTON_START;
                        puts("Boot probe: injecting one synthetic Start edge into game input.");
                    }
                    pfClsScr();
                    HuPrcCall(1);
                    MGSeqMain();
                    Hu3DExec();
                    WipeExecAlways();
                    pfDrawFonts();
                    HuSysDoneRender(retrace);
                    aurora_end_frame();
                    if (frame == frameCount-1) {
                        EM_ASM({ globalThis.bootProbeImage = document.getElementById('canvas').toDataURL('image/png'); });
                    }
                    frame_limiter();
                }
                if (bootStart) {
                    assert(omnextovl == DLL_modeseldll);
                    puts("Boot Start passed: synthetic input requested mode-select overlay; loading and physical input remain untested.");
                    if (transition) {
                        assert(omcurovl == DLL_NONE && !omwatchproc->child);
                        assert(PartyBoardOverlayStateRestore(boot) == 1);
                        puts("Boot private state restored after actual child cleanup.");
                        // Exercise repeated setup after a real teardown, while
                        // keeping the production watcher paused in this probe.
                        for (int visit=0; visit<2; ++visit) {
                            omcurovl = DLL_bootDll;
                            omovlevtno = 0;
                            setup();
                            assert(omwatchproc->child);
                            for (int tick=0; tick<3; ++tick) HuPrcCall(1);
                            for (int frame=0; frame<36; ++frame) {
                                aurora_update();
                                const u32 retrace = VIGetRetraceCount();
                                assert(aurora_begin_frame());
                                HuSysBeforeRender(); Hu3DPreProc(); HuPadRead(); pfClsScr();
                                HuPrcCall(1); MGSeqMain(); Hu3DExec(); WipeExecAlways(); pfDrawFonts();
                                HuSysDoneRender(retrace); aurora_end_frame(); frame_limiter();
                            }
                            omOvlKill(1);
                            assert(omcurovl == DLL_NONE && !omwatchproc->child);
                            assert(PartyBoardOverlayStateRestore(boot) == 1);
                            printf("Boot re-entry passed: visit %d scheduled/rendered and cleaned up.\n", visit+1);
                        }
                        void* menu = dlopen("/modeseldll.wasm", RTLD_NOW | RTLD_LOCAL);
                        if (!menu) fprintf(stderr, "Mode-select setup load failed: %s\n", dlerror());
                        assert(menu);
                        assert(PartyBoardOverlayStateCapture(menu) == 1);
                        auto menuSetup = reinterpret_cast<void (*)(void)>(dlsym(menu, "ObjectSetup"));
                        assert(menuSetup && menuSetup != setup);
                        omcurovl = DLL_modeseldll;
                        omovlevtno = omnextovlevtno;
                        menuSetup();
                        assert(omwatchproc->child);
                        puts("Mode-select setup passed: real boot cleanup removed its child; distinct menu setup created a new child, not scheduled.");
                        if (menuFrames) {
                            const bool onlineMenu = EM_ASM_INT({return globalThis.testOnlineMenu ? 1 : 0;});
                            const int menuFrameCount = onlineMenu ? 600 : menuSlot ? 720 : menuChoose ? 240 : 120;
                            for (int frame=0; frame<menuFrameCount; ++frame) {
                                if (onlineMenu && frame % 50 == 0) printf("Online menu probe: frame %d\n", frame);
                                aurora_update();
                                const u32 retrace = VIGetRetraceCount();
                                assert(aurora_begin_frame());
                                HuSysBeforeRender();
                                Hu3DPreProc();
                                HuPadRead();
                                if (onlineMenu) {
                                    if (frame == 330 || frame == 440) HuPadDStkRep[0] |= PAD_BUTTON_DOWN;
                                    if (frame == 350 || frame == 380 || frame == 470) HuPadBtnDown[0] |= PAD_BUTTON_A;
                                    if (frame == 410 || frame == 500 || frame == 530 || frame == 560) HuPadBtnDown[0] |= PAD_BUTTON_B;
                                }
                                if ((menuChoose && frame == 120) || (menuSlot && (frame == 240 || frame == 360))) {
                                    HuPadBtnDown[0] |= PAD_BUTTON_A;
                                    puts("Menu probe: injecting one synthetic A edge; card storage is temporary browser memory.");
                                }
                                pfClsScr();
                                HuPrcCall(1);
                                MGSeqMain();
                                Hu3DExec();
                                WipeExecAlways();
                                pfDrawFonts();
                                HuSysDoneRender(retrace);
                                aurora_end_frame();
                                if (onlineMenu && (frame == 365 || frame == 395 || frame == 455 || frame == 545)) {
                                    EM_ASM({
                                        if (!globalThis.onlineMenuImages) globalThis.onlineMenuImages = [];
                                        globalThis.onlineMenuImages.push(document.getElementById('canvas').toDataURL('image/png'));
                                    });
                                }
                                if (frame == menuFrameCount-1) {
                                    EM_ASM({ globalThis.bootProbeImage = document.getElementById('canvas').toDataURL('image/png'); });
                                }
                                frame_limiter();
                            }
                            assert(omcurovl == DLL_modeseldll && omwatchproc->child);
                            puts("Mode-select rendering passed: bounded process/render ticks completed; inspect capture for visible state.");
                            if (menuSlot) {
                                static SaveBufAll preservedSave;
                                std::memcpy(&preservedSave, &saveBuf, sizeof(saveBuf));
                                HuCardUnMount(0);
                                assert(HuCardMount(0) == CARD_RESULT_READY);
                                CARDFileInfo savedFile{};
                                assert(HuCardOpen(0, SaveFileNameTbl[0], &savedFile) == CARD_RESULT_READY);
                                std::memset(saveBuf.buf, 0xa5, SAVE_BUF_SIZE);
                                assert(HuCardRead(&savedFile, saveBuf.buf, SAVE_BUF_SIZE, 0) == CARD_RESULT_READY);
                                assert(HuCardClose(&savedFile) == CARD_RESULT_READY);
                                assert(SLCheckSumCheck());
                                saveBuf.buf[0] ^= 1;
                                assert(!SLCheckSumCheck());
                                saveBuf.buf[0] ^= 1;
                                assert(SLCheckSumCheck());
                                HuCardUnMount(0);
                                std::memcpy(&saveBuf, &preservedSave, sizeof(saveBuf));
                                puts("Browser save readback passed: reopened 16384-byte payload validates checksum; one-bit corruption rejected in RAM only.");
                                printf("Menu final state: overlay %d, wipe %u, time %.1f, duration %.1f\n", omcurovl, WipeStatGet(), wipeData.time, wipeData.duration);
                                puts("Menu slot probe completed: three synthetic A edges and 720 frames; inspect capture and temporary save files.");
                                omOvlKill(1);
                                assert(omcurovl == DLL_NONE && !omwatchproc->child);
                                assert(PartyBoardOverlayStateRestore(menu) == 1);
                                omcurovl = DLL_modeseldll;
                                omovlevtno = 0;
                                menuSetup();
                                assert(omwatchproc->child);
                                for (int frame=0; frame<120; ++frame) {
                                    aurora_update();
                                    const u32 retrace = VIGetRetraceCount();
                                    assert(aurora_begin_frame());
                                    HuSysBeforeRender(); Hu3DPreProc(); HuPadRead(); pfClsScr();
                                    HuPrcCall(1); MGSeqMain(); Hu3DExec(); WipeExecAlways(); pfDrawFonts();
                                    HuSysDoneRender(retrace); aurora_end_frame(); frame_limiter();
                                }
                                assert(omcurovl == DLL_modeseldll && omwatchproc->child);
                                omOvlKill(1);
                                assert(omcurovl == DLL_NONE && !omwatchproc->child);
                                assert(PartyBoardOverlayStateRestore(menu) == 1);
                                puts("Mode-select re-entry passed: saved card retained while overlay was reset, rendered and cleaned up again.");
                            }
                            else if (menuChoose) puts("Menu selection probe completed: one synthetic A edge and 240 frames; inspect capture to verify response.");
                        }
                    }
                } else if (bootTitle) {
                    assert(!WipeStatGet());
                    puts("Boot long probe passed: 900 bounded renderer frames; inspect capture for title state, movie playback remains stubbed.");
                } else if (bootAssets) {
                    assert(!WipeStatGet());
                    puts("Boot assets passed: 36 bounded renderer frames completed the opening wipe; full game loop not exercised.");
                } else {
                    assert(wipeData.time > initialWipeTime && WipeStatGet());
                    puts("Boot rendering passed: four bounded renderer frames advanced the actual boot wipe; full game loop not exercised.");
                }
            }
        }
        }
#endif
        for(int heap=0;heap<HEAP_MAX;++heap) {
            assert(HuMemHeapPtrGet(static_cast<HeapID>(heap)));
            auto* data=HuMemDirectMalloc(static_cast<HeapID>(heap),128);
            assert(data); std::memset(data,0x5a,128); HuMemDirectFree(data);
        }
        puts("Game system initialization passed: game_main returned and all five heaps allocate/free.");
#ifdef PARTYBOARD_BOOT_SETUP_PROBE
        const auto spriteBytes = HuMemUsedMallocSizeGet(HEAP_DATA);
        const auto spriteBlocks = HuMemUsedMallocBlockGet(HEAP_DATA);
        for (int iteration=0; iteration<64; ++iteration) {
            auto* glyph=HuSprAnimMake(64,64,ANIM_BMP_RGBA8);
            assert(glyph);
            glyph->bmp->data=HuMemDirectMalloc(HEAP_DATA,64*64*4);
            assert(glyph->bmp->data);
            std::memset(glyph->bmp->data,0xff,64*64*4);
            HuSprTexLoad(glyph,0,0,GX_CLAMP,GX_CLAMP,GX_LINEAR);
            HuSprTexLoad(glyph,0,7,GX_CLAMP,GX_CLAMP,GX_LINEAR);
            assert(glyph->bmp->texData[0].tex_initialized);
            assert(glyph->bmp->texData[7].tex_initialized);
            HuSprAnimKill(glyph);
            assert(HuMemUsedMallocSizeGet(HEAP_DATA)==spriteBytes);
            assert(HuMemUsedMallocBlockGet(HEAP_DATA)==spriteBlocks);
        }
        puts("Dynamic sprite cleanup passed: 64 RGBA allocations with cached texture slots 0/7 return heap bytes and blocks to baseline.");
#endif
        aurora_dvd_close();
    } else assert(disc_contract_main(argc,argv)==0);
    render(true);
    aurora_shutdown();
    puts("Disc renderer passed: red before async disc reads, green after reads, clean shutdown.");
}

// Diagnostic setup bridge. Caller must set testExternalEngine before invoking;
// game initialization/resume remain separately owned by the lifecycle adapter.
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardTestPrepareMountedPlatform(void) {
    if (!EM_ASM_INT({return globalThis.testExternalEngine ? 1 : 0;})) return -1;
    char program[] = "downloaded-platform-probe";
    char path[] = "/assets/verified-disc.ciso";
    char startup[] = "--startup-game";
    char watcher[] = "--watcher-start";
    char* arguments[] = {program, path, startup, watcher, nullptr};
    return main(4, arguments);
}
