#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <aurora/dvd.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_gamepad.h>
#include <emscripten.h>
#include <dlfcn.h>
#include <cstdio>
#include <port/settings.h>
#include <port/browser_resolution.hpp>
#include <port/config.hpp>
#include <port/port_version.h>
#include <port/dolassets.h>
#include <port/wasm_language.h>
#include <port/wasm_platform.h>
#include <port/main.h>
extern "C" {
#include <game/audio.h>
#include <game/object.h>
#include <game/pad.h>
#include <game/window.h>
#include <game/sprite.h>
#include <game/memory.h>
int PartyBoardOverlayStateCapture(void*);
int PartyBoardOverlayStateRestore(void*);
extern u32 GlobalCounter;
}
namespace {
// Lifecycle: 0=new, 1=preparing, 2=prepared, 3=released. Reuse a new Wasm
// instance for a new game; resident overlays retain per-instance captured state.
int platformPhase;
ANIMDATA* browserWindowGlyph(s16 entry) {
    if(!EM_ASM_INT({const bytes=Module.partyBoardGlyphTextures?.[$0];return bytes instanceof Uint8Array&&bytes.length===16384?1:0;},entry))return nullptr;
    auto* glyph=HuSprAnimMake(64,64,ANIM_BMP_RGBA8);
    if(!glyph)return nullptr;
    glyph->bmp->data=HuMemDirectMalloc(HEAP_DATA,16384);
    if(!glyph->bmp->data){HuSprAnimKill(glyph);return nullptr;}
    EM_ASM({HEAPU8.set(Module.partyBoardGlyphTextures[$1],$0);},glyph->bmp->data,entry);
    printf("Browser window glyph installed: bank %d\n",entry);
    HuSprAnimLock(glyph);
    return glyph;
}
ANIMDATA* browserPortGlyph(s16 entry, s16 port) {
    if(port<0 || port>=4)return nullptr;
    const bool local=EM_ASM_INT({
        const lanes=Module.partyBoardGlyphLocalPadSlots;
        return Array.isArray(lanes)?lanes[$0]===0:$0===0;
    },port);
    // The active texture set describes local PAD lane zero. Remote devices
    // must keep authored labels until their physical mappings are exchanged.
    return local?browserWindowGlyph(entry):nullptr;
}
u32 browserGlyphRevision() {
    return EM_ASM_INT({
        const lanes=Module.partyBoardGlyphLocalPadSlots;
        let local=1;
        if(Array.isArray(lanes)) {
            local=0;
            for(let port=0;port<4;port++)if(lanes[port]===0)local|=1<<port;
        }
        return (((Module.partyBoardGlyphRevision||0)<<4)|local)>>>0;
    });
}
void* overlayHandles[DLL_MAX]{};
const char* overlayPath(s16 overlay) {
    switch(overlay) {
        case DLL_w04dll: return "/w04Dll.wasm";
        case DLL_w01dll: return "/w01Dll.wasm";
        case DLL_w02dll: return "/w02Dll.wasm";
        case DLL_w03dll: return "/w03Dll.wasm";
        case DLL_w05dll: return "/w05Dll.wasm";
        case DLL_w06dll: return "/w06Dll.wasm";
        case DLL_w10dll: return "/w10Dll.wasm";
        case DLL_m419dll: return "/m419Dll.wasm";
        case DLL_m410dll: return "/m410Dll.wasm";
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
        case DLL_m416dll: return "/m416Dll.wasm";
        case DLL_m417dll: return "/m417Dll.wasm";
        case DLL_m418dll: return "/m418Dll.wasm";
        case DLL_m420dll: return "/m420Dll.wasm";
        case DLL_m421dll: return "/m421Dll.wasm";
        case DLL_m422dll: return "/m422Dll.wasm";
        case DLL_m423dll: return "/m423Dll.wasm";
        case DLL_m424dll: return "/m424Dll.wasm";
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
        case DLL_bootDll: return "/bootDll.wasm";
        case DLL_modeseldll: return "/modeseldll.wasm";
        case DLL_mentdll: return "/mentDll.wasm";
        case DLL_instdll: return "/instDll.wasm";
        case DLL_resultdll: return "/resultDll.wasm";
        default: return nullptr;
    }
}
s32 overlayBackend(s16 overlay,s32 start) {
    if(overlay<0||overlay>=DLL_MAX)return 0;
    const char* path=overlayPath(overlay);if(!path)return 0;
    if(!start)return overlayHandles[overlay]&&PartyBoardOverlayStateRestore(overlayHandles[overlay]);
    if(!overlayHandles[overlay]) {
        void* handle=dlopen(path,RTLD_NOW|RTLD_LOCAL);
        if(!handle||!PartyBoardOverlayStateCapture(handle))return 0;
        overlayHandles[overlay]=handle;
    }
    auto setup=reinterpret_cast<void(*)(void)>(dlsym(overlayHandles[overlay],"ObjectSetup"));
    if(!setup)return 0;
    setup();return 1;
}
void bindKeyboard() {
    PADInit();PADStatus initial[4]{};PADRead(initial);PADSetKeyboardActive(0,TRUE);
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_RETURN,PAD_BUTTON_START});
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_Z,PAD_BUTTON_A});
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_X,PAD_BUTTON_B});
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_C,PAD_BUTTON_X});
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_V,PAD_BUTTON_Y});
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_E,PAD_TRIGGER_R});
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_Q,PAD_TRIGGER_L});
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_R,PAD_TRIGGER_Z});
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_W,PAD_BUTTON_UP});
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_S,PAD_BUTTON_DOWN});
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_A,PAD_BUTTON_LEFT});
    PADSetKeyButtonBinding(0,{SDL_SCANCODE_D,PAD_BUTTON_RIGHT});
    PADSetKeyAxisBinding(0,{SDL_SCANCODE_RIGHT,PAD_AXIS_LEFT_X_POS,0});
    PADSetKeyAxisBinding(0,{SDL_SCANCODE_LEFT,PAD_AXIS_LEFT_X_NEG,0});
    PADSetKeyAxisBinding(0,{SDL_SCANCODE_UP,PAD_AXIS_LEFT_Y_POS,0});
    PADSetKeyAxisBinding(0,{SDL_SCANCODE_DOWN,PAD_AXIS_LEFT_Y_NEG,0});
    PADSetKeyAxisBinding(0,{SDL_SCANCODE_L,PAD_AXIS_RIGHT_X_POS,0});
    PADSetKeyAxisBinding(0,{SDL_SCANCODE_J,PAD_AXIS_RIGHT_X_NEG,0});
    PADSetKeyAxisBinding(0,{SDL_SCANCODE_I,PAD_AXIS_RIGHT_Y_POS,0});
    PADSetKeyAxisBinding(0,{SDL_SCANCODE_K,PAD_AXIS_RIGHT_Y_NEG,0});
    PADSetKeyAxisBinding(0,{SDL_SCANCODE_Q,PAD_AXIS_TRIGGER_L,0});
    PADSetKeyAxisBinding(0,{SDL_SCANCODE_E,PAD_AXIS_TRIGGER_R,0});
}
}
static unsigned int browserWindowWidth=640,browserWindowHeight=480;
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserSetInitialResolution(int width,int height) {
    if(platformPhase!=0||PartyBoard_GameLifecyclePhase()!=0)return -1;
    if(!partyboard::browserResolutionSupported(width,height))return -2;
    browserWindowWidth=(unsigned int)width;browserWindowHeight=(unsigned int)height;return 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserPreparePlatform(void) {
    if(platformPhase!=0||PartyBoard_GameLifecyclePhase()!=0)return -1;
    platformPhase=1;
    partyboard::registerSettings();partyboard::config::FinishRegistration();
    AuroraConfig config{};
    config.appName="Party Board";config.userPath="/";config.cachePath="/";config.resourcesPath="/";
    config.windowWidth=browserWindowWidth;config.windowHeight=browserWindowHeight;
    config.mem1Size=64*1024*1024;config.mem2Size=24*1024*1024;
    config.desiredBackend=BACKEND_WEBGPU;config.vsync=true;config.allowCpuAdapter=true;config.logLevel=LOG_INFO;
    config.logCallback=[](AuroraLogLevel,const char* module,const char* message,unsigned int len){printf("%s: %.*s\n",module,(int)len,message);};
    char program[]="partyboard-browser";char* argv[]={program,nullptr};
    if(!aurora_initialize(1,argv,&config).window){platformPhase=3;return -2;}
    aurora_set_pause_on_focus_lost(false);bindKeyboard();
    if(!aurora_dvd_open("/assets/verified-disc.ciso")){aurora_shutdown();platformPhase=3;return -3;}
    partyboard::version::init();PartyBoardBrowserLanguageInit();InitializeDol();
    omDLLBrowserSetBackend(overlayBackend);HuWinControlGlyphResolverSet(browserWindowGlyph);
    HuWinControlGlyphPortResolverSet(browserPortGlyph);
    HuWinControlGlyphRevisionResolverSet(browserGlyphRevision);platformPhase=2;return 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserSetAudioChannels(int channels) {
    if(platformPhase!=2 || (channels!=2 && channels!=6)) return -1;
    partyboard::getSettings().audio.outputChannels.setValue(channels);
    return 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserSetAudioOutput(int volume,int muted) {
    if(platformPhase!=2 || volume<0 || volume>100 || (muted!=0 && muted!=1))return -1;
    auto& audio=partyboard::getSettings().audio;
    audio.masterVolume.setValue(volume);audio.muted.setValue(muted!=0);
    return HuAudPortApplyOutputSettings()?0:-2;
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserApplyKeyboardBindings(const int* words,int count) {
    if(platformPhase!=2 || PartyBoard_GameLifecyclePhase()!=2 || GlobalCounter!=0)return -1;
    if(!words || count!=PAD_BUTTON_COUNT*2+PAD_AXIS_COUNT*3)return -2;
    u32 buttonCount=0,axisCount=0;
    const auto* currentButtons=PADGetKeyButtonBindings(0,&buttonCount);
    const auto* currentAxes=PADGetKeyAxisBindings(0,&axisCount);
    if(!currentButtons || !currentAxes || buttonCount!=PAD_BUTTON_COUNT || axisCount!=PAD_AXIS_COUNT)return -3;
    PADKeyButtonBinding buttons[PAD_BUTTON_COUNT];
    PADKeyAxisBinding axes[PAD_AXIS_COUNT];
    for(int i=0;i<PAD_BUTTON_COUNT;i++) {
        const int key=words[i*2],action=words[i*2+1];
        if(key<=SDL_SCANCODE_UNKNOWN || key>=SDL_SCANCODE_COUNT)return -4;
        bool found=false;
        for(u32 j=0;j<buttonCount;j++)if(currentButtons[j].padButton==action)found=true;
        if(!found)return -4;
        for(int j=0;j<i;j++)if(buttons[j].padButton==action)return -4;
        buttons[i]={key,static_cast<PADButton>(action)};
    }
    const int offset=PAD_BUTTON_COUNT*2;
    for(int i=0;i<PAD_AXIS_COUNT;i++) {
        const int key=words[offset+i*3],action=words[offset+i*3+1],influence=words[offset+i*3+2];
        if(key<=SDL_SCANCODE_UNKNOWN || key>=SDL_SCANCODE_COUNT || influence!=0)return -4;
        bool found=false;
        for(u32 j=0;j<axisCount;j++)if(currentAxes[j].padAxis==action)found=true;
        if(!found)return -4;
        for(int j=0;j<i;j++)if(axes[j].padAxis==action)return -4;
        axes[i]={key,static_cast<PADAxis>(action),0};
    }
    // Every action is now known and unique. Aurora's individual setters cannot
    // fail after this validation, and no gameplay poll can interleave this call.
    for(const auto& binding:buttons)PADSetKeyButtonBinding(0,binding);
    for(const auto& binding:axes)PADSetKeyAxisBinding(0,binding);
    PADSetKeyboardActive(0,TRUE);
    return 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserApplyControllerBindings(const int* words,int count) {
    if(platformPhase!=2 || PartyBoard_GameLifecyclePhase()!=2 || GlobalCounter!=0)return -1;
    if(!words || count!=PAD_BUTTON_COUNT*2+PAD_AXIS_COUNT*4)return -2;
    auto* zones=PADGetDeadZones(0);
    u32 buttonCount=0,axisCount=0;
    const auto* currentButtons=PADGetButtonMappings(0,&buttonCount);
    const auto* currentAxes=PADGetAxisMappings(0,&axisCount);
    if(!zones || !currentButtons || !currentAxes || buttonCount!=PAD_BUTTON_COUNT || axisCount!=PAD_AXIS_COUNT)return -3;
    PADButtonMapping buttons[PAD_BUTTON_COUNT];
    PADAxisMapping axes[PAD_AXIS_COUNT];
    for(int i=0;i<PAD_BUTTON_COUNT;i++) {
        const int native=words[i*2],action=words[i*2+1];
        if(native!=-1 && (native<0 || native>=SDL_GAMEPAD_BUTTON_COUNT))return -4;
        bool found=false;
        for(u32 j=0;j<buttonCount;j++)if(currentButtons[j].padButton==action)found=true;
        if(!found)return -4;
        for(int j=0;j<i;j++)if(buttons[j].padButton==action)return -4;
        buttons[i]={static_cast<u32>(native),static_cast<PADButton>(action)};
    }
    const int offset=PAD_BUTTON_COUNT*2;
    for(int i=0;i<PAD_AXIS_COUNT;i++) {
        const int axis=words[offset+i*4],sign=words[offset+i*4+1],button=words[offset+i*4+2],action=words[offset+i*4+3];
        if(axis< -1 || axis>=SDL_GAMEPAD_AXIS_COUNT || (sign!=-1 && sign!=1))return -4;
        if(button< -1 || button>=SDL_GAMEPAD_BUTTON_COUNT || (axis==-1)==(button==-1))return -4;
        bool found=false;
        for(u32 j=0;j<axisCount;j++)if(currentAxes[j].padAxis==action)found=true;
        if(!found)return -4;
        for(int j=0;j<i;j++)if(axes[j].padAxis==action)return -4;
        axes[i]={{axis,static_cast<PADAxisSign>(sign)},button,static_cast<PADAxis>(action)};
    }
    for(const auto& mapping:buttons)PADSetButtonMapping(0,mapping);
    for(const auto& mapping:axes)PADSetAxisMapping(0,mapping);
    zones->emulateTriggers=true;
    return 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserGetAudioChannels(void) {
    return platformPhase==2 ? (int)partyboard_settings_audio_channels() : 0;
}
extern "C" int HuAudPortSynthMetric(int metric);
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserAudioDeviceChannels(void) {
    if(platformPhase!=2)return 0;
    const int stream=(int)HuAudPortStreamDeviceChannels();
    return stream?stream:HuAudPortSynthMetric(10);
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserAudioSourceChannels(void) {
    if(platformPhase!=2)return 0;
    const int stream=(int)HuAudPortStreamSourceChannels();
    return stream?stream:HuAudPortSynthMetric(9);
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserAudioSynthMetric(int metric) {
    return HuAudPortSynthMetric(metric);
}
extern "C" EMSCRIPTEN_KEEPALIVE unsigned int PartyBoardBrowserPipelineCount(int created) {
    if(platformPhase!=2)return 0;
    const AuroraStats* stats=aurora_get_stats();return created?stats->createdPipelines:stats->queuedPipelines;
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserAdvancePreparedRenderer(void) {
    if(platformPhase!=2||PartyBoard_GameLifecyclePhase()!=2||GlobalCounter!=0)return -1;
    aurora_update();if(!aurora_begin_frame())return -2;
    aurora_end_frame();emscripten_sleep(20);
    return PartyBoard_GameLifecyclePhase()==2&&GlobalCounter==0?0:-1;
}
extern "C" EMSCRIPTEN_KEEPALIVE int PartyBoardBrowserShutdownPlatform(void) {
    if(platformPhase==3)return 0;
    const int phase=PartyBoard_GameLifecyclePhase();
    if(platformPhase!=2||phase==1||phase==3)return -1;
    if(phase==2&&omwatchproc) {
        const s32 loadedDll=omcurdll;
        const bool loaded=loadedDll>=0&&loadedDll<OM_DLL_MAX&&omDLLinfoTbl[loadedDll];
        omOvlKill(1);if(loaded)omDLLEnd(loadedDll,1);
    }
    HuAudPortStreamShutdown();HuWinControlGlyphResolverSet(nullptr);HuWinControlGlyphPortResolverSet(nullptr);
    HuWinControlGlyphRevisionResolverSet(nullptr);
    omDLLBrowserSetBackend(nullptr);aurora_dvd_close();aurora_shutdown();platformPhase=3;return 0;
}
