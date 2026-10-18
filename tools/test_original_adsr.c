/* Exercise actual MusyX envelope implementation used by hardware scheduling. */
#include <assert.h>
#include <string.h>
#include "musyx/adsr.h"
#include "../src/port/musyx_pcm_envelope.h"
extern u32 salChangeADSRState(ADSR_VARS *adsr);
int main(void) {
    {
        unsigned attack_mode;
        for (attack_mode = 0; attack_mode < 2; ++attack_mode) {
            PartyboardMusyxPcmEnvelope whole = {0}, split;
            int16_t a[256], b[256];
            unsigned i, calls;
            whole.adsr.mode = 1;
            whole.adsr.data.dls.aMode = (u8)attack_mode;
            whole.adsr.data.dls.aTime = 2;
            whole.adsr.data.dls.dTime = 4;
            whole.adsr.data.dls.sLevel = 96;
            whole.adsr.data.dls.rTime = 4;
            assert(!salChangeADSRState(&whole.adsr));
            split = whole;
            for (i = 0; i < 256; ++i) a[i] = b[i] = 30000;
            assert(PartyboardMusyxPcmEnvelopeApply(&whole, a, 256) == 256);
            for (i = 0; i < 256; i += 16)
                assert(PartyboardMusyxPcmEnvelopeApply(&split, b + i, 16) == 16);
            assert(memcmp(a, b, sizeof(a)) == 0);
            assert(whole.adsr.state == 3 && whole.adsr.currentVolume > 0);
            assert(a[0] == 0 && a[64] > a[128] && a[160] == a[255]);
            adsrRelease(&whole.adsr);
            calls = 0;
            while (!whole.finished && calls < 100) {
                for (i = 0; i < 32; ++i) a[i] = 30000;
                PartyboardMusyxPcmEnvelopeApply(&whole, a, 32);
                ++calls;
            }
            assert(whole.finished && calls < 100 && whole.adsr.currentVolume == 0);
        }
    }
    {
        PartyboardMusyxPcmEnvelope whole = {0}, split;
        int16_t a[96], b[96];
        unsigned i;
        whole.adsr.mode = 0;
        whole.adsr.state = 4;
        whole.adsr.cnt = 2;
        whole.adsr.currentVolume = 16384 * 65536;
        whole.adsr.currentDelta = -8192 * 65536;
        split = whole;
        for (i = 0; i < 96; ++i) a[i] = b[i] = 16384;
        assert(PartyboardMusyxPcmEnvelopeApply(&whole, a, 96) == 64 && whole.finished);
        assert(PartyboardMusyxPcmEnvelopeApply(&split, b, 7) == 7);
        assert(PartyboardMusyxPcmEnvelopeApply(&split, b + 7, 89) == 57 && split.finished);
        assert(memcmp(a, b, sizeof(a)) == 0);
        assert(a[0] == 8192 && a[31] == 4224 && a[32] == 4096 && a[63] == 128);
    }
    ADSR_VARS adsr = {0};
    u16 start, delta;
    adsr.data.dls.aTime = 2;
    adsr.data.dls.dTime = 2;
    adsr.data.dls.sLevel = 16384;
    adsr.data.dls.rTime = 2;
    assert(!salChangeADSRState(&adsr) && adsr.state == 1);
    assert(!adsrHandle(&adsr, &start, &delta) && start == 0 && delta == 511);
    assert(!adsrHandle(&adsr, &start, &delta) && start == 16383 && adsr.state == 2);
    assert(!adsrHandle(&adsr, &start, &delta) && start == 32767 && (s16)delta == -255);
    assert(!adsrHandle(&adsr, &start, &delta) && adsr.state == 3);
    assert(!adsrHandle(&adsr, &start, &delta) && start == 16384 && delta == 0);
    assert(!adsrRelease(&adsr) && adsr.state == 4);
    assert(!adsrHandle(&adsr, &start, &delta) && start == 16384 && (s16)delta == -256);
    assert(adsrHandle(&adsr, &start, &delta) && start == 8192 && adsr.currentVolume == 0);
    return 0;
}
