#ifndef PARTYBOARD_DSP_PRODUCT_ROUNDING_H
#define PARTYBOARD_DSP_PRODUCT_ROUNDING_H
#include <stdint.h>
/* Rounded product middle word for ADDPAXZ: nearest, ties to even.
 * Return the integer word before accumulator addition/saturation. */
static inline int64_t PartyboardDspProductRoundedMiddle(int64_t product) {
    uint64_t magnitude=product<0 ? (uint64_t)(-(product+1))+1 : (uint64_t)product;
    uint64_t quotient=magnitude>>16,remainder=magnitude&65535;
    if(remainder>32768 || (remainder==32768 && (quotient&1))) ++quotient;
    return product<0 ? -(int64_t)quotient : (int64_t)quotient;
}
/* Mode1 arithmetic only: SET40 saturates the difference register before
 * signed-high/unsigned-low MULX, M0, then rounded ADDPAXZ. Source order and
 * fractional operand preparation belong to the separately traced routine. */
static inline int16_t PartyboardDspInterpolateOperands(int16_t base,int16_t next,
        uint16_t fraction) {
    int32_t difference=(int32_t)next-base;
    int64_t value;
    if(difference>32767) difference=32767;
    if(difference<-32768) difference=-32768;
    value=(int64_t)base+PartyboardDspProductRoundedMiddle((int64_t)difference*fraction);
    if(value>32767) value=32767;
    if(value<-32768) value=-32768;
    return (int16_t)value;
}
#endif
