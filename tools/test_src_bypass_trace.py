"""Guard selected original mode2 read loop and SRC state writeback words."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',required=True)
p.add_argument('--output',required=True)
a=p.parse_args()
body=re.findall(r'char dspSlave\[\].*?=\s*\{(.*?)\};',Path(a.source).read_text(),re.S)[0]
raw=bytes(int(v,16) for v in re.findall(r'0x([0-9A-Fa-f]{2})\b',body))
digest=hashlib.sha256(raw).hexdigest()
assert digest=='be04d4ee80c858c79a5a140379095f13e86ae3a3a575a0a08683b1dcda4d1c77'
w=struct.unpack('>'+str(len(raw)//2)+'H',raw)
assert w[0xb31:0xb34]==(0x5a8,0x65d,0x707)
assert w[0x73f:0x74b]==(0x00c0,0x0e42,0x0081,0xffdd,0x1120,0x0748,
                         0x1824,0x1b04,0,0,0x00e0,0x0e42)
assert w[0x74b:0x758]==(0x0082,0x0bd9,0x0004,0x189f,0x1adf,
                         0x189f,0x1adf,0x189f,0x1adf,0x189f,0x1adf,0x8900,0x1adc)
assert w[0x6b2:0x6ba]==(0x009a,0x01fc,0x009e,0x0e45,0x0081,0xffdd,0x0083,0x0d80)
assert w[0x6f7:0x707]==(0x1120,0x0703,0x8900,0x1940,0x189e,0x181b,
                         0x199a,0x5400,0x1f5e,0x1959,0xb000,0xfb00,
                         0x8139,0x00e1,0x0e42,0x02df)
Path(a.output).write_text(json.dumps({
    'scope':'Raw selected microcode guard, not full DSP/accelerator execution',
    'microcode_sha256':digest,
    'mode':2,'entrypoint':'0x707',
    'read_loop':'32 iterations: read processed accelerator FFDD, store next output word',
    'state_writeback':'Four trailing output samples copied backward into PB history; fraction cleared',
    'mode1_guard':'At0x6f7 a32-iteration loop reads adjacent source values, subtracts, multiplies and adds product; exact signed/fraction/pipeline semantics not yet executed',
    'remaining':'Accelerator exception/tail and complete DSP state need independent execution validation'
},indent=2)+'\n')
print('Original mode2 read/writeback and mode1 arithmetic raw guards PASS')
