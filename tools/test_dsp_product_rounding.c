#include <assert.h>
#include <stdio.h>
#include "../src/port/dsp_product_rounding.h"
int main(void) {
    int high;
    unsigned low_index;
    const int low[]={0,1,32767,32768,32769,65535};
    for(high=-32768;high<32768;++high) for(low_index=0;low_index<6;++low_index) {
        int64_t product=(int64_t)high*65536+low[low_index];
        /* Original cached interpreter bias/low-word-clear semantics. */
        int64_t rounded=(product+(((uint64_t)product&65536)?32768:32767));
        int64_t reference=rounded>=0 ? rounded/65536 : -1-((-1-rounded)/65536);
        assert(PartyboardDspProductRoundedMiddle(product)==reference);
    }
    assert(PartyboardDspProductRoundedMiddle(INT64_MAX)==140737488355328LL);
    assert(PartyboardDspProductRoundedMiddle(INT64_MIN)==-140737488355328LL);
    {
      const int16_t samples[]={-32768,-32767,-20000,-1,0,1,20000,32766,32767};
      unsigned base_index,next_index,fraction;
      for(base_index=0;base_index<9;++base_index) for(next_index=0;next_index<9;++next_index)
        for(fraction=0;fraction<65536;fraction+=17) {
          int64_t accumulator=((int64_t)samples[next_index]-samples[base_index])*65536;
          int16_t operand;
          int64_t product,bias;
          volatile int64_t result;
          volatile int16_t actual;
          if(accumulator>INT32_MAX) operand=32767;
          else if(accumulator<INT32_MIN) operand=-32768;
          else operand=(int16_t)(accumulator/65536);
          product=(int64_t)operand*fraction;
          bias=((uint64_t)product&65536)?32768:32767;
          product+=bias;
          result=(product>=0?product/65536:-1-((-1-product)/65536))+samples[base_index];
          if(result>32767) result=32767;
          if(result<-32768) result=-32768;
          actual=PartyboardDspInterpolateOperands(samples[base_index],samples[next_index],
              (uint16_t)fraction);
          if(actual!=result) {
            printf("Mismatch base=%d next=%d fraction=%u operand=%d expected=%lld actual=%d\n",
                samples[base_index],samples[next_index],fraction,operand,(long long)result,
                actual);
            return 1;
          }
        }
      assert(PartyboardDspInterpolateOperands(-32768,32767,32768)==-16384);
      assert(PartyboardDspInterpolateOperands(32767,-32768,32768)==16383);
    }
    puts("393216 signed DSP rounding comparisons and integer bounds PASS");
    return 0;
}
