"""Trace mode1 pointer/fraction scratch writes; excludes sample/MMIO execution."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
data=json.loads((root/'outputs/validation/musyx-microcode-linear-boundaries.json').read_text())
ops={i['offset']:i['words'][0] for i in data['variants'][0]['primary_instructions']}
expected={0x6b4:0x009e,0x6b6:0x0081,0x6ba:0x0064,
          0x6be:0x4a00,0x6bf:0x1b7e,0x6c2:0x1b7c,
          0x6cc:0x0065,0x6d2:0x0066,0x6d4:0x4a00,
          0x6d5:0x1b7e,0x6d6:0x1b7c}
assert all(ops[address]==word for address,word in expected.items())
instructions={i['offset']:i['words'] for i in data['variants'][0]['primary_instructions']}
assert instructions[0x6b4]==[0x009e,0x0e45]
assert instructions[0x23a]==[0x009f,0x0ce0]
assert instructions[0x23c]==[0x00ff,0x0e42]
assert instructions[0x242]==[0x00ff,0x0e43]
assert instructions[0x403]==[0x009f,0x0ce0]
assert instructions[0x405]==[0x00ff,0x0e42]
assert instructions[0x40b]==[0x00ff,0x0e43]
assert instructions[0x6f2]==[0x00c1,0x0e42]
assert instructions[0x704]==[0x00e1,0x0e42]
assert instructions[0x2f6]==[0x00de,0x0e42]
assert instructions[0x2f8]==[0x00fe,0x0e43]
assert instructions[0x32a]==[0x00de,0x0e42]
assert instructions[0x330]==[0x00fe,0x0e43]
checked=0
low_consumption=0
for pitch in range(0x4000):
    ratio=pitch*16
    for fraction in (0,1,511,512,32767,65534,65535):
        groups=(fraction+32*ratio)>>18
        accumulator=(0xe45<<16)|fraction
        scratch=[]
        for _ in range(groups):
            accumulator+=ratio
            scratch.extend([accumulator>>16,accumulator&65535])
        for _ in range(32-groups):
            accumulator+=ratio
            scratch.extend([accumulator>>16,accumulator&65535])
        position=fraction
        expected_scratch=[]
        for _ in range(32):
            position+=ratio
            expected_scratch.extend([0xe45+(position>>16),position&65535])
        assert scratch==expected_scratch
        consumed=position>>16
        # Mode1 copies only PB history2/3 before decoded samples. Four
        # decrementing writebacks can therefore reach preceding scratch.
        if consumed<2: low_consumption+=1
        checked+=1
(root/'outputs/validation/src-linear-pointer-schedule.json').write_text(json.dumps({
    'scope':'Guarded mode1 phase/pointer scratch arithmetic; excludes decoder, accelerator, history writeback and output pipeline',
    'microcode_sha256':data['variants'][0]['sha256'],
    'cases':checked,'result':'PASS',
    'low_consumption_cases':low_consumption,
    'history_writeback_caveat':'With zero/one consumed sample, four-word writeback reaches0xe42/0xe43 or0xe43, outside the two initialized history words; scratch ownership must be traced',
    'shared_scratch_initialization':'Both guarded main voice setup paths initialize0xe42 and0xe43 to0x0ce0; subsequent period processing can change these output pointers',
    'period_context':'Mode1 writes its advanced output pointer to0xe42; both main voice branches copy0xe42 to0xe43 afterward. Track processed source calls, not blindly absolute block offset, because inactive periods skip the source routine',
    'behavior':'Each output receives source pointer0xe45+wholephase and unquantized16bit fractional phase after ratio advance'
},indent=2)+'\n')
print(f'{checked} mode1 pointer/fraction schedules PASS')
