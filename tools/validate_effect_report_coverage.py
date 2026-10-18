"""Compare a completed render diagnostic's coverage against dispatch evidence."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline', required=True)
parser.add_argument('--report', required=True)
parser.add_argument('--output', required=True)
args = parser.parse_args()

def read(path):
    raw = Path(path).read_bytes()
    document = json.loads(raw)
    rows = document['metrics']
    indexed = {row['metrics'][0]: row for row in rows}
    if len(indexed) != len(rows):
        raise ValueError('Duplicate effect indices')
    return raw, indexed

baseline_raw, baseline = read(args.baseline)
report_raw, report = read(args.report)
if set(baseline) != set(report) or set(report) != set(range(2308)):
    raise ValueError('Incomplete full descriptor coverage')
invalid = 0
source_modes = [0, 0, 0]
source_mode_rows = 0
for index, row in report.items():
    expected = baseline[index]
    if row['invalid_fxid_rejected'] != expected['invalid_fxid_rejected']:
        raise ValueError(f'Effect {index} rejection classification differs')
    if row['metrics'][1] != expected['metrics'][1]:
        raise ValueError(f'Effect {index} group loading differs')
    if row['metrics'][2] != expected['metrics'][2]:
        raise ValueError(f'Effect {index} selected voice count differs')
    selected = row['metrics'][2]
    modes = row.get('selected_source_mode_counts')
    if modes is not None:
        if (not isinstance(modes, list) or len(modes) != 3
                or any(type(count) is not int or count < 0 for count in modes)
                or sum(modes) != selected):
            raise ValueError(f'Effect {index} invalid source mode coverage')
        source_mode_rows += 1
        source_modes = [total + count for total, count in zip(source_modes, modes)]
    if row['invalid_fxid_rejected']:
        invalid += 1
        if selected != 0:
            raise ValueError('Rejected FXID rendered a voice')
    elif selected <= 0:
        raise ValueError(f'Valid effect {index} rendered no voice')
if invalid != 236:
    raise ValueError('Unexpected rejected descriptor count')
Path(args.output).write_text(json.dumps({
    'scope': 'Full descriptor coverage and dispatch classifications only; no ROM fidelity or device audio',
    'descriptor_count': len(report), 'valid_rendered': len(report)-invalid,
    'dispatch_selected_voice_counts_match': True,
    'invalid_fxid_rejected': invalid,
    'source_mode_metadata_rows': source_mode_rows,
    'selected_source_mode_counts': source_modes if source_mode_rows else None,
    'source_mode_scope': 'Original first-selected voice modes only; missing rows are not covered; no forced mode or full playback coverage',
    'baseline_sha256': hashlib.sha256(baseline_raw).hexdigest(),
    'report_sha256': hashlib.sha256(report_raw).hexdigest(),
}, indent=2))
print('2308 descriptors: 2072 valid rendered, 236 invalid rejected; coverage PASS')
