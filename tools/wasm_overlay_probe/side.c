/* Test-only entry wrapper around the actual m420 RNG source. */
#include "dolphin/types.h"
extern u32 lbl_1_bss_A48;
void fn_1_8934(u32 seed);
s32 fn_1_8944(void);
int initialized_data = MODULE_SEED;
static int private_data = MODULE_SEED + 100;
static int private_bss;
static int *relocated_pointer = &private_data;
void MutatePrivate(void) { private_data=-1; private_bss=5; relocated_pointer=&private_bss; }
int CheckPrivate(void) {
    return private_data==MODULE_SEED+100 && private_bss==0 && relocated_pointer==&private_data;
}
void ObjectSetup(void) { fn_1_8934(MODULE_SEED); }
u32 ReadState(void) { return lbl_1_bss_A48; }
s32 Draw(void) { return fn_1_8944(); }
