"""Compare cached DSP product rounding semantics to independent nearest-even arithmetic."""
import argparse
import hashlib
import json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--reference',required=True)
p.add_argument('--output',required=True)
a=p.parse_args()
raw=Path(a.reference).read_bytes()
text=raw.decode()
body=text.split('s64 Interpreter::GetLongProductRounded() const',1)[1].split('\n}',1)[0]
assert 'return (prod + 0x8000) & ~0xffff;' in body
assert 'return (prod + 0x7fff) & ~0xffff;' in body
cases=0
for high in range(-32768,32768):
    for low in (0,1,32767,32768,32769,65535):
        product=high*65536+low
        actual=(product+(32768 if product&65536 else 32767))&~65535
        q,r=divmod(product,65536)
        expected=(q+(r>32768 or (r==32768 and q%2!=0)))*65536
        assert actual==expected
        cases+=1
Path(a.output).write_text(json.dumps({
    'scope':'Cached primary DSP product rounding arithmetic only; not mode1 source/operand/pipeline fidelity',
    'reference_sha256':hashlib.sha256(raw).hexdigest(),
    'cases':cases,'result':'PASS','rounding':'nearest integer, ties to even, including negative products'
},indent=2)+'\n')
print(f'{cases} signed DSP product rounding cases PASS')
