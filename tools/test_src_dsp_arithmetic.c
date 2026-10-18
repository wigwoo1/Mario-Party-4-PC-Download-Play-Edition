#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/port/src_filter.h"
static uint32_t random_state=0x19790607;
static int16_t random_word(void) {
    uint32_t word;
    random_state=random_state*1664525u+1013904223u;
    word=random_state>>16;
    return (int16_t)(word<32768? (int32_t)word:(int32_t)word-65536);
}
/* Model signed DSP M2 products, unrounded MULXMV/MULXAC/ADDP,
 * then SET40 saturating middle-word read. Coefficients are synthetic. */
static int16_t reference(const int16_t *samples,const int16_t *coefficients) {
    int64_t accumulator=0;
    unsigned i;
    for(i=0;i<4;++i) accumulator+=(int64_t)samples[i]*coefficients[i]*2;
    if(accumulator>INT32_MAX) return 32767;
    if(accumulator<INT32_MIN) return -32768;
    /* Signed arithmetic middle word, expressed without signed right shift. */
    if(accumulator<0) return (int16_t)(-1-((-1-accumulator)/65536));
    return (int16_t)(accumulator/65536);
}
int main(void) {
    int16_t bank[512],samples[4],output;
    uint32_t fraction;
    unsigned i,pass;
    for(i=0;i<512;++i) bank[i]=random_word();
    for(pass=0;pass<16;++pass) for(fraction=0;fraction<65536;++fraction) {
        for(i=0;i<4;++i) samples[i]=random_word();
        assert(PartyboardSrcFilter(samples,fraction,bank,512,&output));
        assert(output==reference(samples,bank+((fraction>>7)&0x1fc)));
    }
    puts("1048576 synthetic DSP filter arithmetic comparisons PASS");return 0;
}
