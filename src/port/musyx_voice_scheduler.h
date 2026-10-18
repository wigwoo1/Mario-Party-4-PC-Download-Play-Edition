#ifndef PARTYBOARD_MUSYX_VOICE_SCHEDULER_H
#define PARTYBOARD_MUSYX_VOICE_SCHEDULER_H
#include "musyx_pcm_runtime.h"
#include "musyx_depop_voice.h"
#include "src_coefficients.h"

enum { PARTYBOARD_MUSYX_MAX_VOICES = 64, PARTYBOARD_MUSYX_BLOCK_SAMPLES = 160 * 9 };
typedef struct {
    PartyboardMusyxPcmRuntime runtime;
    _PBDPOP residual;
    uint16_t mixer_control;
    uint8_t studio, retained, owned;
} PartyboardMusyxScheduledVoice;
typedef struct {
    PartyboardMusyxScheduledVoice voice[PARTYBOARD_MUSYX_MAX_VOICES];
    const PartyboardMusyxSampleMemory *memory;
    const DSPvoice *bound_voices;
    const int16_t *banks;
    size_t bank_words;
    unsigned voices, studios;
    unsigned rendered_voices, completed_voices, retired_voices;
    unsigned failed_voice;
} PartyboardMusyxVoiceScheduler;

/* Destination starts zeroed and belongs to the synthesis thread, not an SDL
 * callback. Caller retains original global DSP arrays, samples and coefficient
 * banks until every render has ended. No allocation or device I/O occurs here.
 * Output is one 160-frame, nine-channel integer bus per studio. Studio inputs,
 * aux processors, depop ramps and final device routing are separate stages.
 * After rendering, the control owner clears changed[0..4], then runs the next
 * five seqHandle/synthHandle periods with offsets0..4, as hardware.c does. */
static inline int PartyboardMusyxVoiceSchedulerInit(PartyboardMusyxVoiceScheduler *scheduler,
        unsigned voices, unsigned studios, const PartyboardMusyxSampleMemory *memory,
        const int16_t *banks, size_t bank_words) {
    if(!scheduler || scheduler->voices || !dspVoice || !memory || !voices
            || voices > PARTYBOARD_MUSYX_MAX_VOICES || !studios || studios > 8
            || (banks && bank_words < 1536) || (!banks && bank_words)) return 0;
    memset(scheduler, 0, sizeof(*scheduler));
    scheduler->voices = voices; scheduler->studios = studios; scheduler->memory = memory;
    scheduler->bound_voices = dspVoice;
    scheduler->banks = banks; scheduler->bank_words = bank_words;
    scheduler->failed_voice = UINT32_MAX;
    return 1;
}
/* Explicit approved default; keep Init's strict caller-bank behavior for
 * external providers and diagnostics. The replacement has static lifetime. */
static inline int PartyboardMusyxVoiceSchedulerInitDefault(PartyboardMusyxVoiceScheduler *scheduler,
        unsigned voices, unsigned studios, const PartyboardMusyxSampleMemory *memory) {
    size_t words;
    const int16_t *banks = PartyboardSrcDefaultCoefficients(&words);
    return PartyboardMusyxVoiceSchedulerInit(scheduler, voices, studios, memory, banks, words);
}
/* Preserve the original priority-sort rule, including its tie permutation. */
static inline void PartyboardMusyxScheduledSort(unsigned *order, int left, int right) {
    int last, i;
    unsigned temp;
    if(left >= right) return;
    temp = order[left]; order[left] = order[(left + right) / 2]; order[(left + right) / 2] = temp;
    last = left;
    for(i = left + 1; i <= right; ++i) if(dspVoice[order[i]].prio < dspVoice[order[left]].prio) {
        ++last; temp = order[last]; order[last] = order[i]; order[i] = temp;
    }
    temp = order[left]; order[left] = order[last]; order[last] = temp;
    PartyboardMusyxScheduledSort(order, left, last - 1);
    PartyboardMusyxScheduledSort(order, last + 1, right);
}
static inline int PartyboardMusyxScheduledIndex(const PartyboardMusyxVoiceScheduler *scheduler,
        const DSPvoice *voice, unsigned *index) {
    uintptr_t address = (uintptr_t)voice, base = (uintptr_t)dspVoice;
    if(address < base || (address - base) % sizeof(*voice)
            || (address - base) / sizeof(*voice) >= scheduler->voices) return 0;
    *index = (unsigned)((address - base) / sizeof(*voice));
    return 1;
}
/* Saved residuals survive reuse of the original PB or deletion of its sample
 * payload. Depositing them never dereferences that retired sample. */
static inline void PartyboardMusyxScheduledDepop(PartyboardMusyxVoiceScheduler *scheduler,
        PartyboardMusyxScheduledVoice *slot) {
    if(slot->retained) {
        _PB pb = {0}; DSPvoice old = {0};
        pb.dpop = slot->residual; pb.mixerCtrl = slot->mixer_control; old.pb = &pb;
        PartyboardMusyxDepopVoice200(&dspStudio[slot->studio].hostDPopSum, &old);
        slot->retained = 0;
        ++scheduler->retired_voices;
    }
}
static inline int PartyboardMusyxScheduledIdentity(const PartyboardMusyxScheduledVoice *slot,
        const DSPvoice *voice, const PartyboardMusyxSampleMemory *memory) {
    return slot->runtime.callback_user_value == voice->mesgCallBackUserValue
        && slot->runtime.sample_id == voice->smp_id
        && slot->runtime.sample_address == voice->smp_info.addr
        && slot->runtime.sample_generation != 0
        && slot->runtime.sample_generation == PartyboardMusyxSampleMemoryGeneration(memory, voice->smp_info.addr);
}
/* Validate list bounds/cycles and unsupported paths before consuming any
 * voice's control time. A failed render must not be submitted to a device. */
static inline int PartyboardMusyxVoiceSchedulerRender(PartyboardMusyxVoiceScheduler *scheduler,
        int32_t *buses) {
    unsigned order[PARTYBOARD_MUSYX_MAX_VOICES], offsets[9] = {0};
    uint8_t seen[PARTYBOARD_MUSYX_MAX_VOICES] = {0};
    unsigned count = 0, studio, index, n;
    if(!scheduler || !scheduler->voices || !scheduler->memory || !dspVoice || !buses
            || scheduler->bound_voices != dspVoice) return 0;
    scheduler->failed_voice = UINT32_MAX;
    for(studio = 0; studio < scheduler->studios; ++studio) {
        DSPvoice *voice;
        offsets[studio] = count;
        for(voice = dspStudio[studio].voiceRoot; voice; voice = voice->next) {
            if(!PartyboardMusyxScheduledIndex(scheduler, voice, &index) || seen[index]
                    || !voice->state || voice->state > 2 || voice->studio != studio || !voice->pb) return 0;
            seen[index] = 1; order[count++] = index;
            if(!dspStudio[studio].state) continue;
            scheduler->failed_voice = index;
            /* Original block-entry break and finite completion take precedence
             * over now-unused sample payload or requested source controls. */
            if(voice->state == 2 && (voice->postBreak || (voice->changed[0] & 0x20)
                    || (!voice->smp_info.loopLength && voice->playInfo.posHi >= voice->smp_info.length))) continue;
            if(dspStudio[studio].type != SND_STUDIO_TYPE_STD || (voice->flags & 0x80000000U)
                    || (voice->state == 2 && (voice->pb->itd.flag || (voice->pb->mixerCtrl & 0xfff0U)))) return 0;
            if(voice->state == 1) {
                PartyboardMsmSample sample;
                if(voice->singleOffset >= 5 || !PartyboardMusyxVoiceSourceOpen(&sample, voice, scheduler->memory)) return 0;
            } else if(!scheduler->voice[index].owned
                    || !PartyboardMusyxScheduledIdentity(&scheduler->voice[index], voice, scheduler->memory)) return 0;
            {
                unsigned mode = voice->state == 1 || (voice->changed[0] & 0x100)
                    ? voice->srcTypeSelect : scheduler->voice[index].runtime.source_mode;
                unsigned bank = voice->state == 1 || (voice->changed[0] & 0x80)
                    ? voice->srcCoefSelect : scheduler->voice[index].runtime.coefficient_bank;
                if(mode > 2 || bank >= 3 || (mode == 0 && (!scheduler->banks || scheduler->bank_words < 1536))) return 0;
            }
        }
        PartyboardMusyxScheduledSort(order, (int)offsets[studio], (int)count - 1);
    }
    offsets[scheduler->studios] = count;
    for(index = 0; index < scheduler->voices; ++index) if(dspVoice[index].state && !seen[index]) return 0;
    scheduler->failed_voice = UINT32_MAX;
    scheduler->rendered_voices = scheduler->completed_voices = scheduler->retired_voices = 0;
    memset(buses, 0, scheduler->studios * PARTYBOARD_MUSYX_BLOCK_SAMPLES * sizeof(*buses));
    for(index = 0; index < scheduler->voices; ++index) {
        PartyboardMusyxScheduledVoice *slot = &scheduler->voice[index];
        DSPvoice *voice = &dspVoice[index];
        if(slot->owned && (!voice->state || voice->state == 1)) {
            PartyboardMusyxScheduledDepop(scheduler, slot);
            PartyboardMusyxPcmRuntimeStop(&slot->runtime);
            slot->owned = 0;
        } else if(slot->owned && slot->studio != voice->studio) {
            /* Reconnecting an active voice to a studio preserves its source
             * cursor/envelope; only the old studio's residual is retired. */
            PartyboardMusyxScheduledDepop(scheduler, slot);
            slot->studio = voice->studio;
        }
    }
    for(studio = 0; studio < scheduler->studios; ++studio) {
        if(!dspStudio[studio].state) continue;
        for(n = offsets[studio + 1]; n > offsets[studio]; --n) {
            DSPvoice *voice;
            PartyboardMusyxScheduledVoice *slot;
            int startup;
            unsigned first;
            size_t rendered;
            index = order[n - 1]; voice = &dspVoice[index]; slot = &scheduler->voice[index];
            startup = voice->state == 1; first = startup ? voice->singleOffset : 0;
            if(voice->postBreak || (voice->changed[0] & 0x20)) {
                PartyboardMusyxScheduledDepop(scheduler, slot);
                if(!startup || voice->startupBreak) {
                    if(voice->virtualSampleID != UINT32_MAX) salSynthSendMessage(voice, 3);
                    salDeactivateVoice(voice); voice->startupBreak = 0;
                    PartyboardMusyxPcmRuntimeStop(&slot->runtime);
                    slot->owned = 0;
                    continue;
                }
            }
            if(!startup && !voice->smp_info.loopLength
                    && voice->playInfo.posHi >= voice->smp_info.length) {
                salSynthSendMessage(voice, 0); salDeactivateVoice(voice);
                slot->retained = slot->owned = 0;
                PartyboardMusyxPcmRuntimeStop(&slot->runtime);
                ++scheduler->completed_voices;
                continue;
            }
            if(startup) {
                PartyboardMusyxPcmEnvelope envelope;
                uint32_t ratio = voice->changed[first] & 8 ? voice->pitch[first] : voice->playInfo.pitch;
                voice->virtualSampleID = UINT32_MAX;
                if(!PartyboardMusyxPcmEnvelopeStart(&envelope, &voice->adsr)) {
                    salSynthSendMessage(voice, 0); salDeactivateVoice(voice);
                    ++scheduler->completed_voices;
                    continue;
                }
                voice->pb->mixerCtrl = 0; voice->pb->itd.flag = 0;
                memset(&voice->pb->dpop, 0, sizeof(voice->pb->dpop));
                if(!PartyboardMusyxPcmRuntimeStart(&slot->runtime, voice, scheduler->memory,
                        ratio, scheduler->banks, scheduler->bank_words)) {
                    scheduler->failed_voice = index; return 0;
                }
                slot->owned = 1;
            }
            rendered = PartyboardMusyxPcmRuntimeRenderStudio200(&slot->runtime, voice,
                SND_STUDIO_TYPE_STD, first, startup,
                buses + studio * PARTYBOARD_MUSYX_BLOCK_SAMPLES);
            if(!rendered && !slot->runtime.completion_pending) {
                scheduler->failed_voice = index; return 0;
            }
            if(rendered) {
                slot->residual = voice->pb->dpop; slot->mixer_control = voice->pb->mixerCtrl;
                slot->studio = (uint8_t)studio; slot->retained = 1;
                ++scheduler->rendered_voices;
            }
            if(slot->runtime.completion_pending) {
                /* Original finite/ADSR completion notifies before unlinking;
                 * it is not an explicit break/depop event. */
                if(!PartyboardMusyxPcmRuntimeNotifyCompletion(&slot->runtime, voice)) {
                    scheduler->failed_voice = index; return 0;
                }
                slot->retained = slot->owned = 0;
                PartyboardMusyxPcmRuntimeStop(&slot->runtime);
                ++scheduler->completed_voices;
            } else voice->state = 2;
        }
    }
    for(studio = 0; studio < scheduler->studios; ++studio) dspStudio[studio].alienVoiceRoot = NULL;
    return 1;
}
#endif
