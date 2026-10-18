#ifndef PARTYBOARD_MUSYX_STUDIO_MIX_H
#define PARTYBOARD_MUSYX_STUDIO_MIX_H
#include <stddef.h>
#include <stdint.h>
#include "musyx/voice.h"

/* Software standard-studio bus, in integer sample units. Quantize each voice
 * before adding it, as the DSP mix_add/mix_add_ramp writeback does; retaining
 * fractional products across different voices produces different results.
 * Source/reference and fidelity limits: tools/test_musyx_studio_mix.c.
 * This is not a ROM, source filter, aux processor or device scheduler. */
static inline int32_t PartyboardMusyxMixSigned32(uint32_t bits) {
    return bits <= INT32_MAX ? (int32_t)bits : (int32_t)((int64_t)bits - 4294967296LL);
}
static inline int32_t PartyboardMusyxMixProduct(int64_t product) {
    return (int32_t)(product >= 0 ? product / 32768 : -((-product + 32767) / 32768));
}
static inline int PartyboardMusyxStudioChannel200(unsigned channel, uint16_t control) {
    if(channel >= 9) return 0;
    if(channel >= 3 && !(control & (channel < 6 ? 1U : 2U))) return 0;
    return channel % 3 != 2 || (control & 4U) != 0;
}
/* One voice's post-envelope/post-gain products, not the final mixed PCM.
 * Last-product residual words belong to this voice alone. The caller owns
 * source scheduling and retains these words until its depop/stop boundary.
 * Unsupported DPL2 control and malformed products fail before any mutation. */
static inline int PartyboardMusyxStudioMixVoice200(int32_t *bus, const int64_t *products,
        size_t frames, _PB *pb) {
    size_t frame;
    unsigned channel;
    uint16_t residual[9];
    if(!bus || !products || !pb || !frames || frames > 160 || (pb->mixerCtrl & 0xfff0U)) return 0;
    for(frame = 0; frame < frames; ++frame) for(channel = 0; channel < 9; ++channel)
        if(products[frame * 9 + channel] < -1073741824LL
                || products[frame * 9 + channel] > 1073741824LL) return 0;
    for(channel = 0; channel < 9; ++channel) {
        int enabled = PartyboardMusyxStudioChannel200(channel, pb->mixerCtrl);
        for(frame = 0; frame < frames; ++frame) if(enabled) {
            int32_t contribution = PartyboardMusyxMixProduct(products[frame * 9 + channel]);
            /* Defined 32-bit bus writeback even at the wrap boundary. */
            bus[frame * 9 + channel] = PartyboardMusyxMixSigned32(
                (uint32_t)bus[frame * 9 + channel] + (uint32_t)contribution);
        }
        residual[channel] = enabled ? (uint16_t)PartyboardMusyxMixProduct(
            products[(frames - 1) * 9 + channel]) : 0;
    }
    pb->dpop.aL = residual[0]; pb->dpop.aR = residual[1]; pb->dpop.aS = residual[2];
    pb->dpop.aAuxAL = residual[3]; pb->dpop.aAuxAR = residual[4]; pb->dpop.aAuxAS = residual[5];
    pb->dpop.aAuxBL = residual[6]; pb->dpop.aAuxBR = residual[7]; pb->dpop.aAuxBS = residual[8];
    return 1;
}
#endif
