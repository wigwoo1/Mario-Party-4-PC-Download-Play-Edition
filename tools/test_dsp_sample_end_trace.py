"""Guard selected MusyX sample-end and source-read instruction evidence."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
source = Path(a.source).read_text()
body = re.findall(r'char dspSlave\[\].*?=\s*\{(.*?)\};', source, re.S)[0]
raw = bytes(int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})\b', body))
assert hashlib.sha256(raw).hexdigest() == 'be04d4ee80c858c79a5a140379095f13e86ae3a3a575a0a08683b1dcda4d1c77'
words = struct.unpack('>' + str(len(raw)//2) + 'H', raw)
assert words[0xa:0xc] == (0x029f, 0x0c50)
assert words[0xc54:0xc5f] == (0x00de, 0x0bb7, 0x0601, 0x0295, 0x0c5f,
                             0x0e00, 0x00fe, 0x0b87, 0x1fcd, 0x1f8d, 0x02ff)
assert words[0x605:0x607] == (0x0081, 0xffdd)
assert words[0x609:0x60b] == (0x0064, 0x061a)
assert words[0x61b:0x61d] == (0x0065, 0x0620)
assert words[0x623:0x625] == (0x0066, 0x0629)
assert 0x0b87 not in words[0x5a8:0x643]
assert words[0x24f:0x252] == (0x0e05,0x00fe,0x0e04)
assert words[0x260:0x267] == (0x00de,0x0b87,0x0601,0x0295,0x0267,0x029f,0x0332)
assert words[0x34e:0x353] == (0x7800,0x00fe,0x0e04,0x0294,0x0253)
def handler(loop_flag, loop_type):
    # Restricted execution of the selected handler, using its actual words.
    # MMIO is captured, not treated as a full accelerator implementation.
    regs=[0]*32
    memory={0xbb7:loop_flag,0xb88:loop_type,0xb87:1,0xbda:7,
            0xbdb:123,0xbdc:456,0xbdd:9,0xffdb:789,0xffdc:987}
    writes=[]
    pc=0xc50
    equal=False
    for _ in range(64):
        op=words[pc];pc+=1
        if op==0x02ff: return memory,writes
        if op==0x8e00: continue  # SET16; all values here fit16bits.
        if op&0xfc00==0x1c00:
            regs[(op>>5)&31]=regs[op&31]
        elif op==0x8100: regs[30]=0
        elif op&0xffe0==0x00c0:
            regs[op&31]=memory.get(words[pc],0);pc+=1
        elif op==0x0601: equal=regs[30]==1
        elif op==0x0295:
            target=words[pc];pc+=1
            if equal: pc=target
        elif op==0x0e00: regs[30]=0
        elif op&0xffe0==0x00e0:
            address=words[pc];pc+=1
            memory[address]=regs[op&31];writes.append(address)
        elif op&0xf800==0x2800:
            address=0xff00|(op&255)
            memory[address]=regs[24+((op>>8)&7)];writes.append(address)
        elif op&0xf800==0x2000:
            regs[24+((op>>8)&7)]=memory.get(0xff00|(op&255),0)
        elif op==0x7600: regs[28]=(regs[28]+1)&65535
        else: raise AssertionError(f'Unsupported handler word {op:04x} at{pc-1:04x}')
    raise AssertionError('Handler did not return')
cases=0
for loop_flag in range(4):
    for loop_type in range(2):
        memory,writes=handler(loop_flag,loop_type)
        if loop_flag!=1:
            assert memory[0xb87]==0 and 0xffdc not in writes
        else:
            assert memory[0xb87]==1 and 0xffdc in writes
            assert memory[0xffdc]==(987 if loop_type else 456)
        cases+=1
evidence = {
    'microcode_sha256': hashlib.sha256(raw).hexdigest(),
    'sample_end_vector': '0x000a -> 0x0c50',
    'restricted_handler_execution_cases': cases,
    'period_gate': 'Five-period counter initialized0x24f; state checked0x260 before source call; counter decrement0x34e returns0x253 for next period.',
    'nonloop_handler': 'Clears PB state and returns without writing accelerator YN2.',
    'source_routine': '0x605 selects processed accelerator read0xffdd; 0x609/0x61b/0x623 loops continue; no PB state address0xb87 in routine0x5a8..0x642.',
    'inference': 'Finite exception does not branch out of the current source routine. Stopped accelerator reads pass through remaining source/SRC work; next period rechecks cleared PB state and skips source call. Model stopped zero input through current period filter, not zero output padding across the whole block; source-loop execution still needs validation.',
    'scope': 'Exact selected raw-word guards; not a complete DSP execution or audio fidelity test.'
}
Path(a.output).write_text(json.dumps(evidence, indent=2)+'\n')
print(f'Selected DSP raw-word guards and {cases} restricted handler executions PASS')
