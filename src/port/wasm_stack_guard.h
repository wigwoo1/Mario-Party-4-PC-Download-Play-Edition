#ifndef PARTYBOARD_WASM_STACK_GUARD_H
#define PARTYBOARD_WASM_STACK_GUARD_H
#include <stdint.h>
#include <stddef.h>
enum { PartyBoardStackGuardBytes = 16 };
static void PartyBoardStackGuardInit(void *allocation, size_t usable)
{
    uint32_t *low = allocation;
    uint32_t *high = (uint32_t *)((unsigned char *)allocation + usable + PartyBoardStackGuardBytes);
    for (unsigned i = 0; i < 4; ++i) low[i] = high[i] = 0xBADC0DE0u + i;
}
// Bit 1: lower boundary changed; bit 2: upper boundary changed.
static unsigned PartyBoardStackGuardDamage(const void *allocation, size_t usable)
{
    const uint32_t *low = allocation;
    const uint32_t *high = (const uint32_t *)((const unsigned char *)allocation + usable + PartyBoardStackGuardBytes);
    unsigned damage = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (low[i] != 0xBADC0DE0u + i) damage |= 1;
        if (high[i] != 0xBADC0DE0u + i) damage |= 2;
    }
    return damage;
}
#endif
