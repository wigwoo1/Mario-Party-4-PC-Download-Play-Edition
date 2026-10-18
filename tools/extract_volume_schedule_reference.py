"""Extract unchanged original static volume functions for parity fixtures."""
import argparse
import hashlib
import json
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',required=True)
parser.add_argument('--header',required=True)
parser.add_argument('--evidence',required=True)
args=parser.parse_args()
source=Path(args.source).read_text()
start=source.index('static u32 salCheckVolErrorAndResetDelta(')
end=source.index('static void sal_update_hostplayinfo(',start)
reference=source[start:end]
if reference.count('static ')!=2 or 'static void sal_setup_dspvol(' not in reference:
    raise ValueError('Original function boundaries changed; inspect before extracting')
Path(args.header).write_text(reference)
evidence={'scope':'Unchanged original host volume schedule/residual functions, caller-supplied source',
    'source_sha256':hashlib.sha256(source.encode()).hexdigest(),
    'reference_sha256':hashlib.sha256(reference.encode()).hexdigest(),
    'functions':['salCheckVolErrorAndResetDelta','sal_setup_dspvol']}
Path(args.evidence).write_text(json.dumps(evidence,indent=2))
print('Original two-function volume reference extracted; SHA evidence saved')
