/* Actual original volume tables/pan curves, no replacement pan law. */
#include <assert.h>
#include <math.h>
#include "musyx/sal.h"
#include "../src/port/musyx_pcm_gain.h"
int main(void) {
    SAL_VOLINFO left = {0}, right = {0}, center = {0}, rear = {0}, quiet = {0};
    salCalcVolume(0, &left, 1.f, 0, 0, 0.f, 0.f, 0, 0);
    salCalcVolume(0, &right, 1.f, 127u << 16, 0, 0.f, 0.f, 0, 0);
    salCalcVolume(0, &center, 1.f, 64u << 16, 0, 0.f, 0.f, 0, 0);
    assert(left.volL > .99f && left.volR == 0.f && left.volS == 0.f);
    assert(right.volR > .99f && right.volL < .00001f);
    assert(fabsf(center.volL - center.volR) < .00001f);
    assert(center.volL > .70f && center.volL < .72f);
    salCalcVolume(0, &rear, 1.f, 64u << 16, 127u << 16, 0.f, 0.f, 0, 0);
    assert(rear.volS > .70f && rear.volL < .00001f && rear.volR < .00001f);
    salCalcVolume(0, &quiet, 0.f, 64u << 16, 0, 0.f, 0.f, 0, 0);
    assert(quiet.volL == 0.f && quiet.volR == 0.f && quiet.volS == 0.f);
    {
        int16_t gains[9], source[1] = {30000}, output[9];
        int64_t bus[9] = {0};
        unsigned i;
        PartyboardMusyxPcmGain(&left, gains);
        assert(gains[0] == 32767 && gains[1] == 0);
        PartyboardPcmMixAdd(bus, source, 1, gains, 9);
        PartyboardPcmMixFinish(bus, output, 9);
        assert(output[0] == 29999);
        for (i = 1; i < 9; ++i) assert(output[i] == 0);
        PartyboardMusyxPcmGain(&rear, gains);
        assert(gains[2] > 23000 && gains[0] == 0 && gains[1] == 0);
    }
    return 0;
}
