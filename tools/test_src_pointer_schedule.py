"""Trace MusyX source/coefficient pointer writes, excluding MMIO sample decoding."""
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3]
data=json.loads((root/'outputs/validation/musyx-microcode-linear-boundaries.json').read_text())
ops={i['offset']:i['words'][0] for i in data['variants'][0]['primary_instructions']}
expected={0x60d:0x4a00,0x60e:0x1ffc,0x611:0x1579,0x612:0x3500,
          0x615:0x4100,0x616:0x1b7e,0x619:0x1b7f,
          0x621:0x0007,0x622:0x187f,0x625:0x4a3b,
          0x626:0x1ffc,0x627:0x1579,0x628:0x3533,
          0x629:0x4100,0x62a:0x1b7f}
assert all(ops[address]==word for address,word in expected.items())
checked=0
for pitch in range(0x4000):
    ratio=pitch*16
    for fraction in (0,1,511,512,32767,65534,65535):
        groups=(fraction+32*ratio)>>18
        # SET16 retains the middle word rather than saturating pointer values.
        accumulator=(0xe44<<16)|fraction
        coefficient=0x1000
        memory=[]
        for _ in range(groups):
            accumulator+=ratio
            shifted=((accumulator&65535)<<16)>>7
            coefficient=0x1000+((shifted>>16)&0x1fc)
            memory.extend([accumulator>>16,coefficient])
        # DAR/LRR backs onto the prior coefficient. At pitch<1 with no
        # first-loop iterations, this is a scratch predecessor overwritten
        # before any output reads it; keep a sentinel to model that write.
        if not memory: memory=[None]
        cursor=len(memory)-1
        for _ in range(32-groups):
            memory[cursor]=coefficient # ADDAX::S stores old AC1.m first
            cursor+=1
            accumulator+=ratio
            coefficient=0x1000+(((accumulator&65535)>>9)*4)
            memory.append(accumulator>>16) # ANDR::S stores AC0.m
            cursor+=1
            memory.append(None)
        memory[cursor]=coefficient
        if groups==0: memory=memory[1:]
        reference=[]
        position=fraction
        for _ in range(32):
            position+=ratio
            reference.extend([0xe44+(position>>16),0x1000+((position&65535)>>9)*4])
        assert memory==reference,(pitch,fraction)
        checked+=1
print(f'{checked} original two-loop pointer schedules match 32 phase advances PASS')
