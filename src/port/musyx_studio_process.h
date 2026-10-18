#ifndef PARTYBOARD_MUSYX_STUDIO_PROCESS_H
#define PARTYBOARD_MUSYX_STUDIO_PROCESS_H
#include "musyx_depop_voice.h"
#include "musyx_studio_mix.h"
#include <string.h>

extern u8 salFrame, salAuxFrame;
extern s32 *dspSurround;

/* Simulation-thread studio stage for the original 2.0 standard command order:
 * SETUP, previous-main inputs, voices, aux exchange, main upload, master sum,
 * previous-surround matrix and saturated stereo PCM. Voice sums commute with
 * SETUP/input addition under defined32-bit writeback. The scheduler has already
 * deposited stopped-voice residuals before this stage runs.
 *
 * SDK owns the SPBs, two planar main buffers, three planar aux buffers and
 * surround history. Every non-null buffer describes480 s32s (surround160).
 * Callbacks run later in salHandleAuxProcessing, not here. This is an original
 * command-boundary implementation, not an original DSP ROM fidelity claim.
 * The standard mono surround bus can also feed both rear speakers directly.
 * It does not implement DPL2/ITD, other aux algorithms or physical speaker QA.
 * Numeric/command semantics cross-check: tools/test_musyx_studio_process.c. */
typedef struct {
    unsigned studios;
    uint64_t blocks;
    int32_t master_lrs[160 * 3]; /* Interleaved, before matrixing/clipping. */
    int16_t stereo[160 * 2];    /* Host-native L,R, ready for a PCM adapter. */
    int16_t speakers[160 * 6];  /* SDL: FL,FR,FC,LFE,BL,BR. FC/LFE un-authored. */
} PartyboardMusyxStudioProcessor;

static inline int32_t PartyboardMusyxStudioAdd32(int32_t a, int32_t b) {
    return PartyboardMusyxMixSigned32((uint32_t)a + (uint32_t)b);
}
static inline int32_t PartyboardMusyxStudioScale32(int32_t sample, uint16_t volume) {
    int64_t product = (int64_t)sample * volume;
    int64_t result = product >= 0 ? product / 32768 : -((-product + 32767) / 32768);
    return PartyboardMusyxMixSigned32((uint32_t)result);
}
static inline int16_t PartyboardMusyxStudioClamp16(int32_t sample) {
    return (int16_t)(sample < -32768 ? -32768 : sample > 32767 ? 32767 : sample);
}
static inline int PartyboardMusyxStudioOverlap(const void *a, size_t a_size,
        const void *b, size_t b_size) {
    uintptr_t first = (uintptr_t)a, second = (uintptr_t)b;
    if(first > UINTPTR_MAX - a_size || second > UINTPTR_MAX - b_size) return 1;
    return first < second + b_size && second < first + a_size;
}
/* No mutation on bad graph/configuration. Distinct SDK-owned buffers keep a
 * write in one studio/ring slot from destroying another's previous-block data.
 * Bounds are an owner contract; arbitrary pointer validity is not inferred. */
static inline int PartyboardMusyxStudioPreflight(unsigned studios, const int32_t *buses,
        const PartyboardMusyxStudioProcessor *processor) {
    const void *regions[8 * 9 + 1];
    size_t sizes[8 * 9 + 1];
    unsigned count = 0, studio, i, j;
    if(!studios || studios > 8 || salMaxStudioNum != studios || salFrame > 1
            || salAuxFrame > 2 || !dspSurround
            || MUSY_VERSION != MUSY_VERSION_CHECK(2, 0, 0)) return 0;
    regions[count] = dspSurround; sizes[count++] = 160 * sizeof(s32);
    for(studio = 0; studio < studios; ++studio) {
        DSPstudioinfo *s = &dspStudio[studio];
        if(s->state > 1) return 0;
        if(s->state) {
            if(s->type != SND_STUDIO_TYPE_STD || s->numInputs > 7 || !s->spb
                    || !s->main[0] || !s->main[1]) return 0;
            for(i = 0; i < s->numInputs; ++i)
                if(s->in[i].studio >= studios || !dspStudio[s->in[i].studio].main[salFrame ^ 1]) return 0;
            for(i = 0; i < 3; ++i)
                if((s->auxAHandler && !s->auxA[i]) || (s->auxBHandler && !s->auxB[i])) return 0;
        }
        if(s->spb) { regions[count] = s->spb; sizes[count++] = sizeof *s->spb; }
        for(i = 0; i < 8; ++i) {
            const s32 *buffer = i < 2 ? s->main[i] : i < 5 ? s->auxA[i - 2] : s->auxB[i - 5];
            if(buffer) { regions[count] = buffer; sizes[count++] = 480 * sizeof(s32); }
        }
    }
    for(i = 0; i < count; ++i) {
        if(processor && PartyboardMusyxStudioOverlap(regions[i], sizes[i], processor, sizeof *processor)) return 0;
        if(buses && PartyboardMusyxStudioOverlap(regions[i], sizes[i], buses, studios * 160 * 9 * sizeof *buses)) return 0;
        for(j = 0; j < i; ++j)
            if(PartyboardMusyxStudioOverlap(regions[i], sizes[i], regions[j], sizes[j])) return 0;
    }
    return !processor || !buses || !PartyboardMusyxStudioOverlap(processor, sizeof *processor,
        buses, studios * 160 * 9 * sizeof *buses);
}
static inline int PartyboardMusyxStudioProcessorInit(PartyboardMusyxStudioProcessor *processor,
        unsigned studios) {
    if(!processor || processor->studios || !PartyboardMusyxStudioPreflight(studios, NULL, processor)) return 0;
    memset(processor, 0, sizeof *processor); processor->studios = studios;
    return 1;
}
/* Signature also fits PartyboardMusyxBlockProcessor. A synthesis failure is
 * distinct from a device dropping PCM; the driver must latch it before sink
 * submission or SDK control advancement. All fallible checks precede writes. */
static inline int PartyboardMusyxStudioProcess(void *context, int32_t *buses, unsigned studios) {
    PartyboardMusyxStudioProcessor *processor = (PartyboardMusyxStudioProcessor*)context;
    DSPhostDPop next[8];
    _SPB prepared[8];
    unsigned studio, frame, channel, input, group;
    if(!processor || processor->studios != studios || processor->blocks == UINT64_MAX || !buses
            || !PartyboardMusyxStudioPreflight(studios, buses, processor)) return 0;
    for(studio = 0; studio < studios; ++studio) if(dspStudio[studio].state) {
        next[studio] = dspStudio[studio].hostDPopSum; prepared[studio] = *dspStudio[studio].spb;
        if(!PartyboardMusyxDepopStudio200(&next[studio], &prepared[studio])) return 0;
    }
    memset(processor->master_lrs, 0, sizeof processor->master_lrs);
    for(studio = 0; studio < studios; ++studio) if(dspStudio[studio].state) {
        DSPstudioinfo *s = &dspStudio[studio];
        int32_t *bus = buses + studio * 160 * 9;
        uint16_t words[27];
        memcpy(words, &prepared[studio], sizeof words);
        for(channel = 0; channel < 9; ++channel) {
            int32_t start = PartyboardMusyxMixSigned32(((uint32_t)words[channel * 3] << 16) | words[channel * 3 + 1]);
            int32_t delta = PartyboardMusyxDepopSigned(words[channel * 3 + 2]);
            for(frame = 0; frame < 160; ++frame)
                bus[frame * 9 + channel] = PartyboardMusyxStudioAdd32(bus[frame * 9 + channel], start + (int32_t)frame * delta);
        }
        for(input = 0; input < s->numInputs; ++input) {
            DSPinput *in = &s->in[input];
            const int32_t *previous = dspStudio[in->studio].main[salFrame ^ 1];
            const uint16_t volumes[3] = {in->vol, in->volA, in->volB};
            for(group = 0; group < 3; ++group) for(channel = 0; channel < 3; ++channel)
                for(frame = 0; frame < 160; ++frame) {
                    unsigned index = frame * 9 + group * 3 + channel;
                    bus[index] = PartyboardMusyxStudioAdd32(bus[index],
                        PartyboardMusyxStudioScale32(previous[channel * 160 + frame], volumes[group]));
                }
        }
        for(group = 0; group < 2; ++group) if(group ? s->auxBHandler != NULL : s->auxAHandler != NULL) {
            s32 **ring = group ? s->auxB : s->auxA;
            for(channel = 0; channel < 3; ++channel) for(frame = 0; frame < 160; ++frame) {
                unsigned planar = channel * 160 + frame, dry = frame * 9 + channel;
                ring[salAuxFrame][planar] = bus[frame * 9 + (group + 1) * 3 + channel];
                bus[dry] = PartyboardMusyxStudioAdd32(bus[dry], ring[(salAuxFrame + 1) % 3][planar]);
            }
        }
        for(channel = 0; channel < 3; ++channel) for(frame = 0; frame < 160; ++frame) {
            int32_t value = bus[frame * 9 + channel];
            s->main[salFrame][channel * 160 + frame] = value;
            if(s->isMaster) processor->master_lrs[frame * 3 + channel] =
                PartyboardMusyxStudioAdd32(processor->master_lrs[frame * 3 + channel], value);
        }
        s->hostDPopSum = next[studio]; *s->spb = prepared[studio];
    }
    for(frame = 0; frame < 160; ++frame) {
        int32_t previous = dspSurround[frame];
        int32_t left = PartyboardMusyxMixSigned32((uint32_t)processor->master_lrs[frame * 3] - (uint32_t)previous);
        int32_t right = PartyboardMusyxStudioAdd32(processor->master_lrs[frame * 3 + 1], previous);
        processor->stereo[frame * 2] = PartyboardMusyxStudioClamp16(left);
        processor->stereo[frame * 2 + 1] = PartyboardMusyxStudioClamp16(right);
        /* Preserve the original surround block delay and its SDK gain. The
         * mono S bus already includes the original0.7079 gain; sending it to
         * both rear speakers preserves its two-speaker matrix energy. Do not
         * matrix that same signal into the front pair in six-channel output.
         * This is the game's3-bus material, not six independently authored buses. */
        processor->speakers[frame * 6] = PartyboardMusyxStudioClamp16(processor->master_lrs[frame * 3]);
        processor->speakers[frame * 6 + 1] = PartyboardMusyxStudioClamp16(processor->master_lrs[frame * 3 + 1]);
        processor->speakers[frame * 6 + 2] = processor->speakers[frame * 6 + 3] = 0;
        processor->speakers[frame * 6 + 4] = processor->speakers[frame * 6 + 5] = PartyboardMusyxStudioClamp16(previous);
        dspSurround[frame] = processor->master_lrs[frame * 3 + 2];
    }
    ++processor->blocks;
    return 1;
}
#endif
