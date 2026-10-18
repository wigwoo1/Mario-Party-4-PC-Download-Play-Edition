"""Extract unchanged first/later scheduled controls from original host code."""
import argparse
import hashlib
import json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',required=True)
p.add_argument('--header',required=True)
p.add_argument('--evidence')
a=p.parse_args()
s=Path(a.source).read_text()
first=s.index('          if ((dsp_vptr->changed[mix_start] & 0x40) != 0) {')
first_end=s.index('          VoiceDone = adsrHandle(',first)
later=s.index('              if ((dsp_vptr->changed[s] & 0x20) != 0) {',first_end)
later_end=s.index('              current_delta = dsp_vptr->adsr.currentDelta;',later)
Path(a.header).write_text('static void original_first_controls(DSPvoice *dsp_vptr,unsigned mix_start) {\n'
    '  _PB *pb=dsp_vptr->pb;\n'+s[first:first_end]+'}\n'
    'static void original_later_controls(DSPvoice *dsp_vptr,unsigned s) {\n'
    '  _PB *pb=dsp_vptr->pb;\n  u16 patch[64], *pptr=patch, *pend=patch+64;\n'
    +s[later:later_end]+'}\n')
print('Original scheduled control blocks extracted')
if a.evidence:
    evidence={
        'source_sha256':hashlib.sha256(Path(a.source).read_bytes()).hexdigest(),
        'reference_sha256':hashlib.sha256(Path(a.header).read_bytes()).hexdigest(),
        'scope':'Unchanged first/later scheduled control blocks; excludes rendering, host accounting and production audio.'
    }
    Path(a.evidence).write_text(json.dumps(evidence,indent=2)+'\n')
