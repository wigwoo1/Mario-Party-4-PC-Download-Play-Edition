"""Compare actual private DSP samples across targets; retain only PCM metrics."""
import argparse
import hashlib
import json
import struct
import subprocess
from inspect_disc import Disc
from inspect_audio_assets import inspect

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('disc')
parser.add_argument('--native', required=True)
parser.add_argument('--wasm', required=True)
parser.add_argument('--output', required=True)
parser.add_argument('--limit', type=int, default=16)
parser.add_argument('--container', action='store_true')
parser.add_argument('--pcm16', action='store_true')
parser.add_argument('--common', action='store_true')
parser.add_argument('--converted', action='store_true')
parser.add_argument('--registered', action='store_true')
parser.add_argument('--baseline-native', help='Independent common-loader executable for registered-library validation')
parser.add_argument('--one-per-group', action='store_true', help='Validate full converted directory and decode one populated sample per group')
args = parser.parse_args()
if args.registered and (not args.converted or not args.baseline_native):
    parser.error('--registered requires --converted and --baseline-native')
if args.one_per_group and not args.converted:
    parser.error('--one-per-group requires --converted')
disc = Disc(args.disc)
results = []
try:
    for asset in inspect(disc)['assets']:
        if not asset['path'].endswith('.msm'):
            continue
        table = asset['sections']['grpInfo']['offset']
        for group in asset['groups']:
            group_entry = disc.read(asset['disc_offset'] + table + group['index'] * 32, 32)
            sample_base = struct.unpack('>I', disc.read(asset['disc_offset'] + table + group['index'] * 32 + 12, 4))[0]
            selected = group['pcm16_samples'] if args.pcm16 else group['dsp_samples']
            if args.one_per_group:
                selected = group['dsp_samples'] + group['pcm16_samples']
            for sample in selected:
                pcm16 = sample in group['pcm16_samples']
                if not sample['decoded_samples']:
                    continue
                count = sample['decoded_samples']
                if sample['loop_length']:
                    count = sample['loop_start'] + sample['loop_length'] * 2 + 1
                raw = disc.read(asset['disc_offset'] + asset['sections']['samp']['offset'] + sample_base + sample['sample_offset'], sample['encoded_bytes'])
                suffix = []
                if pcm16:
                    payload = struct.pack('>5I', sample['decoded_samples'], 0, sample['loop_start'], sample['loop_length'], count) + raw
                    suffix = ['--pcm16']
                else:
                    coeff = [v for pair in sample['coefficient_pairs'] for v in pair]
                    payload = struct.pack('>HBBhh16h4I', sample['num_coefficients'], sample['initial_ps'],
                        sample['loop_ps'], sample['loop_y0'], sample['loop_y1'], *coeff,
                        sample['decoded_samples'], sample['loop_start'], sample['loop_length'], count) + raw
                if args.converted or args.common or (args.container and not args.pcm16):
                    data_offset, data_bytes, sample_offset, sample_bytes = struct.unpack_from('>4I', group_entry, 4)
                    payload = struct.pack('>4I', data_bytes, sample_bytes, sample['id'], count)
                    payload += disc.read(asset['disc_offset'] + asset['sections']['grpData']['offset'] + data_offset, data_bytes)
                    payload += disc.read(asset['disc_offset'] + asset['sections']['samp']['offset'] + sample_offset, sample_bytes)
                    suffix = ['--converted-container' if args.converted else '--common-container' if args.common else '--container']
                native = subprocess.run([args.native] + suffix, input=payload, capture_output=True, check=True).stdout
                wasm = subprocess.run(['node', args.wasm] + suffix, input=payload, capture_output=True, check=True).stdout
                if len(native) != count * 2 or native != wasm:
                    raise ValueError('Native/Wasm PCM mismatch')
                if args.converted:
                    baseline = subprocess.run([args.baseline_native if args.registered else args.native, '--common-container'], input=payload, capture_output=True, check=True).stdout
                    if native != baseline:
                        raise ValueError('Converted directory output differs from original BE container reader')
                if pcm16:
                    expected = [v[0] for v in struct.iter_unpack('>h', raw)]
                    if sample['loop_length']:
                        end = sample['loop_start'] + sample['loop_length']
                        expected = expected[:end] + expected[sample['loop_start']:end] + expected[sample['loop_start']:sample['loop_start'] + 1]
                    if native != struct.pack('<' + 'h' * len(expected), *expected):
                        raise ValueError('PCM16 output differs from independent byte-order/loop reference')
                results.append({'group': group['index'], 'sample': sample['id'], 'samples': count,
                    'loop_start': sample['loop_start'], 'loop_length': sample['loop_length'],
                    'pcm_sha256': hashlib.sha256(native).hexdigest()})
                if args.one_per_group:
                    break
                if len(results) >= args.limit:
                    break
            if len(results) >= args.limit:
                break
finally:
    disc.file.close()
with open(args.output, 'w', encoding='utf-8') as output:
    json.dump({'scope': 'Actual sample decoder cross-target equality including loop repeats; no reference fidelity or effect mixer acceptance', 'container_loader': args.container or args.common or args.converted, 'common_loader': args.common, 'converted_directory': args.converted, 'original_registration_lookup': args.registered, 'one_per_group': args.one_per_group, 'samples': results}, output, indent=2)
    output.write('\n')
print(f'{len(results)} actual sample entries match native/Wasm')
