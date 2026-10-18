"""Read private MusyX project/pool structure; save counts only, never payload."""
import argparse
import json
import struct
import subprocess
from inspect_disc import Disc
from inspect_audio_assets import inspect

def u32(data, offset):
    return struct.unpack_from('>I', data, offset)[0]

def macro_summary(data, synth_control):
    if len(data) < 12:
        raise ValueError('Truncated macro result')
    loaded, started, voices = struct.unpack_from('>3I', data)
    if not 1 <= loaded <= 16 or started > 1 or voices > 64:
        raise ValueError('Invalid macro result counts')
    cursor, seen = 12, set()
    summary = {'envelopes_processed': 0, 'releases_completed': 0,
               'releases_at_observation_cap': 0, 'max_release_blocks': 0}
    for _ in range(voices):
        if len(data) - cursor < 16:
            raise ValueError('Truncated selected sample metadata')
        voice, sample, offset, count = struct.unpack_from('>4I', data, cursor)
        cursor += 16
        if voice >= 64 or voice in seen or sample >= 65535 or not 1 <= count <= 32:
            raise ValueError('Invalid selected sample metadata')
        seen.add(voice)
        if synth_control:
            cursor += 24 * 4  # state/studio/flags/SRC, five change/pitch pairs, nine gains
        cursor += count * 2
        if cursor > len(data):
            raise ValueError('Truncated selected sample/control result')
        if synth_control:
            if len(data) - cursor < 12:
                raise ValueError('Truncated release result')
            processed, blocks, finished = struct.unpack_from('>3I', data, cursor)
            cursor += 12
            if processed > count or blocks > 10000 or finished > 1:
                raise ValueError('Invalid release result')
            if processed:
                summary['envelopes_processed'] += 1
                summary['releases_completed'] += finished
                summary['releases_at_observation_cap'] += not finished and blocks == 10000
                summary['max_release_blocks'] = max(summary['max_release_blocks'], blocks)
            cursor += processed * 2
    if cursor != len(data):
        raise ValueError('Invalid macro result extent')
    return summary

def refs(data, start):
    if start % 2:
        raise ValueError('Unaligned reference list')
    count = 0
    while start + 2 <= len(data):
        value = struct.unpack_from('>H', data, start)[0]
        start += 2
        if value == 65535:
            return count
        if value & 0x8000:
            end = struct.unpack_from('>H', data, start)[0]
            start += 2
            first = value & 0x3fff
            if end < first or end >= 0x8000:
                raise ValueError('Invalid reference range')
            count += end - first + 1
        else:
            count += 1
    raise ValueError('Unterminated reference list')

def examine(group):
    pool_base, project_base = struct.unpack_from('>2I', group)
    project = group[project_base:]
    pool = group[pool_base:]
    cursor = 0
    projects = []
    while cursor + 4 <= len(project):
        following = u32(project, cursor)
        if following == 0xffffffff:
            break
        if cursor + 40 > len(project) or following <= cursor or following > len(project) - 4:
            raise ValueError('Invalid project chain')
        gid, kind = struct.unpack_from('>2H', project, cursor + 4)
        lists = [refs(project, u32(project, cursor + offset)) for offset in (8, 12, 16, 20, 24)]
        projects.append({'id': gid, 'type': kind, 'macro_sample_curve_keymap_layer_counts': lists})
        cursor = following
    else:
        raise ValueError('Missing project terminator')
    records = {}
    for kind, start in zip(('macro', 'curve', 'keymap', 'layer'), struct.unpack_from('>4I', pool)):
        if not start:
            records[kind] = {'count': 0, 'payload_span_sizes': []}
            continue
        cursor = start
        spans = []
        while cursor + 4 <= len(pool):
            following = u32(pool, cursor)
            if following == 0xffffffff:
                break
            if following < 8 or following > len(pool) - cursor - 4:
                raise ValueError('Invalid pool chain')
            spans.append(following - 8)
            cursor += following
        else:
            raise ValueError('Missing pool terminator')
        records[kind] = {'count': len(spans), 'payload_span_sizes': sorted(set(spans))}
    return {'projects': projects, 'pool_records': records}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc')
    parser.add_argument('--output', required=True)
    parser.add_argument('--native')
    parser.add_argument('--wasm')
    parser.add_argument('--group-index', type=int)
    parser.add_argument('--macro-execution', action='store_true')
    parser.add_argument('--synth-control', action='store_true')
    args = parser.parse_args()
    if args.synth_control and not args.macro_execution:
        parser.error('--synth-control requires --macro-execution')
    disc = Disc(args.disc)
    results = []
    try:
        for asset in inspect(disc)['assets']:
            if not asset['path'].endswith('.msm'):
                continue
            info = disc.read(asset['disc_offset'] + asset['sections']['info']['offset'], 64)
            base_groups = list(info[41:41 + info[40]])
            for group in asset['groups']:
                if args.group_index is not None and group['index'] != args.group_index:
                    continue
                entry = disc.read(asset['disc_offset'] + asset['sections']['grpInfo']['offset'] + group['index'] * 32, 32)
                offset, length = struct.unpack_from('>2I', entry, 4)
                if not length:
                    continue
                raw = disc.read(asset['disc_offset'] + asset['sections']['grpData']['offset'] + offset, length)
                result = {'group': group['index'], **examine(raw)}
                if args.native and args.wasm:
                    print(f'Original group registration {group["index"]}', flush=True)
                    load_order = base_groups + ([] if group['index'] in base_groups else [group['index']])
                    payload = struct.pack('>I', len(load_order))
                    if args.macro_execution:
                        payload += struct.pack('>I', load_order.index(group['index']))
                    for index in load_order:
                        loading = disc.read(asset['disc_offset'] + asset['sections']['grpInfo']['offset'] + index * 32, 32)
                        data_offset, data_bytes, sample_offset, sample_bytes = struct.unpack_from('>4I', loading, 4)
                        payload += struct.pack('>2I', data_bytes, sample_bytes)
                        payload += disc.read(asset['disc_offset'] + asset['sections']['grpData']['offset'] + data_offset, data_bytes)
                        payload += disc.read(asset['disc_offset'] + asset['sections']['samp']['offset'] + sample_offset, sample_bytes)
                    result['original_load_order'] = load_order
                    try:
                        extra = ['--synth-control'] if args.synth_control else []
                        native = subprocess.run([args.native, *extra], input=payload, capture_output=True, check=True, timeout=60).stdout
                    except subprocess.CalledProcessError as error:
                        raise RuntimeError(f'Group {group["index"]} native failure {error.returncode}: {error.stderr.decode(errors="replace")}') from error
                    wasm = subprocess.run(['node', args.wasm, *extra], input=payload, capture_output=True, check=True, timeout=60).stdout
                    if (native != wasm) if args.macro_execution else (native.split() != wasm.split()):
                        if args.macro_execution:
                            raise ValueError(f'Original group {group["index"]} selected sample metadata/PCM differs between targets')
                        raise ValueError(f'Original group {group["index"]} registration differs: native={native!r}, wasm={wasm!r}')
                    key = 'loaded_groups_effect_started_sample_voices' if args.macro_execution else 'registered_macro_directory_effect_removal_counts'
                    if args.macro_execution:
                        import hashlib
                        result[key] = list(struct.unpack_from('>3I', native))
                        result['envelope_release_summary'] = macro_summary(native, args.synth_control)
                        result['selected_pcm_protocol_bytes'] = len(native)
                        result['selected_pcm_protocol_sha256'] = hashlib.sha256(native).hexdigest()
                    else:
                        result[key] = [int(value) for value in native.split()]
                results.append(result)
    finally:
        disc.file.close()
    with open(args.output, 'w', encoding='utf-8') as output:
        scope = 'Actual first effect macro per selected group,20 interpreter ticks; selected metadata and up to32 decoded PCM samples byte-equal native/Wasm; no rendered audio' if args.macro_execution else 'Bounded BE project/pool audit and optional original group registration/lifecycle; no macro execution'
        if args.synth_control:
            scope += '; actual synthHandle voice jobs at original five scheduled offsets; selected voice state/studio/flags/SRC/pitch/change flags and nine gain controls byte-equal; original ADSR startup and first decoded envelope block byte-equal (zero count denotes completed startup)'
            scope += '; configured envelope release completion/block counts compared with10000block observation cap'
        json.dump({'scope': scope, 'original_group_registration': bool(args.native and args.wasm), 'groups': results}, output, indent=2)
        output.write('\n')
    print(f'{len(results)} group project/pool structures bounded successfully')

if __name__ == '__main__':
    main()
