// Include actual implementation to configure its private roll object.
// Sprite API fixtures isolate display/return semantics from rendering.
#include "../src/game/board/player.c"
#include <assert.h>
#include <stdio.h>

PlayerState GWPlayer[4];
static int calls;
static s16 expectedGroup;
void HuSprAttrSet(s16 group, s16 member, s32 attr) { assert(group == expectedGroup); calls++; }
void HuSprAttrReset(s16 group, s16 member, s32 attr) { assert(group == expectedGroup); calls++; }
void HuSprBankSet(s16 group, s16 member, s16 bank) { assert(group == expectedGroup); calls++; }
void HuSprPosSet(s16 group, s16 member, float x, float y) { assert(group == expectedGroup); calls++; }

int main(void) {
    rollObj = NULL;
    assert(BoardRollDispSetResult(0) == 0);
    assert(BoardRollDispSetResult(1) == 1);
    assert(calls == 0);
    omObjData object = {0};
    rollObj = &object;
    bitcopy3 *work = OM_GET_WORK_PTR(rollObj, bitcopy3);
    work->unk_02 = 0;
    const s16 groups[] = {0, 7, -1};
    const s16 rolls[] = {1, 12};
    for (unsigned group = 0; group < 3; group++) {
        work->unk_04 = expectedGroup = groups[group];
        for (unsigned roll = 0; roll < 2; roll++) {
            GWPlayer[0].roll = rolls[roll];
            for (int visible = 0; visible < 2; visible++) {
                calls = 0;
                assert(BoardRollDispSetResult(visible) == groups[group]);
                assert(work->field00_bit1 == visible);
                assert(calls >= 4);
            }
        }
    }
    puts("PASS actual roll display: absent object, zero/nonzero/signed groups, hidden/visible and one/two digits.");
    return 0;
}
