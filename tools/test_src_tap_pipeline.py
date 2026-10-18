"""Symbolically trace original four-tap multiply/load pipeline, not full DSP emulation."""
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3]
report=json.loads((root/'outputs/validation/musyx-microcode-linear-boundaries.json').read_text())
instructions={i['offset']:i['words'][0] for i in report['variants'][0]['primary_instructions']}
expected={0x64a:0x80f0,0x64b:0xb8c0,0x64e:0xa6f0,0x64f:0xbcf0,
          0x652:0xbcf0,0x653:0x4ec0,0x654:0xb831,
          0x655:0xa6f0,0x656:0xbcf0,0x657:0xbc00,0x658:0x4e00}
assert all(instructions[address]==word for address,word in expected.items())
# LD parallel reads are committed after the main multiply has read its inputs.
# Distinct labels catch both reversed coefficients and crossed sample pairs.
registers={}
sample_cursor=coefficient_cursor=0
product=None
accumulator=[]
outputs=[]

def step(address,kind):
    global sample_cursor,coefficient_cursor,product,accumulator
    word=instructions[address]
    extension=word&255
    pending={}
    if extension in (0xf0,0xc0):
        side='high' if extension==0xf0 else 'low'
        pending[('sample',side)]=divmod(sample_cursor,4)
        pending[('coefficient',side)]=divmod(coefficient_cursor,4)
        sample_cursor+=1
        coefficient_cursor+=1
    if kind in ('mul','move','add'):
        sample=registers[('sample','high' if word&0x1000 else 'low')]
        coefficient=registers[('coefficient','high' if word&0x800 else 'low')]
        if kind=='move': accumulator=[product]
        if kind=='add': accumulator.append(product)
        product=(sample,coefficient)
    elif kind=='add_product':
        accumulator.append(product)
    if extension==0x31:
        outputs.append(list(accumulator))
    registers.update(pending)

step(0x64a,'load')
step(0x64b,'mul')
for output in range(31):
    step(0x64e,'move')
    step(0x64f,'add')
    # LRRI at650/651 installs the next output's source/coefficient pointers.
    assert sample_cursor==coefficient_cursor==(output+1)*4
    step(0x652,'add')
    step(0x653,'add_product')
    step(0x654,'mul')
step(0x655,'move')
step(0x656,'add')
step(0x657,'add')
step(0x658,'add_product')
outputs.append(list(accumulator))
assert outputs==[[((output,tap),(output,tap)) for tap in range(4)] for output in range(32)]
print('32 original pipeline outputs: four matching sample/coefficient taps in array order PASS')
