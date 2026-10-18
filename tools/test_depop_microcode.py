"""Guard the original mixer ROM call and residual register writeback."""
import hashlib
import json
from pathlib import Path
import re
import struct

source = Path(__file__).resolve().parents[1] / 'extern/musyx/src/musyx/runtime/dsp_import.c'
body = re.findall(r'char dspSlave\[\].*?=\s*\{(.*?)\};', source.read_text(), re.S)[0]
raw = bytes(int(value,16) for value in re.findall(r'0x([0-9A-Fa-f]{2})\b',body))
digest = hashlib.sha256(raw).hexdigest()
assert digest == 'be04d4ee80c858c79a5a140379095f13e86ae3a3a575a0a08683b1dcda4d1c77'
words = struct.unpack('>'+str(len(raw)//2)+'H',raw)
assert words[0x773:0x77a] == (0x02bf,0x80e7,0x00f8,0x0ba9,0x00fb,0x0bac,0x02df)
assert words[0x785:0x78b] == (0x02bf,0x80e7,0x00f8,0x0ba9,0x00fb,0x0bac)
report = {
    'scope':'Raw selected microcode guard only; no mixer ROM execution or residual numeric proof',
    'microcode_sha256':digest,
    'pb_base':'0xb80','pb_depop_byte_offset':'0x52',
    'dry_left_residual_word':'0xba9','dry_right_residual_word':'0xbac',
    'mixer_call':'0x773 and 0x785 CALL 0x80e7',
    'writeback':'SR AX0L to dry left residual, SR AX1H to dry right residual after ROM call',
    'remaining':'Need verified ROM 0x80e7 arithmetic/register pipeline; do not infer residual from final PCM sample alone'
}
output = source.parents[7] / 'outputs/validation/musyx-depop-rom-writeback.json'
output.write_text(json.dumps(report,indent=2)+'\n')
print('Original mixer ROM call and dry residual register writeback guards PASS')
