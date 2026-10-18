"""Compare private pool conversion with independent field reference; save hashes only."""
import argparse
import hashlib
import json
import struct
import subprocess
from inspect_disc import Disc
from inspect_audio_assets import inspect

def reference(pool):
    expected = bytearray(pool)
    counts = []
    def field(position, code):
        value = struct.unpack_from('>' + code, pool, position)[0]
        struct.pack_into('<' + code, expected, position, value)
        return value
    for kind in range(4):
        position = field(kind * 4, 'I')
        count = 0
        if not position:
            counts.append(0)
            continue
        while True:
            following = field(position, 'I')
            if following == 0xffffffff:
                break
            field(position + 4, 'H')
            field(position + 6, 'H')
            if kind == 0:
                for offset in range(8, following, 4):
                    field(position + offset, 'I')
            elif kind == 2:
                for index in range(128):
                    field(position + 8 + index * 8, 'H')
                    field(position + 12 + index * 8, 'h')
            elif kind == 3:
                entries = field(position + 8, 'I')
                for index in range(entries):
                    field(position + 12 + index * 12, 'H')
                    field(position + 18 + index * 12, 'h')
            position += following
            count += 1
        counts.append(count)
    return bytes(expected), counts

def project_reference(project):
    expected = bytearray(project)
    counts = [0, 0] # song/effect groups
    def field(position, code):
        value = struct.unpack_from('>' + code, project, position)[0]
        struct.pack_into('<' + code, expected, position, value)
        return value
    cursor = 0
    while True:
        following = field(cursor, 'I')
        if following == 0xffffffff:
            break
        if following <= cursor:
            raise ValueError('Project chain does not advance')
        field(cursor + 4, 'H')
        kind = field(cursor + 6, 'H')
        counts[kind] += 1
        offsets = [field(cursor + offset, 'I') for offset in range(8, 40, 4)]
        for position in offsets[:5]:
            while field(position, 'H') != 65535:
                position += 2
        if kind == 1:
            position = offsets[5]
            count = field(position, 'H')
            field(position + 2, 'H')
            for index in range(count):
                field(position + 4 + index * 10, 'H')
                field(position + 6 + index * 10, 'H')
        else:
            for position in offsets[5:7]:
                while True:
                    field(position, 'H')
                    if project[position + 4] == 255:
                        break
                    position += 6
            position = offsets[7]
            while field(position, 'H') != 65535:
                field(position + 2, 'H')
                position += 84
        cursor = following
    return bytes(expected), counts

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc')
    parser.add_argument('--native', required=True)
    parser.add_argument('--wasm', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--project', action='store_true')
    args = parser.parse_args()
    disc = Disc(args.disc)
    results = []
    try:
        for asset in inspect(disc)['assets']:
            if not asset['path'].endswith('.msm'):
                continue
            for group in asset['groups']:
                entry = disc.read(asset['disc_offset'] + asset['sections']['grpInfo']['offset'] + group['index'] * 32, 32)
                offset, length = struct.unpack_from('>2I', entry, 4)
                if not length:
                    continue
                raw = disc.read(asset['disc_offset'] + asset['sections']['grpData']['offset'] + offset, length)
                pool = raw[struct.unpack_from('>I', raw, 4 if args.project else 0)[0]:]
                expected, counts = project_reference(pool) if args.project else reference(pool)
                payload = struct.pack('>I', len(pool)) + pool
                suffix = ['--project'] if args.project else []
                native = subprocess.run([args.native] + suffix, input=payload, capture_output=True, check=True).stdout
                wasm = subprocess.run(['node', args.wasm] + suffix, input=payload, capture_output=True, check=True).stdout
                if native != expected or wasm != expected:
                    raise ValueError(f'Pool field conversion mismatch in group {group["index"]}')
                results.append({'group': group['index'], 'song_effect_counts' if args.project else 'macro_curve_keymap_layer_counts': counts,
                    'bytes': len(expected), 'converted_sha256': hashlib.sha256(expected).hexdigest()})
    finally:
        disc.file.close()
    with open(args.output, 'w', encoding='utf-8') as output:
        json.dump({'scope': 'Actual project/pool endian/layout conversion equality only; no macro execution', 'project': args.project, 'groups': results}, output, indent=2)
        output.write('\n')
    print(f'{len(results)} actual {"projects" if args.project else "pools"} match native/Wasm/independent reference')

if __name__ == '__main__':
    main()
