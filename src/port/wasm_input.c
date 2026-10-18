#include "port/net_diagnostic.h"
#include "port/wasm_input.h"
#include "port/input_trace.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define INPUT_API EMSCRIPTEN_KEEPALIVE
EM_ASYNC_JS(int, exchange_browser_pads, (unsigned char *bytes, unsigned int checkpoint, unsigned int frand, unsigned int rand8), {
    try {
        if(typeof Module.partyBoardInputExchange !== 'function') return 0;
        const input=HEAPU8.slice(bytes,bytes+44);
        const merged=await Module.partyBoardInputExchange(input,{checkpoint:checkpoint>>>0,frand:frand>>>0,rand8:rand8>>>0});
        if(!(merged instanceof Uint8Array)||merged.length!==44) return 0;
        HEAPU8.set(merged,bytes);
        return 1;
    } catch {return 0;}
});
#else
#define INPUT_API
#endif

static int online_required;
INPUT_API void PartyBoardBrowserRequireOnlineInput(int required) { online_required = required != 0; }
INPUT_API int PartyBoardBrowserOnlineInputRequired(void) { return online_required; }
int PartyBoardNetDiagnosticSample(PADStatus pads[4]) {
    if(!pads)return 0;
    if(!online_required)return 1;
#ifdef __EMSCRIPTEN__
    extern s32 rnd_seed;
    unsigned char bytes[44];
    int i;
    for(i=0;i<4;i++) {
        unsigned char *p=bytes+i*11;
        p[0]=pads[i].button;p[1]=pads[i].button>>8;
        p[2]=pads[i].stickX;p[3]=pads[i].stickY;p[4]=pads[i].substickX;p[5]=pads[i].substickY;
        p[6]=pads[i].triggerLeft;p[7]=pads[i].triggerRight;p[8]=pads[i].analogA;p[9]=pads[i].analogB;p[10]=pads[i].err;
    }
    if(!exchange_browser_pads(bytes,PartyBoardSessionCheckpoint(),PartyBoardRandomState(),(u32)rnd_seed))return 0;
    for(i=0;i<4;i++) {
        const unsigned char *p=bytes+i*11;
        pads[i].button=p[0]|(p[1]<<8);pads[i].stickX=(signed char)p[2];pads[i].stickY=(signed char)p[3];
        pads[i].substickX=(signed char)p[4];pads[i].substickY=(signed char)p[5];
        pads[i].triggerLeft=p[6];pads[i].triggerRight=p[7];pads[i].analogA=p[8];pads[i].analogB=p[9];pads[i].err=(signed char)p[10];
    }
    return 1;
#else
    return 0;
#endif
}
u32 PartyBoardNetDiagnosticRumble(u32 localCapability) {
    return online_required ? 0 : localCapability;
}
