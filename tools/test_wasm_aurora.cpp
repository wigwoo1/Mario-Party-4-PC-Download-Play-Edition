#include <aurora/aurora.h>
#include <aurora/rmlui.hpp>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core.h>
#include "../src/port/ui/button.hpp"
#include "../src/port/ui/document.hpp"
class TestMenuDocument : public partyboard::ui::Document {
public:
    using Document::Document;
    Rml::ElementDocument* root() { return mDocument; }
};
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
extern "C" void byteswap_s32(int32_t*);
extern "C" void byteswap_float(float*);
extern "C" void frame_limiter(void);
#include <emscripten.h>
#include <imgui.h>
#include <dolphin/gx.h>
#include <dolphin/card.h>
#include <dolphin/vi.h>
static unsigned retraceCallbacks;
static void test_retrace_callback(u32 count) {
    assert(count == VIGetRetraceCount());
    ++retraceCallbacks;
}
static void draw_gx_quad(float left=0, float top=0, float width=640, float height=480,
                         GXColor color={0,255,0,255}) {
    float projection[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    float model[3][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0}};
    GXSetProjection(projection,GX_ORTHOGRAPHIC);
    GXLoadPosMtxImm(model,GX_PNMTX0); GXSetCurrentMtx(GX_PNMTX0);
    GXSetViewport(left,top,width,height,0,1); GXSetScissor(0,0,640,480);
    GXSetCullMode(GX_CULL_NONE);
    GXClearVtxDesc(); GXSetVtxDesc(GX_VA_POS,GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GXSetNumChans(1); GXSetChanMatColor(GX_COLOR0A0,color);
    GXSetChanCtrl(GX_COLOR0A0,GX_FALSE,GX_SRC_REG,GX_SRC_REG,0,GX_DF_NONE,GX_AF_NONE);
    GXSetNumTexGens(0); GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD_NULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    GXSetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
    GXSetColorUpdate(GX_TRUE); GXSetAlphaUpdate(GX_TRUE);
    GXSetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
    GXSetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
    GXBegin(GX_QUADS,GX_VTXFMT0,4);
    GXPosition3f32(-1,-1,-0.5f); GXPosition3f32(1,-1,-0.5f);
    GXPosition3f32(1,1,-0.5f); GXPosition3f32(-1,1,-0.5f); GXEnd();
}
static void draw_gx_texture() {
    // One GameCube RGBA8 4x4 tile: AR plane followed by GB plane.
    alignas(32) static unsigned char tile[64];
    for (int i=0;i<16;++i) {
        const bool top=i<8;
        tile[i*2]=255; tile[i*2+1]=top?0:255;
        tile[32+i*2]=top?0:255; tile[32+i*2+1]=top?255:0;
    }
    GXTexObj texture;
    GXInitTexObj(&texture,tile,4,4,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,GX_FALSE);
    GXInitTexObjLOD(&texture,GX_NEAR,GX_NEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    GXLoadTexObj(&texture,GX_TEXMAP0);
    GXSetVtxDesc(GX_VA_TEX0,GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
    GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0,GX_REPLACE);
    GXBegin(GX_QUADS,GX_VTXFMT0,4);
    GXPosition3f32(0.25f,-0.8f,-0.5f); GXTexCoord2f32(0,1);
    GXPosition3f32(0.9f,-0.8f,-0.5f); GXTexCoord2f32(1,1);
    GXPosition3f32(0.9f,0.8f,-0.5f); GXTexCoord2f32(1,0);
    GXPosition3f32(0.25f,0.8f,-0.5f); GXTexCoord2f32(0,0); GXEnd();
}
int main(int argc,char **argv) {
    int32_t integer=0x12345678;
    byteswap_s32(&integer); assert(integer==0x78563412);
    const unsigned char encoded[4]={0x3f,0x80,0,0};
    float scalar; std::memcpy(&scalar,encoded,4);
    byteswap_float(&scalar); assert(scalar==1.0f);
    puts("Aurora assets passed: actual asset conversion matches integer and float golden values.");
    AuroraConfig config{};
    config.appName="PartyBoard browser integration probe";
    config.userPath="/"; config.cachePath="/"; config.resourcesPath="/";
    config.windowWidth=640; config.windowHeight=480;
    config.desiredBackend=BACKEND_WEBGPU;
    config.vsync=true; config.allowCpuAdapter=true;
    config.logLevel=LOG_INFO;
    config.logCallback=[](AuroraLogLevel,const char *module,const char *message,unsigned int len) {
        printf("%s: %.*s\n",module,(int)len,message);
    };
    auto info=aurora_initialize(argc,argv,&config);
    assert(info.window);
    auto* context=aurora::rmlui::get_context();
    assert(context);
    assert(Rml::LoadFontFace("/Inter-Regular.ttf"));
    auto* document=context->LoadDocumentFromMemory(R"RML(<rml><head><style>
      body { margin: 0px; width: 640px; height: 480px; }
      #probe { position: absolute; left: 200px; top: 20px; width: 40px; height: 40px; background-color: #ff00ff; }
      #text { position: absolute; left: 200px; top: 100px; width: 120px; height: 60px; background-color: #000000; color: #ffffff; font-family: Inter; font-size: 28px; }
    </style></head><body><div id="probe"></div><div id="text">TEST</div></body></rml>)RML");
    assert(document); document->Show();
    {
        unsigned presses = 0;
        partyboard::ui::Button button(document, Rml::String("Host <test>"));
        button.on_pressed([&] { ++presses; });
        assert(button.root()->GetInnerRML().find("<test>") == Rml::String::npos);
        button.root()->DispatchEvent(Rml::EventId::Click, {});
        assert(presses == 1);
        button.set_disabled(true);
        button.root()->DispatchEvent(Rml::EventId::Click, {});
        assert(presses == 1);
        button.set_disabled(false);
        Rml::Dictionary parameters;
        parameters["key_identifier"] = int(Rml::Input::KI_RETURN);
        button.root()->DispatchEvent(Rml::EventId::Keydown, parameters);
        assert(presses == 2);
        button.root()->SetProperty("display", "none");
    }
    puts("Aurora menu passed: actual Button handles click/Enter, disabled state and escaped labels.");
    const int documentCount = aurora::rmlui::get_context()->GetNumDocuments();
    for (int cycle = 0; cycle < 3; ++cycle) {
        {
            TestMenuDocument menu("<rml><head></head><body></body></rml>");
            assert(menu.root());
            partyboard::ui::Button action(menu.root(), Rml::String("Join"));
            unsigned presses = 0;
            action.on_pressed([&] { ++presses; });
            menu.show();
            assert(menu.visible());
            action.root()->DispatchEvent(Rml::EventId::Click, {});
            assert(presses == 1);
            menu.hide(false);
            action.root()->DispatchEvent(Rml::EventId::Click, {});
            assert(presses == 1);
            Rml::Dictionary menuKey;
            menuKey["key_identifier"] = int(Rml::Input::KI_F1);
            menu.root()->DispatchEvent(Rml::EventId::Keydown, menuKey);
            assert(menu.visible());
            action.root()->DispatchEvent(Rml::EventId::Click, {});
            assert(presses == 2);
            menu.root()->DispatchEvent(Rml::EventId::Keydown, menuKey);
            assert(!menu.visible());
            menu.hide(true);
            assert(menu.closed());
        }
        aurora::rmlui::get_context()->Update();
        assert(aurora::rmlui::get_context()->GetNumDocuments() == documentCount);
    }
    puts("Aurora document passed: hidden input blocked, F1 toggles and three open/close cycles clean up.");
    CARDSetLoadType(CARD_RAWIMAGE);
    CARDSetBasePath("/contract-card",0);
    CARDInit("TEST","01");
    assert(CARDMount(0,nullptr,nullptr)==CARD_RESULT_READY);
    CARDFileInfo save{};
    assert(CARDCreate(0,"browser-probe",8192,&save)==CARD_RESULT_READY);
    alignas(32) unsigned char written[8192],readback[8192];
    for (int i=0;i<8192;++i) written[i]=(unsigned char)(i*17+31);
    assert(CARDWrite(&save,written,8192,0)==CARD_RESULT_READY);
    assert(CARDRead(&save,readback,8192,0)==CARD_RESULT_READY);
    assert(std::memcmp(written,readback,8192)==0);
    assert(CARDClose(&save)==CARD_RESULT_READY);
    puts("Aurora card passed: temporary browser card writes and reads 8192 exact bytes.");
    GXInit(nullptr,0);
    aurora_set_pause_on_focus_lost(false);
    for (int frame=0; frame<12; ++frame) {
        aurora_update();
        assert(aurora_begin_frame());
        draw_gx_quad();
        // Hidden M411 intro cameras must neither invalidate WebGPU commands
        // nor overwrite the visible green quad with red pixels.
        draw_gx_quad(642,10,-10,469,{255,0,0,255});
        draw_gx_quad(10,482,629,-10,{255,0,0,255});
        draw_gx_quad(642,482,-10,-10,{255,0,0,255});
        draw_gx_quad(0,0,0,480,{255,0,0,255});
        GXSetViewport(0,0,640,480,0,1);
        draw_gx_texture();
        ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(20,20),ImVec2(180,180),IM_COL32(255,0,0,255));
        aurora_end_frame();
        if (frame<11) emscripten_sleep(20); // Yield for GPU callbacks and browser presentation.
    }
    const int redPixel=emscripten_run_script_int(R"JS((()=>{
        const source=document.getElementById('canvas');
        const copy=document.createElement('canvas'); copy.width=source.width; copy.height=source.height;
        const ctx=copy.getContext('2d'); ctx.drawImage(source,0,0);
        const p=ctx.getImageData(80,80,1,1).data;
        const g=ctx.getImageData(320,240,1,1).data;
        const ui=ctx.getImageData(220,40,1,1).data;
        Module.print('RmlUi pixel '+Array.from(ui));
        if(ui.toString()!=='255,0,255,255') return 0;
        const text=ctx.getImageData(200,100,120,60).data;
        let white=0,black=0;
        for(let i=0;i<text.length;i+=4) {
          if(text[i]>240&&text[i+1]>240&&text[i+2]>240)white++;
          if(text[i]<5&&text[i+1]<5&&text[i+2]<5)black++;
        }
        Module.print('RmlUi text pixels '+white+' white / '+black+' black');
        if(white<30||black<1000) return 0;
        const b=ctx.getImageData(500,120,1,1).data, y=ctx.getImageData(500,360,1,1).data;
        Module.print('Texture pixels '+Array.from(b)+' / '+Array.from(y));
        if(b.toString()!=='0,0,255,255' || y.toString()!=='255,255,0,255') return 0;
        Module.print('GX pixel '+Array.from(g)+'; ImGui pixel '+Array.from(p));
        if(g[0]!==0 || g[1]!==255 || g[2]!==0 || g[3]!==255) return 0;
        return p[0]===255 && p[1]===0 && p[2]===0 && p[3]===255 ? 1 : 0;
    })())JS");
    assert(redPixel==1);
    puts("Aurora texture passed: RGBA8 tile samples blue and yellow correctly.");
    puts("Aurora GX passed: center pixel is opaque green from GX quad commands.");
    puts("Aurora pixels passed: canvas contains the rendered red ImGui rectangle.");
    puts("Aurora RmlUi passed: live document rendered an opaque magenta rectangle.");
    puts("Aurora text passed: FreeType font produced white glyph pixels over a black text panel.");
    puts("Aurora frames passed: 12 yielding frames submitted with ImGui geometry.");
    aurora_shutdown();
    const u32 retraceStart = VIGetRetraceCount();
    assert(VISetPostRetraceCallback(test_retrace_callback) == nullptr);
    const double twoRetraceStart = emscripten_get_now();
    VIWaitForRetrace();
    VIWaitForRetrace();
    assert(VIGetRetraceCount() == retraceStart + 2 && retraceCallbacks == 2);
    assert(VISetPostRetraceCallback(nullptr) == test_retrace_callback);
    frame_limiter();
    assert(emscripten_get_now() - twoRetraceStart >= 30.0);
    puts("Aurora retrace passed: logical callbacks advance and two retraces defer pacing until submission boundary.");
    // Start a fresh pacing interval, independent of the preceding retrace test.
    emscripten_sleep(100);
    EM_ASM({ Module.pacingTimerFired = false; setTimeout(() => { Module.pacingTimerFired = true; }, 0); });
    const double pacingStart = emscripten_get_now();
    for (int frame = 0; frame < 6; ++frame) frame_limiter();
    assert(emscripten_get_now() - pacingStart >= 95.0);
    assert(EM_ASM_INT({ return Module.pacingTimerFired ? 1 : 0; }));
    emscripten_sleep(100);
    const double resumeStart = emscripten_get_now();
    frame_limiter();
    assert(emscripten_get_now() - resumeStart >= 15.0);
    // An ordinary expensive frame already consumed the interval; adding
    // another full sleep here would turn a ~45ms frame into a ~62ms frame.
    emscripten_sleep(45);
    const double overrunStart = emscripten_get_now();
    frame_limiter();
    assert(emscripten_get_now() - overrunStart < 15.0);
    const double afterOverrunStart = emscripten_get_now();
    for (int frame = 0; frame < 6; ++frame) frame_limiter();
    assert(emscripten_get_now() - afterOverrunStart >= 95.0);
    puts("Aurora ordinary overrun passed: no extra interval, followed by six paced frames without catch-up.");
    puts("Aurora pacing passed: six paced frames yield to browser timers and resume without catch-up burst.");
    puts("Aurora initialization passed: actual browser runtime initialized and shut down.");
}
