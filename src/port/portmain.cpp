// Credits: TwilitRealm

#include "imgui/ImGuiEngine.hpp"
#include "iso_validate.hpp"
#include "ui/ui.hpp"

#include "partyboard_version.h"
#include "ui/menu_bar.hpp"
#include "ui/overlay.hpp"
#include "ui/prelaunch.hpp"
#include "ui/preset.hpp"
#include "ui/settings.hpp"
#include <aurora/rmlui.hpp>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_timer.h>
#include <fstream>
#include <functional>
#include <array>
#include <map>
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.h>
#include <dolphin/gx/GXAurora.h>
#include <dolphin/os.h>
#include <dolphin/pad.h>
#include <dolphin/vi.h>
#include <game/disp.h>
#include <port/config.hpp>
#include <port/dolassets.h>
#include <port/main.h>
#include <port/input_trace.h>
#include <port/net_diagnostic.h>
#include <port/native_prepared_start.hpp>
#include <port/native_renderer_preparation.hpp>
#include <port/native_pipeline_seed.hpp>
#include <port/native_bundled_pipeline_seed.hpp>
#include <port/native_crash_dump.hpp>
#include <port/native_menu_handoff.hpp>
#include <port/native_engine_input_io.hpp>
#include <port/native_input_timing_json.hpp>
extern "C" int HuAudPortSynthMetric(int metric);
#include <port/native_full_match_progress.hpp>
#include <port/native_diagnostic_pad.hpp>
#ifdef PARTYBOARD_NATIVE_RTC
#include "ui/native_host_lobby.hpp"
#include "ui/native_join_lobby.hpp"
#include "ui/native_join_preparation.hpp"
#include "ui/native_join_menu_probe.hpp"
#include "ui/native_guest_recovery_window.hpp"
#include <port/native_guest_staging_file.hpp>
#include <port/native_guest_engine_preparation.hpp>
#include <port/native_guest_prepared_lobby.hpp>
#include <port/native_guest_game_input.hpp>
#include <port/native_external_join_probe.hpp>
#include "ui/native_recovery_window.hpp"
#include <port/native_restart_intent.hpp>
#include <port/native_probe_pipe.hpp>
#include <port/native_rtc_host_input.hpp>
#include <port/native_rtc_engine_probe.hpp>
#include <port/native_session_package.hpp>
#include <port/asset_transfer.hpp>
#endif
#include <charconv>
#include <port/settings.h>
#include <port/port_version.h>

#include <aurora/dvd.h>
#include <aurora/lib/logging.hpp>
#include <game/card.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" int game_main();
extern "C" int PartyBoardNativeRestartPending(){return PartyBoard_RestartRequested?1:0;}
// Assigned by the online frontend once its stopped-game handoff is installed.
static int nativeRestartOnlineAction=-1;
extern "C" int PartyBoardNativeRestartOnlineAction(){return nativeRestartOnlineAction;}
extern "C" {
#include <game/object.h>
#include <game/audio.h>
#include <game/window.h>
#include <game/sprite.h>
#include <game/board/view.h>
}

static int NativeFullMatchBoard() {
    const char* requested=std::getenv("PARTYBOARD_NATIVE_FULL_MATCH_BOARD");
    if(!requested)return 0;
    const std::string_view text(requested);
    int board=-1;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),board);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||board<0||board>5)
        throw std::invalid_argument("Full-match main board index must be 0 through 5");
    return board;
}

static int NativeFullMatchTurns() {
    const char* requested=std::getenv("PARTYBOARD_NATIVE_FULL_MATCH_TURNS");
    if(!requested)return 20;
    const std::string_view text(requested);
    int turns=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),turns);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||turns<5||turns>50||turns%5!=0)
        throw std::invalid_argument("Full-match turn count must be 5 through 50 in five-turn steps");
    return turns;
}

static void EmitNativeFullMatchProgress(const char* prefix, uint64_t& nextFrame) {
    const auto frame=PartyBoard_GameFrameCount();
    if(frame<nextFrame)return;
    nextFrame=uint64_t(frame)+600;
    printf("%s: %s\n",prefix,nlohmann::ordered_json({{"type","full-match-progress"},{"frame",frame},
        {"turn",PartyBoard_GameProgress(0)},{"maxTurn",PartyBoard_GameProgress(1)},
        {"board",PartyBoard_GameProgress(2)},{"player",PartyBoard_GameProgress(3)},
        {"overlay",PartyBoard_GameProgress(4)},{"event",PartyBoard_GameProgress(5)}}).dump().c_str());
    fflush(stdout);
}

#ifdef PARTYBOARD_NATIVE_RTC
static nlohmann::ordered_json NativeMatchObservation(const nlohmann::ordered_json& request,
        const partyboard::net::NativeInputObservations& trace) {
    const auto id=request.at("requestId").get<std::string>();
    if(id.empty()||id.size()>64)throw std::invalid_argument("Bounded observation request id required");
    PartyBoardInputState state{};
    if(!PartyBoardCaptureInputState(&state))throw std::runtime_error("Native match state unavailable");
    nlohmann::ordered_json samples=nlohmann::ordered_json::array();
    nlohmann::ordered_json globalFields=nlohmann::ordered_json::array();
    nlohmann::ordered_json lotteryFields=nlohmann::ordered_json::array();
    nlohmann::ordered_json sequenceFields=nlohmann::ordered_json::array();
    nlohmann::ordered_json sequenceTrace=nlohmann::ordered_json::array();
    for(int row=0;row<PartyBoard_GameSequenceTrace(-1,0);++row){
        nlohmann::ordered_json record=nlohmann::ordered_json::array();
        for(int field=0;field<PartyBoard_GameSequenceTrace(0,-1);++field)
            record.push_back(PartyBoard_GameSequenceTrace(row,field));
        sequenceTrace.push_back(std::move(record));
    }
    for(int field=0;field<PartyBoard_GameSequenceField(-1);++field)
        sequenceFields.push_back(PartyBoard_GameSequenceField(field));
    for(int field=0;field<10;++field)lotteryFields.push_back(PartyBoard_GameLotteryField(field));
    for(int field=0;field<PartyBoard_GameGlobalCheckpointField(-1);++field)
        globalFields.push_back(PartyBoard_GameGlobalCheckpointField(field));
    for(const auto& sample:trace.entries())samples.push_back({{"frame",std::to_string(sample.state.frame)},
        {"checkpoint",sample.state.checkpoint},{"frand",sample.state.frand},{"rand8",sample.state.rand8},{"pads",sample.pads}});
    return {{"type","full-match-observation"},{"requestId",id},{"frame",state.frame},
        {"currentInputState",{{"frame",std::to_string(state.frame)},{"checkpoint",state.checkpoint},{"frand",state.frand},{"rand8",state.rand8}}},
        {"checkpointComponents",{PartyBoard_GameCheckpointComponent(0),PartyBoard_GameCheckpointComponent(1),PartyBoard_GameCheckpointComponent(2),PartyBoard_GameCheckpointComponent(3)}},
        {"globalCheckpointFields",globalFields},
        {"itemEventStage",PartyBoard_GameItemEventStage()},
        {"lotteryFields",lotteryFields},
        {"sequenceFields",sequenceFields},
        {"sequenceTrace",sequenceTrace},
        {"progress",{{"turn",PartyBoard_GameProgress(0)},{"maxTurn",PartyBoard_GameProgress(1)},
        {"board",PartyBoard_GameProgress(2)},{"player",PartyBoard_GameProgress(3)},
        {"overlay",PartyBoard_GameProgress(4)},{"event",PartyBoard_GameProgress(5)},
        {"menuBoard",PartyBoard_GameProgress(6)},{"menuPort",PartyBoard_GameProgress(7)},
        {"interactionStage",PartyBoard_GameProgress(8)},
        {"partySetupTurns",PartyBoard_GameProgress(9)},{"partySetupRow",PartyBoard_GameProgress(10)},
        {"partySetupPort",PartyBoard_GameProgress(11)},
        {"mapView",PartyBoardNativeMapViewActive()!=0},{"mapViewPort",PartyBoardNativeMapViewPort()}}},{"observations",samples}};
}
#endif

static std::map<std::string,std::array<unsigned char,4096>,std::less<>> nativeKeyTextures;
static void LoadNativeKeyboardGlyphs() {
    std::vector<std::string> labels={"Enter","Space","Esc","Tab"};
    for(char c='A';c<='Z';++c)labels.emplace_back(1,c);
    for(char c='0';c<='9';++c)labels.emplace_back(1,c);
    for(const char* family:{"xbox","playstation","switch","generic"})
        for(const char* button:{"south","east","west","north","l1","r1","l2","r2","start","select"})labels.push_back(std::string(family)+"-"+button);
    for(const auto& label:labels) {
        const auto path=partyboard::ui::resource_path(std::filesystem::path("control-glyphs")/("key-"+label+".rgba8"));
        std::ifstream source(path,std::ios::binary);
        std::array<unsigned char,4096> pixels{};
        if(source.read(reinterpret_cast<char*>(pixels.data()),pixels.size()) && source.peek()==std::char_traits<char>::eof())nativeKeyTextures.emplace(label,pixels);
    }
}
static ANIMDATA* NativeKeyboardGlyphForPort(s16 entry, s16 port) {
    port=PartyBoardNativeGlyphLane(port);
    if(port<0)return nullptr;
    // Mixed keyboard/controller ports retain originals until active-device
    // tracking is shared with the engine. Controller labels follow mappings.
    const auto controller=PADGetIndexForPort(port);
    PADButton action;
    switch(entry) {
        case 2:action=PAD_BUTTON_A;break; case 3:action=PAD_BUTTON_B;break;
        case 4:action=PAD_BUTTON_X;break; case 5:action=PAD_BUTTON_Y;break;
        case 6:action=PAD_TRIGGER_R;break; case 8:action=PAD_TRIGGER_L;break;
        case 11:action=PAD_TRIGGER_Z;break; case 12:action=PAD_BUTTON_MENU;break; default:return nullptr;
    }
    u32 count=0;
    auto* bindings=PADGetKeyButtonBindings(port,&count);
    if(controller>=0 && bindings)return nullptr; // Mixed devices need active-input tracking.
    std::string label;
    for(u32 i=0;bindings && i<count;++i) {
        if(bindings[i].padButton==action) {
            label=SDL_GetScancodeName(static_cast<SDL_Scancode>(bindings[i].scancode));break;
        }
    }
    if(label=="Return")label="Enter";
    if(label=="Escape")label="Esc";
    if(controller>=0) {
        const char* family=nullptr;
        switch(PADGetControllerType(port)) {
            case PAD_TYPE_XBOX360:case PAD_TYPE_XBOXONE:family="xbox";break;
            case PAD_TYPE_PS3:case PAD_TYPE_PS4:case PAD_TYPE_PS5:family="playstation";break;
            case PAD_TYPE_SWITCH_PROCON:family="switch";break;
            default:return nullptr; // Preserve originals for unverified layouts.
        }
        const auto* mappings=PADGetButtonMappings(port,&count);
        for(u32 i=0;mappings && i<count;++i)if(mappings[i].padButton==action) {
            if(mappings[i].nativeButton==PAD_NATIVE_BUTTON_INVALID)continue;
            const char* button=nullptr;
            switch(mappings[i].nativeButton) {
                case SDL_GAMEPAD_BUTTON_SOUTH:button="south";break;
                case SDL_GAMEPAD_BUTTON_EAST:button="east";break;
                case SDL_GAMEPAD_BUTTON_WEST:button="west";break;
                case SDL_GAMEPAD_BUTTON_NORTH:button="north";break;
                case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:button="l1";break;
                case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:button="r1";break;
                case SDL_GAMEPAD_BUTTON_START:button="start";break;
                case SDL_GAMEPAD_BUTTON_BACK:button="select";break;
                default:return nullptr;
            }
            label=std::string(family)+"-"+button;break;
        }
        if(label.empty() && (action==PAD_TRIGGER_L || action==PAD_TRIGGER_R)) {
            // Match PADRead: axes generate digital L/R only when emulation is
            // enabled and no explicit digital binding has taken precedence.
            const auto* deadZones=PADGetDeadZones(port);
            if(!deadZones || !deadZones->emulateTriggers)return nullptr;
            const auto* axes=PADGetAxisMappings(port,&count);
            const auto wanted=action==PAD_TRIGGER_L?PAD_AXIS_TRIGGER_L:PAD_AXIS_TRIGGER_R;
            for(u32 i=0;axes && i<count;++i)if(axes[i].padAxis==wanted) {
                if(axes[i].nativeButton!=-1 || axes[i].nativeAxis.sign!=AXIS_SIGN_POSITIVE)return nullptr;
                const char* button=axes[i].nativeAxis.nativeAxis==SDL_GAMEPAD_AXIS_LEFT_TRIGGER?"l2":
                    axes[i].nativeAxis.nativeAxis==SDL_GAMEPAD_AXIS_RIGHT_TRIGGER?"r2":nullptr;
                if(!button)return nullptr;
                label=std::string(family)+"-"+button;break;
            }
        }
    }
    // Whitelist filenames; unknown/localized names retain original artwork.
    const bool single=label.size()==1 && ((label[0]>='A'&&label[0]<='Z')||(label[0]>='0'&&label[0]<='9'));
    if(controller<0 && !single && label!="Enter" && label!="Space" && label!="Esc" && label!="Tab")return nullptr;
    const auto found=nativeKeyTextures.find(label);
    if(found==nativeKeyTextures.end())return nullptr;
    const auto& pixels=found->second;
    auto* glyph=HuSprAnimMake(32,32,ANIM_BMP_RGBA8);
    if(!glyph)return nullptr;
    glyph->bmp->data=HuMemDirectMalloc(HEAP_DATA,pixels.size());
    if(!glyph->bmp->data) {HuSprAnimKill(glyph);return nullptr;}
    std::memcpy(glyph->bmp->data,pixels.data(),pixels.size());
    HuSprAnimLock(glyph);
    static unsigned reported=0;
    if(!(reported & (1u<<entry))) {
        fprintf(stderr,"Native keyboard glyph installed: bank %d uses %s\n",entry,label.c_str());
        reported|=1u<<entry;
    }
    return glyph;
}

static ANIMDATA* NativeKeyboardGlyph(s16 entry) {
    return NativeKeyboardGlyphForPort(entry, 0);
}

using namespace std::string_literals;
using namespace std::string_view_literals;

static bool mainCalled = false;

AuroraInfo auroraInfo;
std::filesystem::path PartyBoard_ConfigPath;

aurora::Module PartyBoardMainLog("partyboard::main");

static u8 selectedLanguage;

u8 OSGetLanguage() {
    return selectedLanguage;
}

static void LanguageInit() {
    // Keep language at 0 (English) if not on a PAL disc.
    // Doubt this matters, but avoid funky shit.
    if (!partyboard::version::isRegionPal()) {
        return;
    }

    // Cache this to avoid funky shenanigans.
    selectedLanguage = static_cast<u8>(partyboard::getSettings().game.language.getValue());
}

// Keep the UI event/render loop independent of game launch state: a stopped
// engine's lobby must still service window/controller events and its owners.
static bool nativeUILoop(const std::function<bool()>& keepRunning) {
    while (keepRunning()) {
        const AuroraEvent* event = aurora_update();
        while (event != nullptr && event->type != AURORA_NONE) {
            switch (event->type) {
                case AURORA_SDL_EVENT:
                    partyboard::ui::handle_event(event->sdl);
                // partyboard::g_imguiConsole.HandleSDLEvent(event->sdl);
                break;
                case AURORA_DISPLAY_SCALE_CHANGED:
                    // TODO PC
                    // partyboard::ImGuiEngine_Initialize(event->windowSize.scale);
                break;
                case AURORA_EXIT:
                    return false;
            }

            event++;
        }

        if (!aurora_begin_frame()) {
            PartyBoardMainLog.debug("aurora_begin_frame returned false, skipping draw this frame");
            continue;
        }

        partyboard::ui::update();

        // partyboard::g_imguiConsole.PreDraw();
        // partyboard::g_imguiConsole.PostDraw();

        aurora_end_frame();
    }

    return true;
}

bool launchUILoop() {
    return nativeUILoop([] { return PartyBoard_IsRunning && !PartyBoard_IsGameLaunched; }) && PartyBoard_IsRunning;
}

void aurora_log_callback(AuroraLogLevel level, const char* module, const char *message, unsigned int len)
{
    const char *levelStr = "??";
    FILE *out = stdout;
    switch (level) {
        case LOG_DEBUG:
            levelStr = "DEBUG";
            break;
        case LOG_INFO:
            levelStr = "INFO";
            break;
        case LOG_WARNING:
            levelStr = "WARNING";
            break;
        case LOG_ERROR:
            levelStr = "ERROR";
            out = stderr;
            break;
        case LOG_FATAL:
            levelStr = "FATAL";
            out = stderr;
            break;
    }
    fprintf(out, "[%s | %s] %s\n", levelStr, module, message);
    if (level == LOG_FATAL) {
        fflush(out);
        abort();
    }
}

static bool try_parse_backend(std::string_view backend, AuroraBackend& outBackend) {
    if (backend == "auto") {
        outBackend = BACKEND_AUTO;
        return true;
    }
    if (backend == "d3d11") {
        outBackend = BACKEND_D3D11;
        return true;
    }
    if (backend == "d3d12") {
        outBackend = BACKEND_D3D12;
        return true;
    }
    if (backend == "metal") {
        outBackend = BACKEND_METAL;
        return true;
    }
    if (backend == "vulkan") {
        outBackend = BACKEND_VULKAN;
        return true;
    }
    if (backend == "opengl") {
        outBackend = BACKEND_OPENGL;
        return true;
    }
    if (backend == "opengles") {
        outBackend = BACKEND_OPENGLES;
        return true;
    }
    if (backend == "webgpu") {
        outBackend = BACKEND_WEBGPU;
        return true;
    }
    if (backend == "null") {
        outBackend = BACKEND_NULL;
        return true;
    }

    return false;
}

static std::string_view backend_name(AuroraBackend backend) {
    switch (backend) {
        default:
            return "Auto"sv;
        case BACKEND_D3D12:
            return "D3D12"sv;
        case BACKEND_D3D11:
            return "D3D11"sv;
        case BACKEND_METAL:
            return "Metal"sv;
        case BACKEND_VULKAN:
            return "Vulkan"sv;
        case BACKEND_OPENGL:
            return "OpenGL"sv;
        case BACKEND_OPENGLES:
            return "OpenGL ES"sv;
        case BACKEND_WEBGPU:
            return "WebGPU"sv;
        case BACKEND_NULL:
            return "Null"sv;
    }
}

static bool IsBackendAvailable(AuroraBackend backend) {
    if (backend == BACKEND_AUTO) {
        return true;
    }

    size_t availableBackendCount = 0;
    const AuroraBackend* availableBackends = aurora_get_available_backends(&availableBackendCount);
    for (size_t i = 0; i < availableBackendCount; ++i) {
        if (availableBackends[i] == backend) {
            return true;
        }
    }

    return false;
}

static AuroraBackend ResolveDesiredBackend() {
    AuroraBackend desiredBackend = BACKEND_AUTO;

    if (!try_parse_backend(
                   static_cast<const std::string&>(partyboard::getSettings().backend.graphicsBackend),
                   desiredBackend))
    {
        PartyBoardMainLog.warn("Unknown configured backend '{}', falling back to Auto",
                     static_cast<const std::string&>(partyboard::getSettings().backend.graphicsBackend));
        desiredBackend = BACKEND_AUTO;
    }

    if (!IsBackendAvailable(desiredBackend)) {
        PartyBoardMainLog.warn("Requested backend '{}' is unavailable, falling back to Auto",
                     backend_name(desiredBackend));
        desiredBackend = BACKEND_AUTO;
    }

    return desiredBackend;
}


static void migrate_directory(const std::filesystem::path& from, const std::filesystem::path& to) {
    std::error_code ec;
    std::filesystem::create_directories(to, ec);
    if (ec) {
        return;
    }

    for (std::filesystem::recursive_directory_iterator it(
             from, std::filesystem::directory_options::skip_permission_denied, ec);
        it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec) {
            return;
        }

        const auto relativePath = std::filesystem::relative(it->path(), from, ec);
        if (ec) {
            return;
        }

        const auto targetPath = to / relativePath;
        if (it->is_directory(ec)) {
            std::filesystem::create_directories(targetPath, ec);
            if (ec) {
                return;
            }
        } else if (it->is_regular_file(ec) && !std::filesystem::exists(targetPath, ec)) {
            std::filesystem::create_directories(targetPath.parent_path(), ec);
            if (ec) {
                return;
            }
            std::filesystem::copy_file(
                it->path(), targetPath, std::filesystem::copy_options::skip_existing, ec);
            if (ec) {
                return;
            }
        }
    }
}

static std::filesystem::path calculate_config_path() {
#ifdef __APPLE__
#if TARGET_OS_IOS && !TARGET_OS_TV
    const char* documentsPath = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    if (!documentsPath) {
        PartyBoardMainLog.error("Unable to get iOS Documents path: {}", SDL_GetError());
    }

    std::filesystem::path configPath = reinterpret_cast<const char8_t*>(documentsPath);

    char* oldPrefPath = SDL_GetPrefPath("MarioPartyRD", "Party Board");
    if (oldPrefPath) {
        const std::filesystem::path oldConfigPath = reinterpret_cast<const char8_t*>(oldPrefPath);
        SDL_free(oldPrefPath);

        std::error_code ec;
        if (oldConfigPath != configPath && std::filesystem::exists(oldConfigPath, ec)) {
            migrate_directory(oldConfigPath, configPath);
        }
    }

    return configPath;
#endif
#endif

    const auto result = SDL_GetPrefPath("MarioPartyRD", "Party Board");
    if (!result) {
        PartyBoardMainLog.error("Unable to get PrefPath: {}", SDL_GetError());
    }

    return reinterpret_cast<const char8_t*>(result);
}

static void EnsureInitialPipelineCache(const std::filesystem::path& configDir) {
    if (configDir.empty()) {
        return;
    }

    const std::filesystem::path pipelineCachePath = configDir / "pipeline_cache.db";
    if (std::filesystem::exists(pipelineCachePath)) {
        return;
    }

    const char* basePath = SDL_GetBasePath();
    if (basePath == nullptr) {
        PartyBoardMainLog.error("Unable to resolve base path while seeding pipeline cache: {}", SDL_GetError());
        return;
    }

    const std::filesystem::path initialPipelineCachePath =
        std::filesystem::path(basePath) / "initial_pipeline_cache.db";
    if (!std::filesystem::exists(initialPipelineCachePath)) {
        PartyBoardMainLog.error("No bundled initial pipeline cache found at '{}'", initialPipelineCachePath.string());
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(configDir, ec);
    if (ec) {
        PartyBoardMainLog.error("Failed to create config directory '{}' for pipeline cache: {}",
                     configDir.string(), ec.message());
        return;
    }

    std::filesystem::copy_file(initialPipelineCachePath, pipelineCachePath, std::filesystem::copy_options::none, ec);
    if (ec) {
        PartyBoardMainLog.error("Failed to seed pipeline cache from '{}' to '{}': {}",
                     initialPipelineCachePath.string(), pipelineCachePath.string(), ec.message());
        return;
    }

    PartyBoardMainLog.info("Seeded pipeline cache from '{}'", initialPipelineCachePath.string());
}

static constexpr PADDefaultMapping defaultPadMapping = {
    .buttons = {
        {SDL_GAMEPAD_BUTTON_SOUTH, PAD_BUTTON_A},
        {SDL_GAMEPAD_BUTTON_EAST, PAD_BUTTON_B},
        {SDL_GAMEPAD_BUTTON_WEST, PAD_BUTTON_X},
        {SDL_GAMEPAD_BUTTON_NORTH, PAD_BUTTON_Y},
        {SDL_GAMEPAD_BUTTON_START, PAD_BUTTON_START},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, PAD_TRIGGER_Z},
        {PAD_NATIVE_BUTTON_INVALID, PAD_TRIGGER_L},
        {PAD_NATIVE_BUTTON_INVALID, PAD_TRIGGER_R},
        {SDL_GAMEPAD_BUTTON_DPAD_UP, PAD_BUTTON_UP},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, PAD_BUTTON_DOWN},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, PAD_BUTTON_LEFT},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, PAD_BUTTON_RIGHT},
    },
    .axes = {
        {{SDL_GAMEPAD_AXIS_LEFTX, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_X_POS},
        {{SDL_GAMEPAD_AXIS_LEFTX, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_X_NEG},
        // SDL's gamepad y-axis is inverted from GC's
        {{SDL_GAMEPAD_AXIS_LEFTY, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_Y_POS},
        {{SDL_GAMEPAD_AXIS_LEFTY, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_Y_NEG},
        {{SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_X_POS},
        {{SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_X_NEG},
        // see above
        {{SDL_GAMEPAD_AXIS_RIGHTY, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_Y_POS},
        {{SDL_GAMEPAD_AXIS_RIGHTY, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_Y_NEG},
        {{SDL_GAMEPAD_AXIS_LEFT_TRIGGER, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_TRIGGER_L},
        {{SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_TRIGGER_R},
    },
};

extern "C" int port_main(int argc, char* argv[]) {
    if (!partyboard::installNativeCrashCapture()) {
        fprintf(stderr, "Unable to install requested native crash capture\n");
        return 2;
    }
    // On iOS, when connected to an external monitor, SDLUIKitSceneDelegate scene:willConnectToSession:
    // can call our main function again. Explicitly guard against this reinitialization.
    if (mainCalled) {
        return 0;
    }
    mainCalled = true;

    // Set controller-driver defaults before Aurora initializes SDL input.
    // Normal-priority hints preserve explicit SDL environment overrides.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "1");
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "1");
#endif

    partyboard::registerSettings();
    partyboard::config::FinishRegistration();

    // Resolve an explicit profile before loading any settings or initializing
    // Aurora, so saves, bindings and caches use the same isolated directory.
    std::string userDirectory;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--user-dir") {
            if (!userDirectory.empty() || i + 1 >= argc || !argv[i + 1][0] ||
                std::string_view(argv[i + 1]).starts_with("--")) {
                fprintf(stderr, "Specify one --user-dir directory\n");
                return 2;
            }
            userDirectory = argv[++i];
        }
    }
    try {
        PartyBoard_ConfigPath = userDirectory.empty() ? calculate_config_path() :
            std::filesystem::absolute(std::filesystem::u8path(userDirectory));
        std::filesystem::create_directories(PartyBoard_ConfigPath);
        if (!std::filesystem::is_directory(PartyBoard_ConfigPath)) {
            fprintf(stderr, "Profile path is not a directory\n");
            return 2;
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "Cannot initialize profile directory: %s\n", e.what());
        return 2;
    }

    partyboard::config::LoadFromUserPreferences();
    bool keyboardPreset = false;
    std::string virtualGlyphController;
    std::string tracePath;
    std::string inputScript;
    bool traceReplay = false;
    bool traceFast = false;
    bool alignStartClock = false;
    bool prepareOnlineEngine = false;
    bool probeJoinLobby=false;
    bool probeJoinConnected=false;
    bool probeJoinPrepared=false;
    bool probeJoinStartup=false;
    bool probeJoinMenu=false;
    bool probeAudioSettings=false;
    const bool automaticLobbyTest=std::getenv("PARTYBOARD_AUTOMATIC_LOBBY_TEST")&&std::string_view(std::getenv("PARTYBOARD_AUTOMATIC_LOBBY_TEST"))=="1";
    bool automateHostRecovery=false;
    bool returnedHostMenuAcceptance=false;
    bool automateJoinRecovery=false;
    bool returnedJoinMenuAcceptance=false;
    bool probeJoinExternal=false;
    bool probeHostLobby = false; bool probeHostPreparation = false; bool probeHostInvitations=false; bool probeHostConnected=false; bool probeHostGameplay=false;
    std::string lobbyPackagePath;
    bool probeHostHandoff = false;
    bool probeJoinHandoff = false;
    int onlineStartupAction=-1;
    bool onlineSessionEndedNotice=false;
#if defined(PARTYBOARD_NATIVE_RTC) && defined(_WIN32)
    try{
        if(const auto intent=partyboard::net::consumeNativeRestartIntent())onlineStartupAction=*intent;
        onlineSessionEndedNotice=partyboard::net::consumeNativeSessionEndedNotice();
        if(onlineSessionEndedNotice&&onlineStartupAction<0)throw std::invalid_argument("Session-ended notice requires an online menu restart");
    }
    catch(const std::exception& error){fprintf(stderr,"%s\n",error.what());return 2;}
#endif
    std::string measuredSessionPath;
    u32 probeOnlineFrames = 0;
    bool probeOnlineButtons = false;
    int probeOnlineMenu = -1;
    unsigned probeRtcFrames = 0;
    bool traceAfterInit = false;
    u32 simulationSeed = 1;
    bool seedSpecified = false;
    u32 recordSamples = 0;
    int netRole = 0;
    unsigned netPort = 0;
    std::string netConfig;
    // A local launcher can select a disc without changing the application's
    // default path in source code. The existing prelaunch UI still verifies it.
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--user-dir") {
            ++i; // Already validated and applied before loading settings.
        } else if (std::string_view(argv[i]) == "--disc") {
            if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                fprintf(stderr, "--disc requires a game image path\n");
                return 2;
            }
            partyboard::getSettings().backend.isoPath.setValue(std::string(argv[++i]));
            partyboard::getSettings().backend.isoVerification.setValue(partyboard::DiscVerificationState::Unknown);
        } else if (std::string_view(argv[i]) == "--keyboard") {
            keyboardPreset = true;
        } else if (std::string_view(argv[i]) == "--virtual-glyph-controller") {
            if(i+1>=argc)return 2;
            virtualGlyphController=argv[++i];
            if(virtualGlyphController!="xboxone" && virtualGlyphController!="ps5" && virtualGlyphController!="switchpro")return 2;
        } else if (std::string_view(argv[i]) == "--record-input" ||
                   std::string_view(argv[i]) == "--replay-input") {
            if (!tracePath.empty() || i + 1 >= argc || !argv[i + 1][0]) {
                fprintf(stderr, "Specify one --record-input or --replay-input path\n");
                return 2;
            }
            traceReplay = std::string_view(argv[i]) == "--replay-input";
            tracePath = argv[++i];
        } else if (std::string_view(argv[i]) == "--net-diagnostic-listen" ||
                   std::string_view(argv[i]) == "--net-diagnostic-connect") {
            if (netRole || i + 1 >= argc) {
                fprintf(stderr, "Specify one diagnostic role and port\n"); return 2;
            }
            netRole = std::string_view(argv[i]) == "--net-diagnostic-listen" ? 1 : 2;
            const std::string_view value(argv[++i]);
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), netPort);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                netPort > 65535 || (netRole == 2 && !netPort)) {
                fprintf(stderr, "Invalid diagnostic port\n"); return 2;
            }
        } else if (std::string_view(argv[i]) == "--net-diagnostic-config") {
            if (!netConfig.empty() || i + 1 >= argc || !argv[i + 1][0]) {
                fprintf(stderr, "Specify one diagnostic config file\n"); return 2;
            }
            netConfig = argv[++i];
        } else if (std::string_view(argv[i]) == "--trace-after-init") {
            traceAfterInit = true;
        } else if (std::string_view(argv[i]) == "--align-start-clock") {
            alignStartClock = true;
        } else if (std::string_view(argv[i]) == "--prepare-online-engine") {
            prepareOnlineEngine = true;
        } else if(std::string_view(argv[i])=="--probe-join-external"){
#ifdef PARTYBOARD_NATIVE_RTC
            probeJoinExternal=true;
#else
            return 2;
#endif
        } else if(std::string_view(argv[i])=="--probe-join-lobby"||std::string_view(argv[i])=="--probe-join-connected"||std::string_view(argv[i])=="--probe-join-prepared"){
#ifdef PARTYBOARD_NATIVE_RTC
            probeJoinLobby=true;probeJoinPrepared=std::string_view(argv[i])=="--probe-join-prepared";probeJoinConnected=probeJoinPrepared||std::string_view(argv[i])=="--probe-join-connected";
#else
            return 2;
#endif
        } else if ((std::string_view(argv[i]) == "--probe-host-lobby" || std::string_view(argv[i]) == "--probe-host-preparation" || std::string_view(argv[i]) == "--probe-host-invitations" || std::string_view(argv[i]) == "--probe-host-connected" || std::string_view(argv[i]) == "--probe-host-gameplay")) {
#ifdef PARTYBOARD_NATIVE_RTC
            probeHostLobby = true; probeHostGameplay=std::string_view(argv[i])=="--probe-host-gameplay"; probeHostConnected=probeHostGameplay||std::string_view(argv[i])=="--probe-host-connected"; probeHostInvitations=probeHostConnected||std::string_view(argv[i])=="--probe-host-invitations"; probeHostPreparation = probeHostInvitations||std::string_view(argv[i]) == "--probe-host-preparation";
#else
            fprintf(stderr,"Native RTC lobby unavailable in this build\n");return 2;
#endif
        } else if(std::string_view(argv[i])=="--probe-audio-settings"){
            probeAudioSettings=true;
        } else if(std::string_view(argv[i])=="--probe-join-menu"){
            probeJoinMenu=true;onlineStartupAction=1;
        } else if(std::string_view(argv[i])=="--probe-join-startup"){
            probeJoinStartup=true;onlineStartupAction=1;
        } else if(std::string_view(argv[i])=="--native-session-package"){
            if(!lobbyPackagePath.empty()||i+1>=argc||!argv[i+1][0])return 2;
            lobbyPackagePath=argv[++i];
        } else if (std::string_view(argv[i]) == "--probe-join-handoff") {
#if defined(PARTYBOARD_NATIVE_RTC) && defined(_WIN32)
            probeJoinHandoff=true;
#else
            return 2;
#endif
        } else if (std::string_view(argv[i]) == "--probe-host-handoff") {
#if defined(PARTYBOARD_NATIVE_RTC) && defined(_WIN32)
            probeHostHandoff=true;
#else
            return 2;
#endif
        } else if (std::string_view(argv[i]) == "--prepare-measured-session") {
            if(!measuredSessionPath.empty()||i+1>=argc||!argv[i+1][0])return 2;
            measuredSessionPath=argv[++i];prepareOnlineEngine=true;
        } else if (std::string_view(argv[i]) == "--probe-online-frames") {
            if (probeOnlineFrames || i + 1 >= argc) return 2;
            const std::string_view value(argv[++i]);
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), probeOnlineFrames);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                probeOnlineFrames == 0 || probeOnlineFrames > 1800) return 2;
        } else if (std::string_view(argv[i]) == "--probe-online-buttons") {
            probeOnlineButtons = true;
        } else if (std::string_view(argv[i]) == "--probe-online-menu") {
            if (probeOnlineMenu != -1 || i + 1 >= argc) return 2;
            const std::string_view mode(argv[++i]);
            if (mode != "host" && mode != "join") return 2;
            probeOnlineMenu = mode == "host" ? 0 : 1;
        } else if (std::string_view(argv[i]) == "--probe-rtc-engine") {
#ifdef PARTYBOARD_NATIVE_RTC
            if (probeRtcFrames || i+1>=argc) return 2;
            const std::string_view value(argv[++i]);
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),probeRtcFrames);
            if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !probeRtcFrames || probeRtcFrames>1800) return 2;
#else
            fprintf(stderr,"--probe-rtc-engine requires PARTYBOARD_NATIVE_RTC build\n"); return 2;
#endif
        } else if (std::string_view(argv[i]) == "--trace-fast") {
            traceFast = true;
        } else if (std::string_view(argv[i]) == "--input-script") {
            if (!inputScript.empty() || i + 1 >= argc || !argv[i + 1][0]) {
                fprintf(stderr, "--input-script requires one trace path\n");
                return 2;
            }
            inputScript = argv[++i];
        } else if (std::string_view(argv[i]) == "--record-samples") {
            if (recordSamples || i + 1 >= argc) {
                fprintf(stderr, "--record-samples requires one count\n");
                return 2;
            }
            const std::string_view value(argv[++i]);
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), recordSamples);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                recordSamples == 0 || recordSamples > 10000000) {
                fprintf(stderr, "--record-samples must be 1..10000000\n");
                return 2;
            }
        } else if (std::string_view(argv[i]) == "--simulation-seed") {
            if (seedSpecified || i + 1 >= argc) {
                fprintf(stderr, "--simulation-seed requires one unsigned decimal integer\n");
                return 2;
            }
            const std::string_view value(argv[++i]);
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), simulationSeed);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
                fprintf(stderr, "Invalid --simulation-seed (expected 0..4294967295)\n");
                return 2;
            }
            seedSpecified = true;
        }
    }
    // Explicit local acceptance automation drives normal Host controls. It does
    // not select the diagnostic game's stop/return branch or force a restart.
    if(const char* acceptance=std::getenv("PARTYBOARD_NATIVE_HOST_RETURN_ACCEPTANCE")) {
#if defined(PARTYBOARD_NATIVE_RTC) && defined(_WIN32)
        if(std::string_view(acceptance)!="1"||onlineStartupAction!=0||probeHostLobby||probeJoinLobby||probeJoinStartup||probeHostHandoff||probeJoinHandoff||probeAudioSettings||prepareOnlineEngine||netRole||!tracePath.empty()||traceFast) {
            fprintf(stderr,"Host return acceptance requires the ordinary fresh Host frontend\n");return 2;
        }
        returnedHostMenuAcceptance=onlineSessionEndedNotice;
        automateHostRecovery=!returnedHostMenuAcceptance;
        if(returnedHostMenuAcceptance) {
            // Observe the actual entry.cpp child through ordinary engine and
            // renderer preparation. Bound its UI only once a new lobby is ready.
            std::ofstream marker(PartyBoard_ConfigPath/"recovery-returned-host-menu.pid.json");
            marker<<nlohmann::ordered_json({{"pid",GetCurrentProcessId()}}).dump()<<"\n";
            if(!marker){fprintf(stderr,"Returned Host menu PID marker failed\n");return 2;}
        }
#else
        fprintf(stderr,"Host return acceptance requires Windows native RTC\n");return 2;
#endif
    }
    if(const char* acceptance=std::getenv("PARTYBOARD_NATIVE_JOIN_RETURN_ACCEPTANCE")) {
#if defined(PARTYBOARD_NATIVE_RTC) && defined(_WIN32)
        if(std::string_view(acceptance)!="1"||onlineStartupAction!=1||probeJoinMenu||probeJoinLobby||probeJoinStartup||probeJoinExternal||probeHostLobby||probeHostHandoff||probeJoinHandoff||probeAudioSettings||prepareOnlineEngine||netRole||!tracePath.empty()||traceFast) {
            fprintf(stderr,"Join return acceptance requires the ordinary fresh Join frontend\n");return 2;
        }
        returnedJoinMenuAcceptance=onlineSessionEndedNotice;automateJoinRecovery=!returnedJoinMenuAcceptance;
        if(returnedJoinMenuAcceptance){
            std::ofstream marker(PartyBoard_ConfigPath/"recovery-returned-join-menu.pid.json");
            marker<<nlohmann::ordered_json({{"pid",GetCurrentProcessId()}}).dump()<<"\n";
            if(!marker){fprintf(stderr,"Returned Join menu PID marker failed\n");return 2;}
        }
#else
        fprintf(stderr,"Join return acceptance requires Windows native RTC\n");return 2;
#endif
    }
    if (probeOnlineButtons && !probeOnlineFrames && !probeRtcFrames) return 2;
    if (probeOnlineMenu != -1 && (!prepareOnlineEngine || probeOnlineFrames)) return 2;
    if (probeRtcFrames && (!prepareOnlineEngine || probeOnlineFrames || probeOnlineMenu!=-1)) return 2;
    if (probeOnlineFrames && !prepareOnlineEngine) {
        fprintf(stderr, "--probe-online-frames requires --prepare-online-engine\n"); return 2;
    }
    if(!measuredSessionPath.empty()&&(seedSpecified||probeOnlineFrames||probeOnlineButtons||probeOnlineMenu!=-1||probeRtcFrames))return 2;
#ifndef PARTYBOARD_NATIVE_RTC
    if(!measuredSessionPath.empty())return 2;
#endif
    if (prepareOnlineEngine && ((!seedSpecified&&measuredSessionPath.empty()) || !tracePath.empty() || !inputScript.empty() || netRole || alignStartClock || traceFast)) {
        fprintf(stderr, "--prepare-online-engine requires --simulation-seed and no trace/net diagnostic flags\n"); return 2;
    }
    if (traceReplay && seedSpecified) {
        fprintf(stderr, "Replay uses the seed from its trace; omit --simulation-seed\n");
        return 2;
    }
    if (recordSamples && (tracePath.empty() || traceReplay)) {
        fprintf(stderr, "--record-samples requires --record-input\n");
        return 2;
    }
    if (!inputScript.empty() && (tracePath.empty() || traceReplay)) {
        fprintf(stderr, "--input-script requires --record-input; it does not validate old checkpoints\n");
        return 2;
    }
    if (traceAfterInit && (!alignStartClock || traceReplay || tracePath.empty())) {
        fprintf(stderr, "--trace-after-init requires --align-start-clock and a new recording\n"); return 2;
    }
    if (alignStartClock && (tracePath.empty() || traceReplay)) {
        fprintf(stderr, "--align-start-clock requires a new diagnostic recording\n");
        return 2;
    }
    if (traceFast && tracePath.empty()) {
        fprintf(stderr, "--trace-fast requires a recording or replay\n");
        return 2;
    }
    if ((netRole || !netConfig.empty()) && (!netRole || netConfig.empty() ||
        tracePath.empty() || traceReplay || inputScript.empty() || recordSamples)) {
        fprintf(stderr, "Net diagnostic requires a role, config, --record-input and --input-script; omit replay/sample limits\n");
        return 2;
    }
    std::optional<std::string> installedPipelineSeed;
    std::shared_ptr<const partyboard::net::NativePipelineSeed> sharedPipelineSeed;
    uint32_t bundledPipelineDefinitions=0;
    const char* requestedPipelineSeed=std::getenv("PARTYBOARD_NATIVE_PIPELINE_SEED");
    const char* expectedPipelineSeed=std::getenv("PARTYBOARD_NATIVE_PIPELINE_SEED_SHA256");
    if(requestedPipelineSeed || expectedPipelineSeed){
        try{
            if(!requestedPipelineSeed || !expectedPipelineSeed)throw std::invalid_argument("Pipeline seed path and SHA256 must be supplied together");
            installedPipelineSeed=partyboard::net::installNativePipelineSeed(requestedPipelineSeed,PartyBoard_ConfigPath,expectedPipelineSeed);
            sharedPipelineSeed=partyboard::net::readNativePipelineSeed(requestedPipelineSeed,*installedPipelineSeed);
        }catch(const std::exception& error){fprintf(stderr,"Native pipeline seed failed: %s\n",error.what());return 2;}
    }else{
        try{
            const auto* directory=SDL_GetBasePath();
            if(!directory)throw std::runtime_error("Native application directory unavailable");
            const auto bundled=partyboard::net::readNativeBundledPipelineSeed(directory);
            sharedPipelineSeed=bundled.snapshot;installedPipelineSeed=sharedPipelineSeed->sha256;
            bundledPipelineDefinitions=bundled.definitions;
            EnsureInitialPipelineCache(PartyBoard_ConfigPath);
        }catch(const std::exception& error){fprintf(stderr,"Native bundled shader definitions failed: %s\n",error.what());return 2;}
    }
    // TODO: How to handle this?
    //PADSetDefaultMapping(&defaultPadMapping, PAD_TYPE_STANDARD);

    {
        const auto configPathString = PartyBoard_ConfigPath.u8string();
        AuroraConfig config{};
        config.appName = "Party Board";
        config.userPath = reinterpret_cast<const char*>(configPathString.c_str());
        config.cachePath = reinterpret_cast<const char*>(configPathString.c_str());
        config.vsync = traceFast ? false : partyboard::getSettings().video.enableVsync;
        config.startFullscreen = partyboard::getSettings().video.enableFullscreen;
        config.windowPosX = -1;
        config.windowPosY = -1;
        config.windowWidth = HU_FB_WIDTH * 2;
        config.windowHeight = HU_FB_HEIGHT * 2;
        config.desiredBackend = ResolveDesiredBackend();
        config.logCallback = &aurora_log_callback;
        config.mem1Size = 64 * 1024 * 1024;
        config.mem2Size = 24 * 1024 * 1024;
        config.allowJoystickBackgroundEvents = partyboard::getSettings().game.allowBackgroundInput;
        config.pauseOnFocusLost = partyboard::getSettings().game.pauseOnFocusLost;
        // config.imGuiInitCallback = &aurora_imgui_init_callback;
        config.allowTextureDumps = false;
        auroraInfo = aurora_initialize(argc, argv, &config);
    }
    if(bundledPipelineDefinitions){
        const auto* stats=aurora_get_stats();
        if(!stats||stats->loadedPipelineConfigurations<bundledPipelineDefinitions){
            fprintf(stderr,"Native renderer did not load the bundled shader definitions\n");aurora_shutdown();return 2;
        }
        printf("Native bundled pipeline seed: %s\n",nlohmann::ordered_json{{"sha256",*installedPipelineSeed},
            {"expectedDefinitions",bundledPipelineDefinitions},{"loadedDefinitions",stats->loadedPipelineConfigurations}}.dump().c_str());fflush(stdout);
    }

    LoadNativeKeyboardGlyphs();
    HuWinControlGlyphResolverSet(NativeKeyboardGlyph);
    HuWinControlGlyphPortResolverSet(NativeKeyboardGlyphForPort);
    // Isolated, capped diagnostics only; never persist this virtual mapping.
    if(!virtualGlyphController.empty()) {
        if(keyboardPreset || userDirectory.empty() || !recordSamples || inputScript.empty())return 2;
        SDL_VirtualJoystickDesc descriptor{};SDL_INIT_INTERFACE(&descriptor);
        descriptor.type=SDL_JOYSTICK_TYPE_GAMEPAD;
        descriptor.name="PartyBoard virtual glyph diagnostic";
        descriptor.naxes=SDL_GAMEPAD_AXIS_COUNT;descriptor.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
        descriptor.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
        descriptor.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
        const auto id=SDL_AttachVirtualJoystick(&descriptor);
        if(!id)return 2;
        auto* gamepad=SDL_OpenGamepad(id);
        char* rawMapping=gamepad?SDL_GetGamepadMapping(gamepad):nullptr;
        if(!rawMapping)return 2;
        std::string mapping(rawMapping);SDL_free(rawMapping);
        const auto start=mapping.find(",type:");
        if(start!=std::string::npos) {
            const auto end=mapping.find(',',start+1);
            mapping.erase(start,end==std::string::npos?std::string::npos:end-start);
        }
        if(mapping.back()!=',')mapping+=',';
        mapping+="type:"+virtualGlyphController+",";
        if(!SDL_SetGamepadMapping(id,mapping.c_str()))return 2;
        SDL_CloseGamepad(gamepad);
        aurora_update();
        PADInit();PADStatus initial[4]{};PADRead(initial);
        bool assigned=false;
        for(u32 index=0;index<PADCount();++index) {
            auto* candidate=PADGetSDLGamepadForIndex(index);
            if(candidate && SDL_GetGamepadID(candidate)==id) {
                PADSetPortForIndex(index,0);assigned=true;break;
            }
        }
        if(!assigned)return 2;
        PADSetKeyboardActive(0,FALSE);
        fprintf(stderr,"Virtual glyph controller assigned: %s type=%d\n",virtualGlyphController.c_str(),PADGetControllerType(0));
    }
    if (keyboardPreset) {
        // Load saved bindings first so lazy PADRead initialization cannot
        // replace this explicitly requested keyboard preset on the first frame.
        PADInit();
        PADStatus initialPads[PAD_CHANMAX]{};
        PADRead(initialPads);
        PADSetKeyboardActive(0, TRUE);
        const PADKeyButtonBinding buttons[] = {
            {SDL_SCANCODE_Z, PAD_BUTTON_A}, {SDL_SCANCODE_X, PAD_BUTTON_B},
            {SDL_SCANCODE_C, PAD_BUTTON_X}, {SDL_SCANCODE_V, PAD_BUTTON_Y},
            {SDL_SCANCODE_RETURN, PAD_BUTTON_START}, {SDL_SCANCODE_R, PAD_TRIGGER_Z},
            {SDL_SCANCODE_Q, PAD_TRIGGER_L}, {SDL_SCANCODE_E, PAD_TRIGGER_R},
            {SDL_SCANCODE_W, PAD_BUTTON_UP}, {SDL_SCANCODE_S, PAD_BUTTON_DOWN},
            {SDL_SCANCODE_A, PAD_BUTTON_LEFT}, {SDL_SCANCODE_D, PAD_BUTTON_RIGHT},
        };
        for (const auto& binding : buttons) PADSetKeyButtonBinding(0, binding);
        const PADKeyAxisBinding axes[] = {
            {SDL_SCANCODE_RIGHT, PAD_AXIS_LEFT_X_POS, 0}, {SDL_SCANCODE_LEFT, PAD_AXIS_LEFT_X_NEG, 0},
            {SDL_SCANCODE_UP, PAD_AXIS_LEFT_Y_POS, 0}, {SDL_SCANCODE_DOWN, PAD_AXIS_LEFT_Y_NEG, 0},
            {SDL_SCANCODE_L, PAD_AXIS_RIGHT_X_POS, 0}, {SDL_SCANCODE_J, PAD_AXIS_RIGHT_X_NEG, 0},
            {SDL_SCANCODE_I, PAD_AXIS_RIGHT_Y_POS, 0}, {SDL_SCANCODE_K, PAD_AXIS_RIGHT_Y_NEG, 0},
            {SDL_SCANCODE_Q, PAD_AXIS_TRIGGER_L, 0}, {SDL_SCANCODE_E, PAD_AXIS_TRIGGER_R, 0},
        };
        for (const auto& binding : axes) PADSetKeyAxisBinding(0, binding);
    }

#ifdef PARTY_BOARD_DISCORD
    partyboard::discord::initialize();
#endif

    char windowTitle[100];
    snprintf(windowTitle, sizeof(windowTitle), "PartyBoard %s", PARTY_BOARD_WC_DESCRIBE);
    VISetWindowTitle(windowTitle);

    if (partyboard::getSettings().video.lockAspectRatio) {
        AuroraSetViewportPolicy(AURORA_VIEWPORT_FIT);
    } else {
        AuroraSetViewportPolicy(AURORA_VIEWPORT_STRETCH);
    }
    VISetFrameBufferScale(partyboard::getSettings().game.internalResolutionScale.getValue());

    // TODO PC
    // partyboard::audio::SetMasterVolume(partyboard::getSettings().audio.masterVolume / 100.0f);
    // partyboard::audio::SetEnableReverb(partyboard::getSettings().audio.enableReverb);
    // partyboard::audio::EnableHrtf = partyboard::getSettings().audio.enableHrtf;

    // Run ImGui UI loop if Aurora couldn't initialize a backend
    if (auroraInfo.backend == BACKEND_NULL) {
        launchUILoop();
        fflush(stdout);
        fflush(stderr);
#ifdef PARTY_BOARD_DISCORD
        partyboard::discord::shutdown();
#endif
        partyboard::ui::shutdown();
        aurora_shutdown();
        return 0;
    }

    partyboard::ui::initialize();
    partyboard::ui::push_document(std::make_unique<partyboard::ui::Overlay>(), true, true);
    partyboard::ui::push_document(std::make_unique<partyboard::ui::MenuBar>(), false);
    if(probeAudioSettings) {
        bool passed=false;
        try {
            if(onlineStartupAction!=-1)throw std::runtime_error("Audio settings probe requires a fresh local profile");
            auto doc=std::make_unique<partyboard::ui::SettingsWindow>();
            auto* settings=doc.get();partyboard::ui::push_document(std::move(doc),true);
            if(!settings->set_active_tab(2))throw std::runtime_error("Audio tab unavailable");
            unsigned frame=0;
            auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
            const bool active=nativeUILoop([&] {
                if(++frame>24)return false;
                auto element=[&](const char* id) {
                    auto* context=aurora::rmlui::get_context();
                    auto* node=context?context->GetRootElement()->GetElementById(id):nullptr;
                    if(!node||!node->IsVisible(true))throw std::runtime_error("Audio control not visible: "+std::string(id)+" at UI frame "+std::to_string(frame));
                    return node;
                };
                auto key=[&](const char* id,Rml::Input::KeyIdentifier code) {
                    auto* node=element(id);node->Focus(true);
                    node->DispatchEvent(Rml::EventId::Keydown,{{"key_identifier",Rml::Variant{static_cast<int>(code)}}});
                };
                auto& audio=partyboard::getSettings().audio;
                if(frame==3) {
                    require(audio.masterVolume.getValue()==80&&!audio.muted.getValue(),"Unexpected fresh volume defaults");
                    key("audio-master-volume",Rml::Input::KI_RIGHT);
                    require(audio.masterVolume.getValue()==85,"Volume right key failed");
                }
                if(frame==6) {
                    element("audio-game-mute")->DispatchEvent(Rml::EventId::Click,{});
                    require(audio.muted.getValue()&&partyboard_settings_audio_gain()==0,"Mute click failed");
                }
                if(frame==9) {
                    key("audio-master-volume",Rml::Input::KI_LEFT);
                    require(audio.masterVolume.getValue()==80&&partyboard_settings_audio_gain()==0,"Muted volume edit failed");
                }
                if(frame==12) {
                    key("audio-game-mute",Rml::Input::KI_RETURN);
                    require(!audio.muted.getValue()&&partyboard_settings_audio_gain()==0.8f,"Unmute did not restore volume");
                }
                if(frame==15) {
                    key("audio-speaker-layout",Rml::Input::KI_RETURN);
                }
                if(frame==18) {
                    element("audio-speaker-6")->DispatchEvent(Rml::EventId::Click,{});
                    require(partyboard_settings_audio_channels()==6,"Surround selection failed");
                }
                if(frame==21) {
                    element("audio-speaker-2")->DispatchEvent(Rml::EventId::Click,{});
                    require(partyboard_settings_audio_channels()==2,"Stereo selection failed");
                    require(PartyBoard_GameLifecyclePhase()==0&&PartyBoard_GameFrameCount()==0,"Settings started the game");
                    passed=true;
                }
                return true;
            });
            (void)active;require(passed,"Audio settings probe ended early");
            printf("Native audio settings UI: PASS visible controls, keyboard volume, click mute, keyboard unmute and stereo/surround selection; phase0 frame0\n");
        } catch(const std::exception& error) {fprintf(stderr,"Native audio settings UI: FAIL %s\n",error.what());passed=false;}
        partyboard::ui::shutdown();HuAudPortStreamShutdown();aurora_shutdown();
        return passed?0:2;
    }
#ifdef PARTYBOARD_NATIVE_RTC
    if(lobbyPackagePath.empty()&&(onlineStartupAction==0||onlineStartupAction==1)){
        if(const auto* base=SDL_GetBasePath()){
            const auto bundled=std::filesystem::path(base)/"native-session-package.json";
            if(std::filesystem::is_regular_file(bundled))lobbyPackagePath=bundled.string();
        }
    }
    if(probeJoinHandoff&&onlineStartupAction==1)probeJoinStartup=true;
    if(onlineStartupAction==1){
      std::unique_ptr<partyboard::net::NativeGuestStagingFile> stagingOwner;
      try{
        if(lobbyPackagePath.empty())throw std::runtime_error("Native Join requires the measured runtime package descriptor");
        std::ifstream descriptorStream(lobbyPackagePath);
        if(!descriptorStream)throw std::runtime_error("Native Join runtime package could not open");
        const auto descriptor=nlohmann::ordered_json::parse(descriptorStream);
        std::vector<partyboard::net::NativeRuntimeArtifact> artifacts;
        for(const auto& file:descriptor.at("files"))artifacts.push_back({file.at("name"),partyboard::net::nativeRuntimeArtifactPath(lobbyPackagePath,file.at("path").get<std::string>())});
        const auto measured=partyboard::net::fingerprintNativeRuntimeFiles(std::move(artifacts),descriptor.at("configuration").at("buildId").get<std::string>());
        stagingOwner=std::make_unique<partyboard::net::NativeGuestStagingFile>(PartyBoard_ConfigPath/"join-assets");
        auto& staging=*stagingOwner;
        partyboard::ui::NativeJoinClipboard diagnosticClipboard;
        std::string diagnosticClipboardText;
        if(probeJoinMenu||automateJoinRecovery){
            diagnosticClipboard.read=[&]{return diagnosticClipboardText;};
            diagnosticClipboard.write=[&](std::string_view text){diagnosticClipboardText.assign(text);return true;};
        }
        // Initial Join and reconnect must use the same clipboard provider.
        const auto recoveryClipboard=diagnosticClipboard;
        auto document=std::make_unique<partyboard::ui::NativeJoinLobby>(staging.stream(),std::move(diagnosticClipboard),onlineSessionEndedNotice,(probeJoinMenu||automateJoinRecovery)&&!automaticLobbyTest);
        auto* lobby=document.get();partyboard::ui::push_document(std::move(document),true);
        partyboard::ui::NativeJoinPreparation preparation(*lobby,staging,measured.at("buildId"),installedPipelineSeed);
        std::unique_ptr<partyboard::ui::NativeJoinMenuProbe> menuProbe;
        if((probeJoinMenu||automateJoinRecovery)&&!automaticLobbyTest)menuProbe=std::make_unique<partyboard::ui::NativeJoinMenuProbe>(*lobby,preparation,measured.at("buildId"));
        const bool fullMatchProbe=menuProbe&&std::getenv("PARTYBOARD_NATIVE_FULL_MATCH")&&std::string_view(std::getenv("PARTYBOARD_NATIVE_FULL_MATCH"))=="1";
        partyboard::net::NativeFullMatchProgress fullMatchProgress(NativeFullMatchBoard(),NativeFullMatchTurns(),DLL_mstory3Dll);
        const bool focusedPlayProbe=fullMatchProbe&&std::getenv("PARTYBOARD_NATIVE_FOCUSED_MINIGAME")&&std::string_view(std::getenv("PARTYBOARD_NATIVE_FOCUSED_MINIGAME"))=="443";
        bool focusedStopAcknowledged=false;
        bool fullMatchComplete=false;
        uint64_t nextFullMatchProgressFrame=0;
        partyboard::net::NativeDiagnosticPad diagnosticPad;
        const auto menuDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(fullMatchProbe?21600:1200);
        bool windowClosed=false;
        auto pumpEvents=[&]{
            const auto* event=aurora_update();
            while(event&&event->type!=AURORA_NONE){
                if(event->type==AURORA_EXIT){windowClosed=true;return false;}
                if(event->type==AURORA_SDL_EVENT)partyboard::ui::handle_event(event->sdl);
                ++event;
            }
            partyboard::ui::update();return !lobby->cancelled();
        };
        unsigned startupUpdates=0;
        bool sessionEndedNoticeSeen=false;
        partyboard::net::NativeProbePipe automaticJoinPipe;bool automaticJoinReceiptEmitted=false;
        if(automaticLobbyTest)partyboard::ui::NativeJoinMenuProbe::emit({{"type","automatic-join-waiting"},{"buildId",measured.at("buildId")}});
        const bool active=nativeUILoop([&]{
            if(automaticLobbyTest){
                for(const auto& line:automaticJoinPipe.poll()){
                    if(line=="stop")return false;
                    const auto command=nlohmann::ordered_json::parse(line);if(command.at("type")!="join-link")throw std::runtime_error("Automatic Join test requires a lobby link");
                    lobby->diagnosticPasteAutomaticLink(command.at("link"));
                }
                if(preparation.prepared()&&!automaticJoinReceiptEmitted){automaticJoinReceiptEmitted=true;partyboard::ui::NativeJoinMenuProbe::emit({{"type","automatic-join-prepared"},{"receipt",preparation.receipt()}});}
            }
            if((probeJoinStartup||returnedJoinMenuAcceptance)&&onlineSessionEndedNotice)sessionEndedNoticeSeen|=lobby->sessionEndedNoticeVisible();
            if(returnedJoinMenuAcceptance&&++startupUpdates>=30&&sessionEndedNoticeSeen&&lobby->initialInvitationAvailable())return false;
            if(probeJoinStartup&&++startupUpdates==20){SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=SDLK_ESCAPE;event.key.scancode=SDL_SCANCODE_ESCAPE;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);}
            if(menuProbe){if(std::chrono::steady_clock::now()>=menuDeadline)throw std::runtime_error("Native Join menu validation deadline exceeded");menuProbe->pump();if(menuProbe->stopped()){focusedStopAcknowledged=focusedPlayProbe;return false;}}
            return preparation.tick(
            [](const auto& path){return aurora_dvd_open(path.string().c_str());},
            []{partyboard::version::init();LanguageInit();InitializeDol();},pumpEvents);});
        int execution=0;
        if(active&&!lobby->cancelled()&&preparation.prepared()&&preparation.prepared()->committed()){
            partyboard::ui::NativeGuestRecoveryWindow* recoveryWindow=nullptr;
            partyboard::net::NativeGuestGameInput* diagnosticInput=nullptr;
            bool joinRecoveryUiSeen=false;
            uint64_t nextJoinObservation=0;
            auto input=preparation.installCommittedInput([&]{
                if(fullMatchProbe){
                    EmitNativeFullMatchProgress("Native external Join event",nextFullMatchProgressFrame);
                    if(std::chrono::steady_clock::now()>=menuDeadline)throw std::runtime_error("Native Join full match timed out");
                    fullMatchComplete=fullMatchProgress.observe(PartyBoard_GameFrameCount(),PartyBoard_GameProgress(2),PartyBoard_GameProgress(0),PartyBoard_GameProgress(1),PartyBoard_GameProgress(4),PartyBoard_GameProgress(5));
                }
                if(probeJoinMenu&&!automaticLobbyTest&&diagnosticInput&&!fullMatchProbe&&diagnosticInput->observations().acceptedCount()>=60){PartyBoard_GameRequestStop();return false;}
                if(menuProbe&&diagnosticInput&&recoveryWindow&&!menuProbe->pumpGameplay(*diagnosticInput,*recoveryWindow,[&](const auto& packet){
                    if(!fullMatchProbe)return false;
                    if(packet.value("type","")=="diagnostic-local-input"){diagnosticPad.accept(packet);return true;}
                    if(packet.value("type","")=="diagnostic-observe"){
                        auto observation=NativeMatchObservation(packet,diagnosticInput->observations());
                        observation["inputDiagnostics"]=diagnosticInput->diagnosticState();
                        observation["inputTiming"]=partyboard::net::nativeInputTimingJson(diagnosticInput->inputTiming());
                        partyboard::ui::NativeJoinMenuProbe::emit(observation);return true;
                    }
                    return false;
                })){
                    focusedStopAcknowledged=focusedPlayProbe&&menuProbe->stopped();
                    return false;
                }
                if(automaticLobbyTest)for(const auto& line:automaticJoinPipe.poll()){
                    if(line=="stop"){PartyBoard_GameRequestStop();return false;}
                    if(line=="disconnect"){diagnosticInput->disconnectDiagnostic();continue;}
                    if(line!="observe")throw std::runtime_error("Unknown automatic Join observation command");
                    nlohmann::ordered_json rows=nlohmann::ordered_json::array();for(const auto& sample:diagnosticInput->observations().entries())rows.push_back({{"frame",sample.state.frame},{"checkpoint",sample.state.checkpoint},{"frand",sample.state.frand},{"rand8",sample.state.rand8},{"pads",sample.pads}});
                    partyboard::ui::NativeJoinMenuProbe::emit({{"type","automatic-game-observation"},{"accepted",diagnosticInput->observations().acceptedCount()},{"records",rows},{"recovery",diagnosticInput->recoveryStatus()},{"error",diagnosticInput->recoveryError()}});
                }
                const auto* event=aurora_update();
                while(event&&event->type!=AURORA_NONE){
                    if(event->type==AURORA_EXIT){windowClosed=true;return false;}
                    if(recoveryWindow&&recoveryWindow->visible()&&event->type==AURORA_SDL_EVENT)partyboard::ui::handle_event(event->sdl);
                    ++event;
                }
                if(recoveryWindow&&recoveryWindow->pump()&&aurora_begin_frame()){
                    partyboard::ui::update();aurora_end_frame();
                }
                if(automateJoinRecovery&&diagnosticInput){
                    joinRecoveryUiSeen|=recoveryWindow&&recoveryWindow->visible();
                    const auto now=partyboard::net::NativeEngineInputIO::monotonicNow();
                    if(now>=nextJoinObservation){
                        PartyBoardInputState state{};
                        if(!PartyBoardCaptureInputState(&state))throw std::runtime_error("Ordinary Join input-boundary observation unavailable");
                        partyboard::ui::NativeJoinMenuProbe::emit({{"type","join-ordinary-observation"},{"status",diagnosticInput->recoveryStatus()},{"recoveryUiVisible",recoveryWindow&&recoveryWindow->visible()},{"state",{{"frame",state.frame},{"checkpoint",state.checkpoint},{"frand",state.frand},{"rand8",state.rand8}}}});
                        nextJoinObservation=now+1000;
                    }
                }
                return true;
            });
            diagnosticInput=input.get();if(automaticLobbyTest)input->useRollingDiagnosticObservations();
            if(fullMatchProbe){input->useRollingDiagnosticObservations();input->setDiagnosticLocalInput([&](const auto&,auto* pads){diagnosticPad.sample(pads);});}
            if(menuProbe){
                partyboard::ui::NativeJoinMenuProbe::emit({{"type","join-input-installed"},{"music",lobby->musicState()},{"commit",preparation.prepared()->commit()},{"peer",lobby->session()->transport().assignedPeer()},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()}});
                if(!nativeUILoop([&]{if(std::chrono::steady_clock::now()>=menuDeadline)throw std::runtime_error("Native Join run command timeout");menuProbe->pump();return !menuProbe->runRequested()&&!menuProbe->stopped();})||!menuProbe->runRequested())throw std::runtime_error("Native Join diagnostic run cancelled");
            }
            auto recoveryDocument=std::make_unique<partyboard::ui::NativeGuestRecoveryWindow>(*input,recoveryClipboard.read,recoveryClipboard.write,lobby->automaticRoom());
            recoveryWindow=recoveryDocument.get();partyboard::ui::push_document(std::move(recoveryDocument),false);
#ifdef _WIN32
            if(automateJoinRecovery)partyboard::ui::NativeJoinMenuProbe::emit({{"type","join-ordinary-lifecycle"},{"pid",GetCurrentProcessId()},{"probeJoinMenu",probeJoinMenu},{"recoveryWindowCreated",recoveryWindow!=nullptr}});
#endif
            struct RecoveryDetach {partyboard::ui::NativeGuestRecoveryWindow* document;~RecoveryDetach(){document->detach();}} recoveryDetach{recoveryWindow};
            lobby->hide(false);
            execution=PartyBoard_GameResume();PartyBoard_GameRequestStop();
            if(menuProbe){
                const auto& trace=input->observations();nlohmann::ordered_json samples=nlohmann::ordered_json::array();
                for(const auto& sample:trace.entries())samples.push_back({{"frame",sample.state.frame},{"checkpoint",sample.state.checkpoint},{"frand",sample.state.frand},{"rand8",sample.state.rand8},{"pads",sample.pads}});
                partyboard::ui::NativeJoinMenuProbe::emit({{"type","join-gameplay"},{"execution",execution},{"accepted",trace.acceptedCount()},{"observations",samples},{"recoveryError",input->recoveryError()},{"inputDiagnostics",input->diagnosticState()},{"inputTiming",partyboard::net::nativeInputTimingJson(input->inputTiming())},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()}});
                if(probeJoinMenu&&(fullMatchProbe?(focusedPlayProbe?(!focusedStopAcknowledged||trace.acceptedCount()<120):!fullMatchComplete):trace.acceptedCount()!=60))execution=2;
                if(fullMatchProbe)partyboard::ui::NativeJoinMenuProbe::emit({{"type","join-full-match"},{"complete",fullMatchComplete},{"focusedMinigame",focusedPlayProbe},{"focusedStopAcknowledged",focusedStopAcknowledged},{"observedTurns",fullMatchProgress.observedTurnCount()},{"renderedRankingsVerified",false}});
            }
            recoveryWindow->detach();
            const bool recoveryEndedSession=input->recoveryStatus().value("returnToMenu",false);
            if(!input->closeStopped())throw std::runtime_error("Native Join input detach failed");
            input.reset();
            if(menuProbe)partyboard::ui::NativeJoinMenuProbe::emit({{"type","join-game-stopped"},{"detached",true},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()}});
#ifdef _WIN32
            if(automateJoinRecovery)partyboard::ui::NativeJoinMenuProbe::emit({{"type","join-ordinary-ended"},{"pid",GetCurrentProcessId()},{"recoveryEndedSession",recoveryEndedSession},{"recoveryUiSeen",joinRecoveryUiSeen},{"probeJoinMenu",probeJoinMenu}});
#endif
            if(!probeJoinMenu&&!windowClosed&&!PartyBoard_GameWindowExitRequested()&&execution==0){
#ifdef _WIN32
                partyboard::net::publishNativeSessionEndedNotice(recoveryEndedSession);
#endif
                nativeRestartOnlineAction=1;PartyBoard_RestartRequested=true;
            }
        }
#ifdef _WIN32
        if(returnedJoinMenuAcceptance){
            const bool fresh=active&&sessionEndedNoticeSeen&&lobby->initialInvitationAvailable()&&!lobby->session()&&!preparation.prepared()&&PartyBoard_GameLifecyclePhase()==0&&PartyBoard_GameFrameCount()==0;
            std::ofstream report(PartyBoard_ConfigPath/"recovery-returned-join-menu.json");
            report<<nlohmann::ordered_json({{"pid",GetCurrentProcessId()},{"passed",fresh},{"menu","join"},{"seenVisible",sessionEndedNoticeSeen},{"invitationAvailable",lobby->initialInvitationAvailable()},{"sessionPresent",lobby->session()!=nullptr},{"enginePrepared",preparation.prepared()!=nullptr},{"probeJoinMenu",probeJoinMenu},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()},{"buildId",measured.at("buildId")},{"text",partyboard::net::NativeSessionEndedNotice}}).dump()<<"\n";
            if(!fresh||!report)execution=2;
        }
#endif
        if(probeJoinStartup){
            const bool passed=active&&lobby->cancelled()&&!lobby->session()&&PartyBoard_GameLifecyclePhase()==0&&PartyBoard_GameFrameCount()==0&&(!onlineSessionEndedNotice||sessionEndedNoticeSeen);
            if(onlineSessionEndedNotice){printf("Native menu notice: %s\n",nlohmann::ordered_json({{"menu","join"},{"seenVisible",sessionEndedNoticeSeen},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()},{"text",partyboard::net::NativeSessionEndedNotice}}).dump().c_str());fflush(stdout);}
            fprintf(stderr,"Native Join startup cancel probe: %s updates=%u phase=%d frame=%u\n",passed?"PASS":"FAIL",startupUpdates,PartyBoard_GameLifecyclePhase(),PartyBoard_GameFrameCount());
            if(!passed)execution=2;
#ifdef _WIN32
            if(probeJoinHandoff){
                std::ofstream report(PartyBoard_ConfigPath/"join-handoff-child.txt");
                report<<GetCurrentProcessId()<<" "<<passed<<" "<<onlineStartupAction<<" "<<PartyBoard_GameLifecyclePhase()<<" "<<startupUpdates<<"\n";
                if(!report)execution=2;
            }
#endif
        }
        preparation.detachStopped();
        partyboard::ui::shutdown();HuAudPortStreamShutdown();aurora_shutdown();
        return execution==0?0:2;
      }catch(const std::exception& error){fprintf(stderr,"Native Join frontend failed: %s\n",error.what());partyboard::ui::shutdown();HuAudPortStreamShutdown();aurora_shutdown();return 2;}
    }
    if(probeJoinExternal){
      try{
        if(lobbyPackagePath.empty())throw std::runtime_error("External Join package descriptor required");
        partyboard::net::NativeExternalJoinProbe probe(PartyBoard_ConfigPath/"external-join-assets",lobbyPackagePath,installedPipelineSeed);
        struct ExternalJoinCleanup {bool done=false;~ExternalJoinCleanup(){if(!done){partyboard::ui::shutdown();HuAudPortStreamShutdown();aurora_shutdown();}}} cleanup;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1200);
        auto pumpEvents=[&]{
            const auto* event=aurora_update();while(event&&event->type!=AURORA_NONE){if(event->type==AURORA_EXIT)return false;if(event->type==AURORA_SDL_EVENT)partyboard::ui::handle_event(event->sdl);++event;}
            partyboard::ui::update();return std::chrono::steady_clock::now()<deadline;
        };
        const bool active=nativeUILoop([&]{
            if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("External Join validation deadline exceeded");
            return probe.tick([](const auto& path){return aurora_dvd_open(path.string().c_str());},
                []{partyboard::version::init();LanguageInit();InitializeDol();},pumpEvents);
        });
        if(active&&probe.runRequested())probe.run();
        const bool passed=active&&probe.stopped();probe.closeStopped();
        partyboard::ui::shutdown();HuAudPortStreamShutdown();aurora_shutdown();cleanup.done=true;
        return passed?0:2;
      }catch(const std::exception& error){fprintf(stderr,"External native Join probe failed: %s\n",error.what());return 2;}
    }
    if(probeJoinLobby){
      try{
        partyboard::net::NativeGuestStagingFile staging(PartyBoard_ConfigPath/"join-assets");
        struct JoinProbeCleanup {bool done=false;~JoinProbeCleanup(){if(!done){partyboard::ui::shutdown();HuAudPortStreamShutdown();aurora_shutdown();}}} cleanup;
        auto document=std::make_unique<partyboard::ui::NativeJoinLobby>(staging.stream(),partyboard::ui::NativeJoinClipboard{},false,true);
        auto* lobby=document.get();partyboard::ui::push_document(std::move(document),true);
        partyboard::net::NativeRtcSessionControl hostControls;
        std::shared_ptr<partyboard::net::NativeSharedAsset> hostAsset;
        std::unique_ptr<partyboard::net::NativeHostGuest> hostGuest;
        const auto sourcePath=PartyBoard_ConfigPath/"probe-join-host.bin";
        std::optional<nlohmann::ordered_json> joinPackage;
        bool configurationSent=false,enginePrepared=false;
        bool hostReceiptAccepted=false;
        bool hostReadyAccepted=false;uint64_t joinReadySequence=0;
        bool joinReadySent=false,joinStartAccepted=false,joinInputInstalled=false,joinInputSampled=false;
        std::unique_ptr<partyboard::ui::NativeJoinPreparation> joinPreparation;
        partyboard::net::NativeGuestPreparedLobby* preparedJoin=nullptr;
        std::unique_ptr<partyboard::net::NativeLobbyState> probeHostLobby;
        std::optional<partyboard::net::SessionStartBarrier> probeStartBarrier;
        std::optional<nlohmann::ordered_json> expectedJoinReceipt;
        if(probeJoinConnected){
            if(!probeJoinPrepared){
                if(std::filesystem::exists(sourcePath))throw std::runtime_error("Join probe source already exists");
                {std::ofstream source(sourcePath,std::ios::binary);for(unsigned i=0;i<500000;++i)source.put(char((i*29)&255));if(!source)throw std::runtime_error("Join probe source write failed");}
            }
            hostAsset=std::make_shared<partyboard::net::NativeSharedAsset>(probeJoinPrepared?std::filesystem::path(partyboard::getSettings().backend.isoPath.getValue()):sourcePath,"game.ciso");
            hostGuest=std::make_unique<partyboard::net::NativeHostGuest>("guest2",hostControls,hostAsset);
            if(probeJoinPrepared){
                if(lobbyPackagePath.empty())throw std::runtime_error("Measured package required for Join preparation probe");
                std::ifstream descriptorStream(lobbyPackagePath);const auto descriptor=nlohmann::ordered_json::parse(descriptorStream);
                std::vector<partyboard::net::NativeRuntimeArtifact> files;
                for(const auto& file:descriptor.at("files"))files.push_back({file.at("name"),partyboard::net::nativeRuntimeArtifactPath(lobbyPackagePath,file.at("path").get<std::string>())});
                joinPackage=partyboard::net::createNativeHostSessionPackage(std::move(files),descriptor.at("configuration").at("buildId"),hostAsset->asset.manifest().at("id"));
            }
        }
        unsigned updates=0;
        unsigned stage=0,stageUpdate=0;bool verified=false;
        auto key=[&](SDL_Keycode code,SDL_Scancode scan){SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=code;event.key.scancode=scan;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);};
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(probeJoinPrepared?300:60);
        const bool active=nativeUILoop([&]{
            if(lobby->cancelled()||std::chrono::steady_clock::now()>=deadline)return false;
            ++updates;
            if(!probeJoinConnected){if(updates==20||updates==22)key(SDLK_DOWN,SDL_SCANCODE_DOWN);if(updates==24)key(SDLK_RETURN,SDL_SCANCODE_RETURN);return true;}
            if(stage==0&&updates>=20){if(const auto invitation=hostGuest->invitation()){
                if(!SDL_SetClipboardText(invitation->c_str()))throw std::runtime_error("Join probe invitation clipboard failed");key(SDLK_RETURN,SDL_SCANCODE_RETURN);stage=1;
            }}else if(stage==1&&lobby->session()&&lobby->session()->transport().reply()){
                key(SDLK_DOWN,SDL_SCANCODE_DOWN);stage=2;stageUpdate=updates;
            }else if(stage==2&&updates>=stageUpdate+2){key(SDLK_RETURN,SDL_SCANCODE_RETURN);stage=3;}
            else if(stage==3){
                char* raw=SDL_GetClipboardText();const std::string reply=raw?raw:"";SDL_free(raw);
                bool answered=false;try{(void)partyboard::net::receiveNativeInvitation(reply,"answer","guest2");answered=true;}catch(const std::exception&){}
                if(answered){hostGuest->acceptReply(reply);stage=4;}
            }else if(stage==4){
                if(probeJoinPrepared&&!configurationSent&&hostControls.connected("guest2")){
                    const auto& configuration=joinPackage->at("configuration");
                    const auto credential=partyboard::net::createRecoveryCredential(configuration.at("session"),configuration.at("assetId"),"guest2");
                    if(!hostControls.send("guest2",partyboard::net::NativeRtcControlChannel::Purpose::Preparation,configuration.dump())||!hostControls.send("guest2",partyboard::net::NativeRtcControlChannel::Purpose::Preparation,credential.dump()))throw std::runtime_error("Join probe configuration delivery failed");
                    configurationSent=true;
                    probeHostLobby=std::make_unique<partyboard::net::NativeLobbyState>(hostAsset->asset.manifest().at("id"),hostAsset->asset.manifest().at("size"));
                    probeHostLobby->join("host","Host");probeHostLobby->join("guest2","Guest");
                }
                if(probeJoinPrepared&&configurationSent&&joinReadySequence<160){
                    ++joinReadySequence;
                    probeHostLobby->report("host",{{"assetId",hostAsset->asset.manifest().at("id")},{"sequence",joinReadySequence},{"bytes",0},{"verified",false},{"shaders",false},{"ready",false},{"membership",2},{"saveHash",nullptr}});
                    if(!hostControls.send("guest2",partyboard::net::NativeRtcControlChannel::Purpose::Lobby,nlohmann::ordered_json{{"type","lobby-snapshot"},{"version",1},{"snapshot",probeHostLobby->snapshot()}}.dump()))throw std::runtime_error("Download snapshot send failed");
                }
                if(!lobby->session()->assetsComplete()||!hostGuest->assetsComplete())return true;
                verified=std::filesystem::file_size(staging.verifiedPath(*lobby->session()))==hostAsset->asset.manifest().at("size").get<uint64_t>();
                if(probeJoinPrepared){
                    if(!joinPreparation)joinPreparation=std::make_unique<partyboard::ui::NativeJoinPreparation>(*lobby,staging,joinPackage->at("buildId"),installedPipelineSeed);
                    joinPreparation->tick([](const auto& path){return aurora_dvd_open(path.string().c_str());},
                        []{partyboard::version::init();LanguageInit();InitializeDol();},[&]{
                            const auto* event=aurora_update();while(event&&event->type!=AURORA_NONE){if(event->type==AURORA_EXIT)return false;if(event->type==AURORA_SDL_EVENT)partyboard::ui::handle_event(event->sdl);++event;}
                            partyboard::ui::update();return !lobby->cancelled();
                        });
                    preparedJoin=joinPreparation->prepared();if(!preparedJoin)return true;
                    expectedJoinReceipt=joinPreparation->receipt();
                    fprintf(stderr,"Native Join prepared receipt: %s\n",expectedJoinReceipt->dump().c_str());enginePrepared=true;
                    stage=6;return true;
                }
                key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);stage=5;
            }else if(stage==6){
                if(const auto packet=hostControls.take(partyboard::net::NativeRtcControlChannel::Purpose::Preparation)){
                    if(packet->route.peer!="guest2")throw std::runtime_error("Join receipt arrived from another assignment");
                    const auto received=partyboard::net::receiveNativePreparedRuntimeReport(packet->text,joinPackage->at("configuration"));
                    if(!partyboard::net::nativePreparedRuntimeReportsMatch(*expectedJoinReceipt,received))throw std::runtime_error("Host received mismatched native Join receipt");
                    hostReceiptAccepted=true;
                    if(!hostControls.send("guest2",partyboard::net::NativeRtcControlChannel::Purpose::Lobby,nlohmann::ordered_json{{"type","lobby-snapshot"},{"version",1},{"snapshot",probeHostLobby->snapshot()}}.dump()))throw std::runtime_error("Join lobby snapshot send failed");
                    stage=7;
                }
            }else if(stage==7){
                preparedJoin->pump();
                if(preparedJoin->hasSnapshot()&&!joinReadySent){
                    // Let the document observe this snapshot and enable Ready
                    // before navigation. Activation follows in a later UI update.
                    stageUpdate=updates;joinReadySent=true;
                }
                if(joinReadySent&&updates==stageUpdate+2){
                    key(SDLK_DOWN,SDL_SCANCODE_DOWN);
                }
                if(joinReadySent&&updates==stageUpdate+4){
                    key(SDLK_RETURN,SDL_SCANCODE_RETURN);
                }
                if(const auto packet=hostControls.take(partyboard::net::NativeRtcControlChannel::Purpose::Lobby)){
                    if(packet->route.peer!="guest2")throw std::runtime_error("Join readiness arrived from another assignment");
                    const auto envelope=nlohmann::ordered_json::parse(packet->text);
                    if(envelope.at("type")!="lobby-report"||envelope.at("version")!=1)throw std::runtime_error("Invalid Join readiness envelope");
                    probeHostLobby->report("guest2",envelope.at("report"));
                    if(envelope.at("report").at("ready")==false)return true;
                    const auto snapshot=probeHostLobby->snapshot();
                    if(snapshot.at("players").at(1).at("ready")!=true||snapshot.at("canStart")!=false)throw std::runtime_error("Join Ready must not bypass host readiness");
                    hostReadyAccepted=true;
                    // This in-process probe uses a fixture host readiness/capture;
                    // the guest engine, preparation and RTC transport are actual.
                    probeHostLobby->report("host",{{"assetId",hostAsset->asset.manifest().at("id")},{"sequence",joinReadySequence+1},{"bytes",hostAsset->asset.manifest().at("size")},{"verified",true},{"shaders",true},{"ready",true},{"membership",2},{"saveHash",nullptr}});
                    if(!hostControls.send("guest2",partyboard::net::NativeRtcControlChannel::Purpose::Lobby,nlohmann::ordered_json{{"type","lobby-snapshot"},{"version",1},{"snapshot",probeHostLobby->snapshot()}}.dump()))throw std::runtime_error("Ready snapshot send failed");
                    stage=8;
                }
            }else if(stage==8){
                preparedJoin->pump();
                if(preparedJoin->snapshot().revision!=probeHostLobby->snapshot().at("revision").get<uint64_t>())return true;
                bool prematureRejected=false;try{partyboard::net::NativeGuestGameInput premature(*lobby->session(),*preparedJoin,[]{return true;});}catch(const std::exception&){prematureRejected=true;}
                if(!prematureRejected||PartyBoardNativeOnlineInputRequired())throw std::runtime_error("Guest input installed before Start commit");
                const auto captured=partyboard::net::captureNativePreparedStart(joinPackage->at("configuration").at("assetId"),joinPackage->at("buildId"));
                probeStartBarrier=partyboard::net::SessionStartBarrier::fromConfiguration(preparedJoin->snapshot(),joinPackage->at("configuration"),captured);
                auto ack=probeStartBarrier->proposal();ack["type"]="session-start-ack";
                if(!probeStartBarrier->acknowledge("host",ack,preparedJoin->snapshot())||!hostControls.send("guest2",partyboard::net::NativeRtcControlChannel::Purpose::Start,probeStartBarrier->proposal().dump()))throw std::runtime_error("Prepared Join proposal failed");
                stage=9;
            }else if(stage==9){
                preparedJoin->pump();
                if(const auto packet=hostControls.take(partyboard::net::NativeRtcControlChannel::Purpose::Start)){
                    if(packet->route.peer!="guest2"||!probeStartBarrier->acknowledge("guest2",nlohmann::ordered_json::parse(packet->text),preparedJoin->snapshot()))throw std::runtime_error("Actual Join acknowledgment rejected");
                    const auto captured=partyboard::net::captureNativePreparedStart(joinPackage->at("configuration").at("assetId"),joinPackage->at("buildId"));
                    if(!hostControls.send("guest2",partyboard::net::NativeRtcControlChannel::Purpose::Start,probeStartBarrier->prepareCommit(preparedJoin->snapshot(),captured).dump()))throw std::runtime_error("Join commit send failed");
                    stage=10;
                }
            }else if(stage==10){
                preparedJoin->pump();if(preparedJoin->committed()){
                    joinStartAccepted=true;
                    const auto sampleDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
                    auto guestInputOwner=joinPreparation->installCommittedInput([sampleDeadline]{return std::chrono::steady_clock::now()<sampleDeadline;});
                    auto& guestInput=*guestInputOwner;
                    auto recoveryDocument=std::make_unique<partyboard::ui::NativeGuestRecoveryWindow>(guestInput);
                    auto* guestRecoveryWindow=recoveryDocument.get();partyboard::ui::push_document(std::move(recoveryDocument),false);
                    const auto initialRecovery=guestInput.recoveryStatus();
                    if(initialRecovery.at("paused")!=false||initialRecovery.at("remainingMs")!=0||guestRecoveryWindow->pump())
                        throw std::runtime_error("Fresh committed Join must not show recovery or grant a recovery deadline");
                    guestRecoveryWindow->detach();
                    if(!PartyBoardNativeOnlineInputRequired()||PartyBoard_GameLifecyclePhase()!=2||PartyBoard_GameFrameCount()!=0)throw std::runtime_error("Guest input installation changed stopped engine");
                    PartyBoardInputState before{};if(!PartyBoardCaptureInputState(&before))throw std::runtime_error("Actual guest input state unavailable");
                    partyboard::net::SessionId inputSession{};
                    const auto sessionHex=preparedJoin->commit().at("session").get<std::string>();
                    for(size_t i=0;i<inputSession.size();++i)inputSession[i]=uint8_t(std::stoul(sessionHex.substr(i*2,2),nullptr,16));
                    PADStatus pads[4]{};pads[0].button=PAD_BUTTON_A;pads[0].stickX=31;pads[0].triggerRight=42;pads[0].extButton=1;
                    const auto expectedLocal=partyboard::net::packPad(pads[0]);
                    PADStatus hostPad{};hostPad.button=PAD_BUTTON_B;hostPad.stickY=-23;hostPad.triggerLeft=17;
                    const auto expectedHost=partyboard::net::packPad(hostPad);
                    struct JoinPadReceipt {std::mutex mutex;std::optional<partyboard::net::Input> packet;bool invalid=false;};
                    const auto hostPadReceipt=std::make_shared<JoinPadReceipt>();
                    const auto inputChannel=hostGuest->inputChannel();
                    inputChannel->onMessage([hostPadReceipt](rtc::message_variant message){
                        std::lock_guard lock(hostPadReceipt->mutex);
                        const auto* bytes=std::get_if<rtc::binary>(&message);
                        if(!bytes){hostPadReceipt->invalid=true;return;}
                        const auto decoded=partyboard::net::decode(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes->data()),bytes->size()));
                        if(!decoded||hostPadReceipt->packet){hostPadReceipt->invalid=true;return;}
                        hostPadReceipt->packet=*decoded;
                    });
                    const auto remotePacket=partyboard::net::encode({inputSession,before.frame,before.checkpoint,0,expectedHost});
                    inputChannel->send(reinterpret_cast<const rtc::byte*>(remotePacket.data()),remotePacket.size());
                    if(PartyBoardNetDiagnosticSample(pads)!=1)throw std::runtime_error("Actual guest engine PAD hook sample rejected");
                    bool hostSawPad=false;
                    while(std::chrono::steady_clock::now()<sampleDeadline){
                        {std::lock_guard lock(hostPadReceipt->mutex);
                         if(hostPadReceipt->invalid)throw std::runtime_error("Unexpected guest PAD packet at host");
                         if(hostPadReceipt->packet){
                             const auto& packet=*hostPadReceipt->packet;
                             if(packet.session!=inputSession||packet.slot!=1||packet.frame!=before.frame||packet.checkpoint!=before.checkpoint||packet.pad!=expectedLocal)throw std::runtime_error("Actual local guest PAD packet differs");
                             hostSawPad=true;break;
                         }}
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    inputChannel->resetCallbacks();
                    if(!hostSawPad||partyboard::net::packPad(pads[0])!=expectedHost||partyboard::net::packPad(pads[1])!=expectedLocal||pads[1].extButton!=0||pads[2].err!=PAD_ERR_NO_CONTROLLER||pads[3].err!=PAD_ERR_NO_CONTROLLER)throw std::runtime_error("Actual guest PAD merge/ownership failed");
                    PartyBoardInputState after{};
                    if(!PartyBoardCaptureInputState(&after)||after.frame!=before.frame||after.checkpoint!=before.checkpoint||after.frand!=before.frand||after.rand8!=before.rand8||guestInput.observations().acceptedCount()!=1)throw std::runtime_error("Guest sample changed engine state or accepted count");
                    const auto& observation=guestInput.observations().entries().front();
                    fprintf(stderr,"Native Join actual engine PAD observation: %s\n",nlohmann::ordered_json{{"frame",observation.state.frame},{"checkpoint",observation.state.checkpoint},{"frand",observation.state.frand},{"rand8",observation.state.rand8},{"pads",observation.pads}}.dump().c_str());
                    joinInputSampled=true;
                    if(!guestInput.closeStopped()||PartyBoardNativeOnlineInputRequired())throw std::runtime_error("Guest stopped input detach failed");
                    joinInputInstalled=true;key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);stage=5;
                }
            }
            return true;
        });
        const bool passed=active&&lobby->cancelled()&&updates>=20&&PartyBoard_GameLifecyclePhase()==(probeJoinPrepared?2:0)&&PartyBoard_GameFrameCount()==0&&(!probeJoinPrepared||(enginePrepared&&hostReceiptAccepted&&hostReadyAccepted&&joinStartAccepted&&joinInputInstalled&&joinInputSampled))&&(probeJoinConnected?(verified&&stage==5):!lobby->session());
        fprintf(stderr,"Native Join actual engine PAD hook sample accepted: %d\n",joinInputSampled);
        fprintf(stderr,"Native Join input installed and detached while stopped: %d\n",joinInputInstalled);
        fprintf(stderr,"Native Join Start agreement accepted while stopped: %d\n",joinStartAccepted);
        fprintf(stderr,"Native Join Ready report accepted: %d\n",hostReadyAccepted);
        fprintf(stderr,"Native Join lobby UI probe: %s updates=%u cancelled=%d phase=%d connected=%d stage=%u verified=%d receiptAccepted=%d\n",passed?"PASS":"FAIL",updates,lobby->cancelled(),PartyBoard_GameLifecyclePhase(),probeJoinConnected,stage,verified,hostReceiptAccepted);
        if(joinPreparation)joinPreparation->detachStopped();joinPreparation.reset();preparedJoin=nullptr;partyboard::ui::shutdown();if(hostGuest)hostGuest->close();hostGuest.reset();hostControls.close();hostAsset.reset();
        if(probeJoinConnected&&!probeJoinPrepared){std::error_code ignored;std::filesystem::remove(sourcePath,ignored);}
        HuAudPortStreamShutdown();aurora_shutdown();cleanup.done=true;return passed?0:2;
      }catch(const std::exception& error){fprintf(stderr,"Native Join lobby UI probe failed: %s\n",error.what());return 2;}
    }
#endif

    // Invalidate a bad saved isoPath so that Party Board can't get blocked from starting up.
    // This is only a metadata check; full hash verification is handled by the prelaunch UI.
    bool forcePreLaunchUI = false;
    bool saveConfigBeforePrelaunch = false;

    const std::string p = partyboard::getSettings().backend.isoPath;
    partyboard::iso::DiscInfo discInfo{};
    if (!p.empty() &&
        partyboard::iso::inspect(p.c_str(), discInfo) != partyboard::iso::ValidationError::Success)
    {
        PartyBoardMainLog.info("Saved DVD image path failed validation, clearing configured path: {}", p);
        partyboard::getSettings().backend.isoPath.setValue("");
        partyboard::getSettings().backend.isoVerification.setValue(partyboard::DiscVerificationState::Unknown);
        forcePreLaunchUI = true;
        saveConfigBeforePrelaunch = true;
    }

    partyboard::iso::log_verification_state(
        partyboard::getSettings().backend.isoPath.getValue(),
        partyboard::getSettings().backend.isoVerification.getValue());

    if (partyboard::getSettings().backend.isoPath.getValue().empty()) {
        forcePreLaunchUI = true;
    }
    if (forcePreLaunchUI && partyboard::getSettings().backend.skipPreLaunchUI.getValue()) {
        PartyBoardMainLog.info("Prelaunch UI was disabled with no usable DVD image, enabling prelaunch UI");
        partyboard::getSettings().backend.skipPreLaunchUI.setValue(false);
        saveConfigBeforePrelaunch = true;
    }
    if (saveConfigBeforePrelaunch) {
        partyboard::config::Save();
    }

    if(probeHostHandoff&&onlineStartupAction==0)probeHostLobby=true;
    if (!partyboard::getSettings().backend.skipPreLaunchUI && !probeHostLobby && !probeHostHandoff && !probeJoinHandoff && onlineStartupAction!=0) {
        partyboard::ui::push_document(std::make_unique<partyboard::ui::Prelaunch>(), true);

        // pre game launch ui main loop
        if (!launchUILoop()) {
            fflush(stdout);
            fflush(stderr);
#ifdef PARTY_BOARD_DISCORD
            partyboard::discord::shutdown();
#endif
            partyboard::ui::shutdown();
            aurora_shutdown();
            return 0;
        }
    }

     std::string dvd_path = partyboard::getSettings().backend.isoPath;
#ifdef PARTYBOARD_NATIVE_RTC
    if(probeHostLobby||onlineStartupAction==0){
        auto document=std::make_unique<partyboard::ui::NativeHostLobby>(dvd_path,lobbyPackagePath,probeHostGameplay&&std::getenv("PARTYBOARD_PROBE_DESCRIPTOR_SEED")&&std::string_view(std::getenv("PARTYBOARD_PROBE_DESCRIPTOR_SEED"))=="1",onlineSessionEndedNotice,(probeHostLobby||automateHostRecovery)&&!automaticLobbyTest);
        document->setPipelineSeed(sharedPipelineSeed);
        auto* lobby=document.get();
        partyboard::ui::push_document(std::move(document),true);
        bool enginePrepared=false;
        std::unique_ptr<partyboard::net::NativeHostGameInput> gameInput;
        partyboard::ui::NativeRecoveryWindow* recoveryWindow=nullptr;
        bool onlineWindowClosed=false;
        bool inputCommitted=false; std::vector<std::string> diagnosticGuests; std::set<std::string> diagnosticInvitations; bool diagnosticStopped=false,diagnosticOpened=false,diagnosticComplete=false,diagnosticReceiptsEmitted=false; unsigned diagnosticReplies=0;
        auto nextDiagnosticTransferState=std::chrono::steady_clock::now();
        std::set<std::string> diagnosticRecoveryPeers,diagnosticRecoveryOffers,diagnosticRecoveryAnswers,diagnosticRecoveryVerifying,diagnosticRecoveryVerified;
        bool diagnosticRecoveryPaused=false;
        bool automaticLinkEmitted=false;uint64_t nextAutomaticObservation=0;
#ifdef _WIN32
        partyboard::net::NativeProbePipe diagnosticPipe;
#endif
        std::string enginePreparationError;
        const bool fullMatchProbe=probeHostGameplay&&std::getenv("PARTYBOARD_NATIVE_FULL_MATCH")&&std::string_view(std::getenv("PARTYBOARD_NATIVE_FULL_MATCH"))=="1";
        partyboard::net::NativeFullMatchProgress fullMatchProgress(NativeFullMatchBoard(),NativeFullMatchTurns(),DLL_mstory3Dll);
        const bool focusedPlayProbe=fullMatchProbe&&std::getenv("PARTYBOARD_NATIVE_FOCUSED_MINIGAME")&&std::string_view(std::getenv("PARTYBOARD_NATIVE_FOCUSED_MINIGAME"))=="443";
        bool focusedStopAcknowledged=false;
        bool fullMatchComplete=false;
        uint64_t nextFullMatchProgressFrame=0;
        partyboard::net::NativeDiagnosticPad diagnosticPad;
        const bool observeHostGameplay=probeHostGameplay||automateHostRecovery;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(fullMatchProbe?21600:(probeHostConnected||automateHostRecovery)?1200:60);
        bool sessionEndedNoticeSeen=false;
        bool recoveryUiSeen=false;
        const bool active=nativeUILoop([&]{
            if((probeHostLobby||returnedHostMenuAcceptance)&&onlineSessionEndedNotice)sessionEndedNoticeSeen|=lobby->sessionEndedNoticeVisible();
            if((!probeHostLobby||probeHostPreparation)&&!lobbyPackagePath.empty()&&lobby->prepared()&&!enginePrepared&&enginePreparationError.empty()){
                try{
                    if(!aurora_dvd_open(dvd_path.c_str()))throw std::runtime_error("Native lobby disc could not open");
                    partyboard::version::init();LanguageInit();InitializeDol();
                    const auto& package=lobby->measuredPackage();
                    const auto& configuration=package.at("configuration");
                    const auto prepared=partyboard::net::initializeNativeConfiguredEngine(configuration,configuration.at("assetId"),package.at("buildId"));
                    partyboard::net::prepareNativeRenderer(prepared,[&]{
                        const auto* event=aurora_update();
                        while(event&&event->type!=AURORA_NONE){
                            if(event->type==AURORA_EXIT)return false;
                            if(event->type==AURORA_SDL_EVENT)partyboard::ui::handle_event(event->sdl);
                            ++event;
                        }
                        partyboard::ui::update();
                        return !lobby->cancelled()&&lobby->error().empty();
                    });
                    const auto receipt=partyboard::net::nativePreparedRuntimeReport(prepared,configuration,true,installedPipelineSeed);
                    lobby->publishHostPrepared(partyboard::net::captureNativePreparedStart(prepared.assetId,prepared.buildId),receipt,installedPipelineSeed);
                    if(probeHostPreparation){
                        lobby->probeUnacknowledgedInput(prepared);
                        lobby->toggleReady();
                        if(!lobby->hostReady())throw std::runtime_error("Native prepared host ready toggle failed");
                        lobby->toggleReady();
                        if(lobby->hostReady())throw std::runtime_error("Native prepared host unready toggle failed");
                        printf("Native owned lobby actual ready/unready toggle: PASS frame=%u\n",PartyBoard_GameFrameCount());
                    }
                    printf("Native owned lobby prepared receipt: %s\n",receipt.dump().c_str());
                    enginePrepared=true;
                }catch(const std::exception& error){enginePreparationError=error.what();}
            }
            if((probeHostInvitations||automateHostRecovery)&&enginePrepared&&!automaticLobbyTest){
                if(diagnosticGuests.empty())for(unsigned slot=0;slot<3;++slot)diagnosticGuests.push_back(lobby->diagnosticAssignGuest());
                for(const auto& peer:diagnosticGuests)if(!diagnosticInvitations.contains(peer))if(const auto invitation=lobby->diagnosticInvitation(peer)){
                    printf("Native frontend invitation: %s\n",invitation->dump().c_str());fflush(stdout);diagnosticInvitations.insert(peer);
                }
            }
            if(enginePrepared&&(!probeHostLobby||probeHostGameplay)&&lobby->startAgreed()&&!inputCommitted){
                try{
                    const auto& package=lobby->measuredPackage();
                    const auto captured=partyboard::net::captureNativePreparedStart(package.at("configuration").at("assetId"),package.at("buildId"));
                    gameInput=lobby->createGameInput(captured,[&]{
                        #ifdef _WIN32
                        if(observeHostGameplay){
                            const auto recovery=gameInput->recoveryStatus();
                            if(recovery.at("paused").get<bool>()!=diagnosticRecoveryPaused){
                                diagnosticRecoveryPaused=recovery.at("paused").get<bool>();
                                printf("Native frontend event: %s\n",nlohmann::ordered_json{{"type","recovery-status"},{"status",recovery}}.dump().c_str());fflush(stdout);
                            }
                            for(const auto& line:diagnosticPipe.poll()){
                                if(line=="stop"){
                                    focusedStopAcknowledged=focusedPlayProbe;
                                    printf("Native frontend event: {\"type\":\"diagnostic-stop-reason\",\"reason\":\"runner-stop\"}\n");fflush(stdout);
                                    diagnosticStopped=true;PartyBoard_GameRequestStop();return false;
                                }
                                if(automaticLobbyTest&&line=="observe"){
                                    nlohmann::ordered_json rows=nlohmann::ordered_json::array();for(const auto& sample:gameInput->observations().entries())rows.push_back({{"frame",sample.state.frame},{"checkpoint",sample.state.checkpoint},{"frand",sample.state.frand},{"rand8",sample.state.rand8},{"pads",sample.pads}});
                                    printf("Native frontend event: %s\n",nlohmann::ordered_json{{"type","automatic-game-observation"},{"accepted",gameInput->observations().acceptedCount()},{"records",rows},{"recovery",gameInput->recoveryStatus()},{"error",gameInput->recoveryError()}}.dump().c_str());fflush(stdout);continue;
                                }
                                const auto command=nlohmann::ordered_json::parse(line);
                                const auto type=command.at("type").get<std::string>();
                                if(type=="diagnostic-local-input"&&fullMatchProbe){diagnosticPad.accept(command);continue;}
                                if(type=="diagnostic-observe"&&fullMatchProbe){
                                    auto observation=NativeMatchObservation(command,gameInput->observations());
                                    observation["inputDiagnostics"]=gameInput->recoveryStatus();
                                    observation["inputTiming"]=partyboard::net::nativeInputTimingJson(gameInput->inputTiming());
                                    printf("Native frontend event: %s\n",observation.dump().c_str());fflush(stdout);continue;
                                }
                                const auto peer=command.at("peer").get<std::string>();
                                if(type=="recovery-begin"){
                                    if(diagnosticRecoveryPeers.contains(peer)&&!diagnosticRecoveryVerified.contains(peer))throw std::runtime_error("Original probe replacement already pending");
                                    gameInput->beginReplacement(peer);diagnosticRecoveryPeers.insert(peer);
                                    diagnosticRecoveryOffers.erase(peer);diagnosticRecoveryAnswers.erase(peer);
                                    diagnosticRecoveryVerifying.erase(peer);diagnosticRecoveryVerified.erase(peer);
                                }else if(type=="recovery-answer"){
                                    if(!diagnosticRecoveryOffers.contains(peer)||diagnosticRecoveryAnswers.contains(peer))throw std::runtime_error("Published unanswered probe replacement required");
                                    gameInput->replacement(peer).acceptAnswer(command.at("answer"),partyboard::net::NativeEngineInputIO::monotonicNow());diagnosticRecoveryAnswers.insert(peer);
                                }else if(type=="recovery-resume"){
                                    if(!gameInput->replacementVerified(peer))throw std::runtime_error("Verified original replacement required before recovery agreement");
                                    gameInput->beginRecoveryAgreement(command.at("owner").get<uint32_t>());
                                }else throw std::runtime_error("Unexpected native gameplay probe command");
                            }
                            for(const auto& peer:diagnosticRecoveryPeers){
                                auto& replacement=gameInput->replacement(peer);const auto time=partyboard::net::NativeEngineInputIO::monotonicNow();
                                if(!diagnosticRecoveryOffers.contains(peer))if(const auto offer=replacement.offer(time)){
                                    printf("Native frontend event: %s\n",nlohmann::ordered_json{{"type","recovery-offer"},{"peer",peer},{"offer",*offer}}.dump().c_str());fflush(stdout);diagnosticRecoveryOffers.insert(peer);
                                }
                                if(diagnosticRecoveryAnswers.contains(peer)&&!diagnosticRecoveryVerifying.contains(peer)&&replacement.ready(time)){
                                    gameInput->beginReplacementVerification(peer);diagnosticRecoveryVerifying.insert(peer);
                                }
                                if(gameInput->replacementVerified(peer)&&!diagnosticRecoveryVerified.contains(peer)){
                                    printf("Native frontend event: %s\n",nlohmann::ordered_json{{"type","recovery-verified"},{"peer",peer},{"status",gameInput->recoveryStatus()}}.dump().c_str());fflush(stdout);diagnosticRecoveryVerified.insert(peer);
                                }
                            }
                        }
                        #endif
                        if(fullMatchProbe){
                            EmitNativeFullMatchProgress("Native frontend event",nextFullMatchProgressFrame);
                            fullMatchComplete=fullMatchProgress.observe(PartyBoard_GameFrameCount(),PartyBoard_GameProgress(2),PartyBoard_GameProgress(0),PartyBoard_GameProgress(1),PartyBoard_GameProgress(4),PartyBoard_GameProgress(5));
                        }
                        if(probeHostGameplay&&((!automaticLobbyTest&&!fullMatchProbe&&gameInput->observations().acceptedCount()>=60)||std::chrono::steady_clock::now()>=deadline)){
                            printf("Native frontend event: %s\n",nlohmann::ordered_json{{"type","diagnostic-stop-reason"},{"reason",std::chrono::steady_clock::now()>=deadline?"deadline":"short-probe-complete"}}.dump().c_str());fflush(stdout);
                            PartyBoard_GameRequestStop();return false;
                        }
                        const auto* event=aurora_update();
                        while(event&&event->type!=AURORA_NONE){
                            if(event->type==AURORA_EXIT){
                                if(probeHostGameplay){printf("Native frontend event: {\"type\":\"diagnostic-stop-reason\",\"reason\":\"window-exit\"}\n");fflush(stdout);}
                                onlineWindowClosed=true;PartyBoard_GameRequestStop();return false;
                            }
                            if(recoveryWindow&&recoveryWindow->visible()&&event->type==AURORA_SDL_EVENT)partyboard::ui::handle_event(event->sdl);
                            ++event;
                        }
                        if(recoveryWindow){
                            const bool redraw=recoveryWindow->pump();
                            if(automateHostRecovery&&recoveryWindow->visible())recoveryUiSeen=true;
                            if(redraw&&aurora_begin_frame()){
                                partyboard::ui::update();aurora_end_frame();
                            }
                        }
                        return true;
                    });
                    if(automaticLobbyTest)gameInput->useRollingDiagnosticObservations();
                    if(fullMatchProbe){gameInput->useRollingDiagnosticObservations();gameInput->setDiagnosticLocalInput([&](const auto&,auto* pads){diagnosticPad.sample(pads);});}
                    if(!gameInput->installAndDeliver(captured))throw std::runtime_error("Native start input commit delivery failed");
                    inputCommitted=true;
                    if(!probeHostLobby||automaticLobbyTest){
                        auto recoveryDocument=std::make_unique<partyboard::ui::NativeRecoveryWindow>(*gameInput,lobby->automaticRoom());
                        recoveryWindow=recoveryDocument.get();partyboard::ui::push_document(std::move(recoveryDocument),false);
                    }
#ifdef _WIN32
                    if(automateHostRecovery){
                        printf("Native frontend event: %s\n",nlohmann::ordered_json({{"type","ordinary-host-lifecycle"},{"pid",GetCurrentProcessId()},{"probeHostLobby",probeHostLobby},{"recoveryWindowCreated",recoveryWindow!=nullptr}}).dump().c_str());fflush(stdout);
                    }
#endif
                }catch(const std::exception& error){enginePreparationError=error.what();}
            }
            #ifdef _WIN32
            if(automaticLobbyTest&&enginePrepared){
                try{
                    if(auto* room=lobby->automaticRoom();room&&room->ready()&&!automaticLinkEmitted){printf("Native frontend event: %s\n",nlohmann::ordered_json{{"type","automatic-lobby-link"},{"link",room->joinLink()},{"automatic",lobby->automatic()}}.dump().c_str());fflush(stdout);automaticLinkEmitted=true;}
                    for(const auto& command:diagnosticPipe.poll()){
                        if(command=="stop"){diagnosticStopped=true;continue;}
                        if(command=="start"){lobby->diagnosticPressAutomaticStart();continue;}
                        throw std::runtime_error("Automatic frontend probe accepts only Start/Stop UI commands");
                    }
                    const auto now=partyboard::net::NativeEngineInputIO::monotonicNow();
                    if(now>=nextAutomaticObservation){printf("Native frontend event: %s\n",nlohmann::ordered_json{{"type","automatic-lobby-state"},{"snapshot",lobby->diagnosticSnapshot()},{"prepared",lobby->diagnosticReceiptsComplete()},{"roomError",lobby->automaticRoom()?lobby->automaticRoom()->error():std::string{}}}.dump().c_str());fflush(stdout);nextAutomaticObservation=now+1000;}
                }catch(const std::exception& error){enginePreparationError=error.what();}
            }
            if((probeHostConnected||automateHostRecovery)&&enginePrepared&&!automaticLobbyTest){
                try{
                    for(const auto& command:diagnosticPipe.poll()){
                        if(command=="stop"){diagnosticStopped=true;continue;}
                        if(command=="ready"){
                            if(!diagnosticReceiptsEmitted||lobby->hostReady())throw std::runtime_error("Native probe ready requires complete receipts and an unready host");
                            lobby->toggleReady();continue;
                        }
                        if(command=="start"){
                            if(!diagnosticReceiptsEmitted)throw std::runtime_error("Native probe start requires complete receipts");
                            lobby->proposeStart();continue;
                        }
                        if(diagnosticReplies>=diagnosticGuests.size())throw std::runtime_error("Unexpected native probe guest reply");
                        lobby->diagnosticAcceptReply(diagnosticGuests[diagnosticReplies++],command);
                    }
                    const auto emit=[](const nlohmann::ordered_json& packet){printf("Native frontend event: %s\n",packet.dump().c_str());fflush(stdout);};
                    if(!diagnosticOpened&&std::all_of(diagnosticGuests.begin(),diagnosticGuests.end(),[&](const auto& peer){return lobby->diagnosticConnected(peer);})){diagnosticOpened=true;emit({{"type","open"}});}
                    const auto snapshot=lobby->diagnosticSnapshot();
                    if(diagnosticOpened&&std::chrono::steady_clock::now()>=nextDiagnosticTransferState){
                        nlohmann::ordered_json transfers=nlohmann::ordered_json::array();
                        for(const auto& peer:diagnosticGuests)transfers.push_back({{"peer",peer},{"sentBytes",lobby->diagnosticSentBytes(peer)},{"senderComplete",lobby->diagnosticAssetComplete(peer)}});
                        emit({{"type","diagnostic-transfer-state"},{"uiUpdates",lobby->diagnosticUpdates()},{"transfers",transfers},{"lobbySnapshot",snapshot},{"connectionRoutes",lobby->diagnosticConnectionRoutes()}});
                        nextDiagnosticTransferState=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                    }
                    if(!diagnosticComplete&&diagnosticOpened&&std::all_of(diagnosticGuests.begin(),diagnosticGuests.end(),[&](const auto& peer){return lobby->diagnosticAssetComplete(peer);})&&std::all_of(snapshot.at("players").begin(),snapshot.at("players").end(),[](const auto& player){return player.at("verified")==true;})){
                        diagnosticComplete=true;nlohmann::ordered_json sent=nlohmann::ordered_json::array();for(const auto& peer:diagnosticGuests)sent.push_back(lobby->diagnosticSentBytes(peer));
                        emit({{"type","complete"},{"lobbySnapshot",snapshot},{"sentBytes",sent}});
                    }
                    if(!diagnosticReceiptsEmitted&&lobby->diagnosticReceiptsComplete()){diagnosticReceiptsEmitted=true;emit({{"type","prepared-guests"},{"receipts",lobby->diagnosticReceipts()},{"frame",PartyBoard_GameFrameCount()},{"phase",PartyBoard_GameLifecyclePhase()}});}
                    if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Connected native frontend probe timed out");
                }catch(const std::exception& error){enginePreparationError=error.what();}
            }
#endif
            // Publishing preparation happens in this callback, before the next
            // UI update applies automatic readiness. Observe that update before
            // ending the returned-menu check; manual lobbies stay unready.
            if(returnedHostMenuAcceptance&&enginePrepared&&sessionEndedNoticeSeen&&lobby->updates()>=30&&lobby->hostReady()==lobby->automatic())return false;
            return !diagnosticStopped&&!inputCommitted&&(enginePrepared||PartyBoard_IsRunning)&&enginePreparationError.empty()&&!lobby->cancelled()&&lobby->error().empty()&&
            (!probeHostLobby||probeHostConnected||((lobby->updates()<30||!lobby->prepared()||(probeHostPreparation&&!enginePrepared)||(probeHostInvitations&&diagnosticInvitations.size()!=3))&&std::chrono::steady_clock::now()<deadline));});
        bool passed=active&&(!probeHostGameplay||inputCommitted)&&(!probeHostConnected||(diagnosticComplete&&(diagnosticStopped||inputCommitted)))&&(!probeHostInvitations||diagnosticInvitations.size()==3)&&lobby->prepared()&&lobby->updates()>=30&&lobby->error().empty()&&(probeHostPreparation?(enginePrepared&&PartyBoard_GameLifecyclePhase()==2&&PartyBoard_GameFrameCount()==0):PartyBoard_GameLifecyclePhase()==0)&&(!onlineSessionEndedNotice||sessionEndedNoticeSeen);
        if(automaticLobbyTest)passed=active&&inputCommitted&&lobby->automatic()&&lobby->error().empty()&&enginePreparationError.empty();
        if(probeHostLobby&&onlineSessionEndedNotice){printf("Native menu notice: %s\n",nlohmann::ordered_json({{"menu","host"},{"seenVisible",sessionEndedNoticeSeen},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()},{"text",partyboard::net::NativeSessionEndedNotice}}).dump().c_str());fflush(stdout);}
#ifdef _WIN32
        if(returnedHostMenuAcceptance){
            const auto players=lobby->prepared()?lobby->diagnosticSnapshot().at("players"):nlohmann::ordered_json::array();
            const bool menuPrepared=active&&enginePrepared&&!inputCommitted&&sessionEndedNoticeSeen&&lobby->prepared()&&lobby->error().empty()&&enginePreparationError.empty()&&PartyBoard_GameLifecyclePhase()==2&&PartyBoard_GameFrameCount()==0&&players.size()==1&&players[0].at("peer")=="host"&&(lobby->hostReady()==lobby->automatic())&&!lobby->startAgreed();
            std::ofstream report(PartyBoard_ConfigPath/"recovery-returned-host-menu.json");
            report<<nlohmann::ordered_json({{"pid",GetCurrentProcessId()},{"passed",menuPrepared},{"menu","host"},{"seenVisible",sessionEndedNoticeSeen},{"enginePrepared",enginePrepared},{"inputCommitted",inputCommitted},{"probeHostLobby",probeHostLobby},{"players",players},{"hostReady",lobby->hostReady()},{"automatic",lobby->automatic()},{"active",active},{"lobbyError",lobby->error()},{"enginePreparationError",enginePreparationError},{"startAgreed",lobby->startAgreed()},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()},{"buildId",menuPrepared?lobby->measuredPackage().at("buildId"):nlohmann::ordered_json(nullptr)},{"text",partyboard::net::NativeSessionEndedNotice}}).dump()<<"\n";
            if(!menuPrepared||!report)enginePreparationError="Returned Host menu was not prepared or its report failed";
        }
#endif

        const bool failed=!lobby->error().empty()||!enginePreparationError.empty();
        if(active&&!failed&&inputCommitted){
            lobby->hide(false);
            const int execution=PartyBoard_GameResume();
            PartyBoard_GameRequestStop();
            if(execution!=0)enginePreparationError="Native committed game execution failed";
        }
        if(observeHostGameplay&&inputCommitted&&gameInput){
            const auto& trace=gameInput->observations();
            nlohmann::ordered_json samples=nlohmann::ordered_json::array();
            for(const auto& sample:trace.entries())samples.push_back({{"frame",sample.state.frame},{"checkpoint",sample.state.checkpoint},{"frand",sample.state.frand},{"rand8",sample.state.rand8},{"pads",sample.pads}});
            passed=passed&&(fullMatchProbe?(focusedPlayProbe?(focusedStopAcknowledged&&trace.acceptedCount()>=120):fullMatchComplete):automaticLobbyTest?trace.acceptedCount()>=120:trace.acceptedCount()==60)&&enginePreparationError.empty();
            if(fullMatchProbe){printf("Native frontend event: %s\n",nlohmann::ordered_json({{"type","full-match"},{"complete",fullMatchComplete},{"focusedMinigame",focusedPlayProbe},{"observedTurns",fullMatchProgress.observedTurnCount()},{"renderedRankingsVerified",false}}).dump().c_str());fflush(stdout);}
            printf("Native frontend event: %s\n",nlohmann::ordered_json({{"type","gameplay"},{"accepted",trace.acceptedCount()},{"observations",samples},{"recoveryError",gameInput->recoveryError()},{"inputTiming",partyboard::net::nativeInputTimingJson(gameInput->inputTiming())},{"phase",PartyBoard_GameLifecyclePhase()}}).dump().c_str());fflush(stdout);
            #ifdef _WIN32
            if(passed&&std::getenv("PARTYBOARD_NATIVE_DIAGNOSTIC_FINISH_ACK")){
                // Keep already-sent final PAD data deliverable while the
                // diagnostic runner verifies every guest's final poll.
                // The engine stays stopped; this grants no simulation input.
                bool acknowledged=false;
                const auto finishDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
                while(!acknowledged&&std::chrono::steady_clock::now()<finishDeadline){
                    for(const auto& command:diagnosticPipe.poll()){
                        if(command=="finish")acknowledged=true;
                        else {enginePreparationError="Unexpected stopped diagnostic command";break;}
                    }
                    if(!enginePreparationError.empty())break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                if(!acknowledged){passed=false;enginePreparationError="All-guest diagnostic finish acknowledgment missing";}
            }
            #endif
        }
        if(recoveryWindow)recoveryWindow->detach();
        const bool recoveryEndedSession=gameInput&&gameInput->recoveryStatus().value("returnToMenu",false);
        if(gameInput&&!gameInput->closeStopped())throw std::runtime_error("Native game input detach failed; retain runtime owner");
        if(observeHostGameplay&&inputCommitted){
            printf("Native frontend event: %s\n",nlohmann::ordered_json({{"type","game-stopped"},{"detached",true},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()}}).dump().c_str());fflush(stdout);
        }
        gameInput.reset();
#ifdef _WIN32
        if(automateHostRecovery){
            printf("Native frontend event: %s\n",nlohmann::ordered_json({{"type","ordinary-host-ended"},{"pid",GetCurrentProcessId()},{"recoveryEndedSession",recoveryEndedSession},{"recoveryUiSeen",recoveryUiSeen},{"probeHostLobby",probeHostLobby}}).dump().c_str());fflush(stdout);
        }
#endif
        // Ending or expiring an online session returns to a fresh host lobby.
        // The entry point restarts only after all old engine/UI/RTC owners have
        // shut down, preserving the existing process-reset lifecycle.
        if(!probeHostLobby&&inputCommitted&&!onlineWindowClosed&&!PartyBoard_GameWindowExitRequested()&&enginePreparationError.empty()){
#ifdef _WIN32
            try{partyboard::net::publishNativeSessionEndedNotice(recoveryEndedSession);}
            catch(const std::exception& error){enginePreparationError=error.what();}
#endif
            if(enginePreparationError.empty()){nativeRestartOnlineAction=0;PartyBoard_RestartRequested=true;}
        }
        if(probeHostLobby)fprintf(stderr,"Native host lobby UI probe: %s updates=%u fresh-phase=%d startup-action=%d\n",passed?"PASS":"FAIL",lobby->updates(),PartyBoard_GameLifecyclePhase(),onlineStartupAction);
#ifdef _WIN32
        if(probeHostHandoff){
            std::ofstream report(PartyBoard_ConfigPath/"host-handoff-child.txt");
            report<<GetCurrentProcessId()<<" "<<passed<<" "<<onlineStartupAction<<" "<<PartyBoard_GameLifecyclePhase()<<" "<<lobby->updates()<<"\n";
        }
#endif
        if(failed)fprintf(stderr,"Native host lobby failed: %s\n",enginePreparationError.empty()?lobby->error().c_str():enginePreparationError.c_str());
        partyboard::ui::shutdown();HuAudPortStreamShutdown();aurora_shutdown();
        return probeHostLobby?(passed?0:2):((failed||!enginePreparationError.empty())?2:0);
    }
#endif

    if (dvd_path.empty()) {
        PartyBoardMainLog.error("No DVD image specified, unable to boot!");
    }
    if (!PartyBoard_IsGameLaunched &&
        partyboard::iso::inspect(dvd_path.c_str(), discInfo) != partyboard::iso::ValidationError::Success)
    {
        PartyBoardMainLog.error("DVD image failed validation: {}", dvd_path);
    }
    PartyBoardMainLog.info("Loading DVD image: {}", dvd_path);
    if (!aurora_dvd_open(dvd_path.c_str())) {
        PartyBoardMainLog.error("Failed to open DVD image: {}", dvd_path);
    }

    PartyBoard_IsGameLaunched = true;

    if (!partyboard::getSettings().backend.wasPresetChosen) {
        partyboard::ui::push_document(std::make_unique<partyboard::ui::PresetWindow>());
    }

    partyboard::version::init();
    LanguageInit();

    // OSInit();

    // Reset Data
    // TODO PC
    // static mDoRstData sResetData = {0};
    // mDoRst::setResetData(&sResetData);
    // mDoRst::offReset();
    // mDoRst::setLogoScnFlag(0);

    if (traceAfterInit) {
        PartyBoardRandomSeed(simulationSeed);
    } else if (!tracePath.empty()) {
        if (!PartyBoardTraceOpen(tracePath.c_str(), traceReplay, simulationSeed)) {
            partyboard::ui::shutdown();
            aurora_shutdown();
            return 2;
        }
        PartyBoardTraceSetRecordLimit(recordSamples);
        if (!inputScript.empty() && !PartyBoardTraceScriptOpen(inputScript.c_str())) {
            PartyBoardTraceClose();
            partyboard::ui::shutdown();
            aurora_shutdown();
            return 2;
        }
    } else if (seedSpecified && !prepareOnlineEngine) {
        PartyBoardRandomSeed(simulationSeed);
    }
    if (netRole && !PartyBoardNetDiagnosticOpen(netRole == 1, (unsigned short)netPort, netConfig.c_str(), simulationSeed)) {
        PartyBoardTraceClose();
        partyboard::ui::shutdown(); aurora_shutdown(); return 2;
    }
    InitializeDol();

    if (traceFast) {
        PartyBoardTraceFastMode();
        fprintf(stderr, "Input trace: fast diagnostic mode (frame limiter disabled)\n");
    }

    int nativePreparationResult = 0;
    if (prepareOnlineEngine) {
        try {
            // Local lifecycle diagnostic; no fabricated asset/build identity or
            // shader readiness, network attachment or simulation authorization.
            partyboard::net::PreparedStartRuntime prepared;
#ifdef PARTYBOARD_NATIVE_RTC
            if(!measuredSessionPath.empty()){
                if(std::filesystem::file_size(measuredSessionPath)>262144)throw std::runtime_error("Measured session descriptor exceeds bound");
                std::ifstream source(measuredSessionPath,std::ios::binary);
                nlohmann::ordered_json descriptor;source>>descriptor;
                const auto configuration=descriptor.at("configuration");
                std::vector<partyboard::net::NativeRuntimeArtifact> files;
                for(const auto& file:descriptor.at("files"))files.push_back({file.at("name").get<std::string>(),partyboard::net::nativeRuntimeArtifactPath(measuredSessionPath,file.at("path").get<std::string>())});
                partyboard::net::NativeAsset asset(partyboard::getSettings().backend.isoPath.getValue(),"Game assets");
                const auto measured=partyboard::net::measureNativeSessionPackage(std::move(files),configuration.at("buildId").get<std::string>(),
                    configuration.at("session").get<std::string>(),configuration.at("seed").get<uint32_t>(),asset.manifest().at("id").get<std::string>());
                if(nlohmann::json(measured.at("configuration"))!=nlohmann::json(configuration))throw std::runtime_error("Measured session descriptor differs from local package/assets");
                prepared=partyboard::net::initializeNativeConfiguredEngine(configuration,asset.manifest().at("id").get<std::string>(),measured.at("buildId").get<std::string>());
                partyboard::net::prepareNativeRenderer(prepared,[]{
                    const auto* event=aurora_update();
                    while(event&&event->type!=AURORA_NONE){
                        if(event->type==AURORA_EXIT)return false;
                        if(event->type==AURORA_SDL_EVENT)partyboard::ui::handle_event(event->sdl);
                        ++event;
                    }
                    return true;
                });
                const auto report=partyboard::net::nativePreparedRuntimeReport(prepared,configuration,true,installedPipelineSeed);
                printf("Native measured prepared receipt: %s\n",report.dump().c_str());
                printf("Native measured session prepared: build=%s asset=%s; no start/resume authorization\n",prepared.buildId.c_str(),prepared.assetId.c_str());
            }else
#endif
            prepared=partyboard::net::initializeNativePreparedEngine(simulationSeed, "", "");
            printf("Native online engine prepared: seed=%u checkpoint=%u frame=%u phase=%d running=%d\n",
                prepared.seed, prepared.checkpoint, PartyBoard_GameFrameCount(), PartyBoard_GameLifecyclePhase(), int(PartyBoard_IsRunning));
            printf("Native prepared checkpoint components: legacy=%.0f global=%.0f progression=%.0f final=%.0f frand=%.0f rand8=%.0f\n",
                PartyBoard_GameCheckpointComponent(0), PartyBoard_GameCheckpointComponent(1), PartyBoard_GameCheckpointComponent(2),
                PartyBoard_GameCheckpointComponent(3), PartyBoard_GamePausedState(2), PartyBoard_GamePausedState(3));
            if (probeRtcFrames) {
#ifdef PARTYBOARD_NATIVE_RTC
                nativePreparationResult = partyboard::net::runNativeRtcEngineProbe(probeRtcFrames, [] {
                    const auto* event = aurora_update();
                    while (event && event->type != AURORA_NONE) {
                        if (event->type == AURORA_EXIT) return false;
                        if (event->type == AURORA_SDL_EVENT) partyboard::ui::handle_event(event->sdl);
                        ++event;
                    }
                    return true;
                },probeOnlineButtons);
#endif
            } else if (probeOnlineMenu != -1) {
                // Actual engine lifecycle diagnostic, not visual menu/RTC proof.
#ifdef PARTYBOARD_NATIVE_RTC
                partyboard::net::SessionId diagnosticSession{};
                diagnosticSession.fill(0x11);
                auto transportOwner = partyboard::net::createNativeRtcHostInput(diagnosticSession, 0, 15);
#endif
                partyboard::net::NativeMenuHandoff handoff;
                partyboard::net::NativeEngineInputIO engineIO([]{return true;},
                    [](const partyboard::net::Packet&){return false;}, []{});
                struct MenuProbe { int action; partyboard::net::NativeEngineInputIO* io; bool accepted = false; } probe{probeOnlineMenu, &engineIO};
                const auto exchange = +[](void* context, PADStatus[4], const PartyBoardInputState* initial)->int {
                    auto& probe = *static_cast<MenuProbe*>(context);
                    try {
                        const auto actual = probe.io->capture();
                        if (actual.frame != initial->frame || actual.checkpoint != initial->checkpoint ||
                            actual.frand != initial->frand || actual.rand8 != initial->rand8) return 0;
                    } catch (...) { return 0; }
                    probe.accepted = PartyBoard_OnlineMenuRequest(probe.action) == 1;
                    return 0; // No simulation frame authorized by this diagnostic.
                };
                if (!handoff.install()) throw std::runtime_error("Menu handoff installation failed");
                if (!PartyBoardNativeInstallInputExchange(exchange, &probe)) {
                    handoff.close();
                    throw std::runtime_error("Menu probe input installation failed");
                }
                const int execution = PartyBoard_GameResume();
                PartyBoard_GameRequestStop();
                const bool inputDetached = PartyBoardNativeClearOwnedInputExchange(exchange, &probe);
                const bool menuDetached = handoff.close();
#ifdef PARTYBOARD_NATIVE_RTC
                transportOwner->closeStopped();
                printf("Native RTC production factory: constructed and closed; no channels or network start authorized\n");
#endif
                const auto action = handoff.takeStoppedRequest();
                if (execution != 0 || !probe.accepted || !inputDetached || !menuDetached ||
                    !action || *action != probeOnlineMenu || PartyBoard_GameFrameCount() != 0 ||
                    handoff.takeStoppedRequest())
                    throw std::runtime_error("Native menu handoff lifecycle failed");
                printf("Native menu handoff PASS: action=%s frame=0 phase=%d inputDetached=1 menuDetached=1; local diagnostic only\n",
                    probeOnlineMenu == 0 ? "host" : "join", PartyBoard_GameLifecyclePhase());
            } else if (probeOnlineFrames) {
                // Explicit local diagnostic of the actual game-thread hook;
                // neutral logical ports, no remote participant/start agreement.
                struct Probe { u32 limit, accepted; bool buttons; } probe{probeOnlineFrames, 0, probeOnlineButtons};
                const auto exchange = +[](void* context, PADStatus pads[4], const PartyBoardInputState* state)->int {
                    auto& probe = *static_cast<Probe*>(context);
                    printf("Native online frame probe: frame=%u checkpoint=%u frand=%u rand8=%u\n",
                        state->frame, state->checkpoint, state->frand, state->rand8);
                    if (state->frame != probe.accepted || probe.accepted == probe.limit) return 0;
                    for (int slot = 0; slot < 4; ++slot) { pads[slot] = {}; pads[slot].err = PAD_ERR_NONE; }
                    if (probe.buttons && state->frame >= 60 && state->frame % 60 < 2)
                        pads[0].button = PAD_BUTTON_A | PAD_BUTTON_START;
                    ++probe.accepted; return 1;
                };
                if (!PartyBoardNativeInstallInputExchange(exchange, &probe))
                    throw std::runtime_error("Native frame probe installation failed");
                const int execution = PartyBoard_GameResume();
                PartyBoard_GameRequestStop();
                const bool detached = PartyBoardNativeClearOwnedInputExchange(exchange, &probe);
                if (!detached || execution != 0 || probe.accepted != probe.limit || PartyBoard_GameFrameCount() != probe.limit)
                    throw std::runtime_error("Native frame probe execution or cleanup failed");
                printf("Native online frame probe complete: accepted=%u frame=%u detached=1; local diagnostic only\n",
                    probe.accepted, PartyBoard_GameFrameCount());
                nlohmann::ordered_json audioMetrics=nlohmann::ordered_json::array();
                for(int metric=0;metric<14;++metric)audioMetrics.push_back(HuAudPortSynthMetric(metric));
                printf("Native audio probe metrics: %s\n",audioMetrics.dump().c_str());
                printf("Native audio output gain: synth=%d stream=%d\n",HuAudPortSynthMetric(14),HuAudPortSynthMetric(15));
                printf("Native terminal checkpoint components: legacy=%.0f global=%.0f progression=%.0f final=%.0f\n",
                    PartyBoard_GameCheckpointComponent(0), PartyBoard_GameCheckpointComponent(1),
                    PartyBoard_GameCheckpointComponent(2), PartyBoard_GameCheckpointComponent(3));
            }
        } catch (const std::exception& error) {
            fprintf(stderr, "Native online preparation failed: %s\n", error.what()); nativePreparationResult = 2;
        }
    } else if (alignStartClock) {
        if (PartyBoard_GameAlignStartClock() != -1 || PartyBoard_GameInitialize() != 0) {
            fprintf(stderr, "Start initialization failed\n"); return 2;
        }
        if (traceAfterInit) {
            if (!PartyBoardTraceOpen(tracePath.c_str(), 0, simulationSeed) ||
                (!inputScript.empty() && !PartyBoardTraceScriptOpen(inputScript.c_str()))) {
                fprintf(stderr, "Post-init trace failed\n"); return 2;
            }
            PartyBoardTraceSetRecordLimit(recordSamples);
        }
        if (PartyBoard_GameAlignStartClock() != 0 || PartyBoard_GameAlignStartClock() != -1) {
            fprintf(stderr, "Start clock alignment failed\n"); return 2;
        }
        PartyBoard_GameRun();
    } else {
#if defined(PARTYBOARD_NATIVE_RTC) && defined(_WIN32)
        // Production Join enters the independently prepared fresh-process lobby.
        // The existing host-only diagnostic retains its explicit rejection check.
        partyboard::net::NativeMenuHandoff handoff(!probeHostHandoff);
        if(!handoff.install()){fprintf(stderr,"Native online menu installation failed\n");return 2;}
        struct HandoffProbe {bool host=false,joinRejected=false,join=false,joinMode=false;} menuProbe;
        menuProbe.joinMode=probeJoinHandoff;
        const auto exchange=+[](void* context,PADStatus[4],const PartyBoardInputState*)->int{
            auto& probe=*static_cast<HandoffProbe*>(context);
            if(probe.joinMode){probe.join=PartyBoard_OnlineMenuRequest(1)==1;return 0;}
            probe.joinRejected=PartyBoard_OnlineMenuRequest(1)==0;
            probe.host=PartyBoard_OnlineMenuRequest(0)==1;
            return 0;
        };
        if(probeHostHandoff||probeJoinHandoff){
            // Diagnostic hook requires a stopped, seeded, aligned engine.
            if(PartyBoard_GameConfigureSessionSeed(1)!=0){handoff.close();return 2;}
            PartyBoard_GameRequestStop();
            if(PartyBoard_GameInitialize()!=0||PartyBoard_GameAlignStartClock()!=0||
                !PartyBoardNativeInstallInputExchange(exchange,&menuProbe)){handoff.close();return 2;}
        }
        if(probeHostHandoff||probeJoinHandoff)PartyBoard_GameResume();else game_main();
        if(probeHostHandoff||probeJoinHandoff){
            const bool detached=PartyBoardNativeClearOwnedInputExchange(exchange,&menuProbe)==1;
            fprintf(stderr,"Native ordinary host handoff probe: host=%d join-rejected=%d detached=%d phase=%d\n",menuProbe.host,menuProbe.joinRejected,detached,PartyBoard_GameLifecyclePhase());
            std::ofstream report(PartyBoard_ConfigPath/(probeJoinHandoff?"join-handoff-parent.txt":"host-handoff-parent.txt"));
            if(probeJoinHandoff){
                report<<GetCurrentProcessId()<<" "<<menuProbe.join<<" "<<detached<<" "<<PartyBoard_GameLifecyclePhase()<<"\n";
                if(!menuProbe.join||!detached||!report){handoff.close();return 2;}
            }else{
            report<<GetCurrentProcessId()<<" "<<menuProbe.host<<" "<<menuProbe.joinRejected<<" "<<detached<<" "<<PartyBoard_GameLifecyclePhase()<<"\n";
            if(!menuProbe.host||!menuProbe.joinRejected||!detached){handoff.close();return 2;}
            }
        }
        if(!handoff.close()){fprintf(stderr,"Native online menu ownership lost\n");return 2;}
        if(const auto action=handoff.takeStoppedRequest()){
            nativeRestartOnlineAction=*action;
            PartyBoard_RequestRestart();
        }
#else
        game_main();
#endif
    }
    const int traceResult = PartyBoardTraceClose();
    const int netResult = PartyBoardNetDiagnosticClose();

    // partyboard::MoviePlayerShutdown();

    fflush(stdout);
    fflush(stderr);

    // Notifies all CVs and causes threads to exit
    OSResetSystem(OS_RESET_SHUTDOWN, 0, 0);

#ifdef PARTY_BOARD_DISCORD
    partyboard::discord::shutdown();
#endif
    partyboard::ui::shutdown();
    HuAudPortStreamShutdown();
    aurora_shutdown();

    return nativePreparationResult ? nativePreparationResult : (traceResult ? traceResult : netResult);
}










