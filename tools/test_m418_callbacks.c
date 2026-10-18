#include "REL/m418Dll.h"
#include <assert.h>
#include <stdio.h>

extern M418DllUnkStruct7 lbl_1_bss_50, lbl_1_bss_38, lbl_1_bss_20;
extern s32 fn_1_5D58(void), fn_1_84B0(void), fn_1_9364(void);
static int entries, updates;
static void enter_state(void) { entries++; }
static s32 update_state(void) { updates++; return 7; }

static void check(M418DllUnkStruct7 *state, s32 (*dispatch)(void)) {
    M418DllUnkStruct8 sequence[] = {
        {enter_state, update_state, 90}, {NULL, NULL, -1}
    };
    entries = updates = 0;
    fn_1_AEDC(state, sequence);
    assert(state->unkC == 90);
    assert(dispatch() == 7);
    assert(entries == 1 && updates == 1);
    assert(dispatch() == 7);
    assert(entries == 1 && updates == 2);
    assert(fn_1_AF38(state) == 1);
    assert(dispatch() == 0);
    assert(entries == 1 && updates == 2);
}

int main(void) {
    check(&lbl_1_bss_50, fn_1_5D58);
    check(&lbl_1_bss_38, fn_1_84B0);
    check(&lbl_1_bss_20, fn_1_9364);
    puts("Actual M418 dispatchers: one-shot entry, update return and sequence end PASS");
    return 0;
}
