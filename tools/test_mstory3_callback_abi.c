#include "REL/mstory3Dll.h"
#include <assert.h>
#include <stdio.h>
static omObjData object;
static StructBss1530 state;
static int calls;
static void one(omObjData* value){assert(value==&object);calls+=1;}
static void two(omObjData* value,StructBss1530* owner){assert(value==&object&&owner==&state);calls+=10;}
int main(void){
    // Use the actual production callback storage and dispatch macros. Volatile
    // forces the indirect calls whose Wasm types differ from variadic lowering.
    volatile StructBss1530* stored=&state;
    stored->unk04=(void*)one;
    MSTORY3_CALL_ONE(stored->unk04,&object);
    stored->unk04=(void*)two;
    MSTORY3_CALL_TWO(stored->unk04,&object,&state);
    stored->unk04=(void*)one;
    MSTORY3_CALL_ONE(stored->unk04,&object);
    assert(calls==12);
    puts("PASS actual mstory3 callback field: fixed one/two-argument Wasm dispatch and retained pointers");
    return 0;
}
