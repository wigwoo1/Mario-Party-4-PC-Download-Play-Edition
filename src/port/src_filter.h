#ifndef PARTYBOARD_SRC_FILTER_H
#define PARTYBOARD_SRC_FILTER_H
#include <stddef.h>
#include <stdint.h>

/* Caller supplies verified signed Q15 filter bank (128 phases,4 taps).
 * No substitute coefficients are generated when the bank is unavailable.
 * This arithmetic kernel alone does not establish MusyX DSP parity. */
static inline int PartyboardSrcFilter(const int16_t history[4], uint32_t fraction,
        const int16_t *coefficients, size_t coefficient_count, int16_t *output) {
    size_t base;
    unsigned i;
    int64_t total = 0, value;
    if (!coefficients || coefficient_count < 512 || fraction > 65535) return 0;
    base = (fraction / 512) * 4;
    for (i = 0; i < 4; ++i) total += (int64_t)history[i] * coefficients[base + i];
    value = total >= 0 ? total / 32768 : -((-total + 32767) / 32768);
    if (value > 32767) value = 32767;
    if (value < -32768) value = -32768;
    *output = (int16_t)value;
    return 1;
}
#endif
