"""Compare private container metadata; retain only counts and hashes."""
import argparse
import hashlib
import json
import struct
import subprocess
from inspect_disc import Disc
from inspect_audio_assets import inspect

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc')
    parser.add_argument('--native', required=True)
    parser.add_argument('--wasm', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    disc = Disc(args.disc)
    results = []
    try:
        for asset in inspect(disc)['assets']:
            if not asset['path'].endswith('.msm'):
                continue
            raw = disc.read(asset['disc_offset'], asset['bytes'])
            words = struct.unpack_from('>24I', raw)
            expected = list(words[2:22])
            count = words[9] // 32
            expected.append(count)
            for index in range(count):
                gid, stack, subgroup, offset, length, sample_offset, sample_bytes = struct.unpack_from('>Hbb4I', raw, words[8] + index * 32)
                expected.extend((gid, stack, subgroup, words[14] + offset, length, words[16] + sample_offset, sample_bytes))
            info = raw[words[4]:words[4] + 64]
            expected.extend(info[:4])
            expected.extend(struct.unpack_from('>2H', info, 4))
            expected.extend(info[8:12])
            expected.extend(struct.unpack_from('>5I', info, 16))
            expected.append(info[40])
            expected.extend(info[41:41 + info[40]])
            auxiliary = struct.unpack_from('>2b', info, 14)
            expected.extend(auxiliary)
            if words[7] % 40:
                raise ValueError('Partial auxiliary entry')
            auxiliary_count = words[7] // 40
            expected.append(auxiliary_count)
            auxiliary_types = []
            for index in range(auxiliary_count):
                offset = words[6] + index * 40
                kind = raw[offset]
                if kind > 3:
                    raise ValueError('Unknown auxiliary type')
                active = (7, 7, 3, 9)[kind]
                parameters = list(struct.unpack_from('>' + 'I' * active, raw, offset + 4))
                expected.extend([kind] + parameters + [0] * (9 - active))
                auxiliary_types.append(kind)
            if words[13] % 16:
                raise ValueError('Partial effect entry')
            effect_count = words[13] // 16
            expected.append(effect_count)
            for index in range(effect_count):
                expected.extend(struct.unpack_from('>HHbbhBBBbb', raw, words[12] + index * 16))
            if words[11] % 16:
                raise ValueError('Partial song entry')
            song_count = words[11] // 16
            expected.append(song_count)
            resolved_songs = 0
            for index in range(song_count):
                song = struct.unpack_from('>HHIIbb', raw, words[10] + index * 16)
                expected.extend(song)
                gid, _, offset, length, group_index, _ = song
                base, available = words[18], words[19]
                valid = gid != 65535 and length > 0
                if group_index >= 0:
                    valid = valid and group_index < count
                    if valid:
                        _, _, _, group_offset, group_bytes, _, _ = struct.unpack_from('>Hbb4I', raw, words[8] + group_index * 32)
                        valid = group_bytes >= 16
                        if valid:
                            sequence_offset = struct.unpack_from('>I', raw, words[14] + group_offset + 12)[0]
                            valid = sequence_offset <= group_bytes
                            base = words[14] + group_offset + sequence_offset
                            available = group_bytes - sequence_offset
                valid = valid and offset <= available and length <= available - offset
                expected.extend((int(valid), base + offset if valid else 0, length if valid else 0))
                resolved_songs += int(valid)
                if valid:
                    arrangement = raw[base + offset:base + offset + length]
                    header = struct.unpack_from('>22I', arrangement)
                    converted = bytearray(arrangement)
                    fields = set()
                    def swap_field(position, width):
                        field = (position, width)
                        if field not in fields:
                            converted[position:position + width] = arrangement[position:position + width][::-1]
                            fields.add(field)
                    for field_offset in range(0, 88, 4):
                        swap_field(field_offset, 4)
                    for field_offset in range(header[0], header[0] + 256, 4):
                        swap_field(field_offset, 4)
                    hash_position = len(expected)
                    expected.append(0)
                    expected.extend(header[:5] + (header[21],))
                    expected.extend(header[5:21])
                    track_offsets = struct.unpack_from('>64I', arrangement, header[0])
                    expected.extend(track_offsets)
                    master_entries = []
                    position = header[3]
                    if position:
                        while True:
                            time = struct.unpack_from('>I', arrangement, position)[0]
                            bpm = struct.unpack_from('>I', arrangement, position + 4)[0] if time != 0xffffffff else 0
                            master_entries.append((time, bpm, int(time == 0xffffffff)))
                            swap_field(position, 4)
                            if time != 0xffffffff:
                                swap_field(position + 4, 4)
                            position += 8
                            if time == 0xffffffff:
                                break
                    expected.append(len(master_entries))
                    for master_entry in master_entries:
                        expected.extend(master_entry)
                    for track_offset in track_offsets:
                        entries = []
                        if track_offset:
                            position = track_offset
                            while True:
                                time, program, velocity, pattern, transpose, velocity_add = struct.unpack_from('>IBB2xHbb', arrangement, position)
                                swap_field(position, 4)
                                swap_field(position + 8, 2)
                                if pattern == 65534:
                                    swap_field(position + 10, 2)
                                loop_index = struct.unpack_from('>H', arrangement, position + 10)[0] if pattern == 65534 else 0
                                entries.append((time, program, velocity, pattern, loop_index,
                                                0 if pattern == 65534 else transpose,
                                                0 if pattern == 65534 else velocity_add))
                                position += 12
                                if pattern in (65534, 65535):
                                    break
                        expected.append(len(entries))
                        for entry in entries:
                            if entry[3] == 65534 and entry[4] >= len(entries):
                                raise ValueError('Track loop target outside terminated list')
                            expected.extend(entry)
                            if entry[3] < 65534:
                                pattern_offset = struct.unpack_from('>I', arrangement, header[1] + entry[3] * 4)[0]
                                swap_field(header[1] + entry[3] * 4, 4)
                                for field_offset in range(pattern_offset, pattern_offset + 12, 4):
                                    swap_field(field_offset, 4)
                                pattern_header = struct.unpack_from('>3I', arrangement, pattern_offset)
                                position = pattern_offset + 12
                                notes = []
                                while True:
                                    time, key, velocity = struct.unpack_from('>HBB', arrangement, position)
                                    end = key == 255 and velocity == 255
                                    size = 4 if end or key & 128 or not (key | velocity) else 6
                                    duration = struct.unpack_from('>H', arrangement, position + 4)[0] if size == 6 else 0
                                    swap_field(position, 2)
                                    if size == 6:
                                        swap_field(position + 4, 2)
                                    notes.append((time, key, velocity, duration, size, int(end)))
                                    position += size
                                    if end:
                                        break
                                expected.extend((pattern_offset,) + pattern_header + (len(notes),))
                                for note in notes:
                                    expected.extend(note)
                                for stream_offset in pattern_header[1:3]:
                                    stream_entries = []
                                    position = stream_offset
                                    if stream_offset:
                                        while True:
                                            start = position
                                            first, second = arrangement[position:position + 2]
                                            if first == 128 and second == 0:
                                                stream_entries.append((0, 0, 2, 1))
                                                break
                                            position += 1
                                            delta_time = first & 127
                                            if first & 128:
                                                delta_time = delta_time * 256 + arrangement[position]
                                                position += 1
                                            first = arrangement[position]
                                            position += 1
                                            value = first & 127
                                            if first & 128:
                                                value = value * 256 + arrangement[position]
                                                position += 1
                                                if value & 16384:
                                                    value -= 32768
                                            elif value & 64:
                                                value -= 128
                                            stream_entries.append((delta_time, value, position - start, 0))
                                    expected.append(len(stream_entries))
                                    for stream_entry in stream_entries:
                                        expected.extend(stream_entry)
                    expected[hash_position:hash_position + 1] = converted
            payload = struct.pack('>I', len(raw)) + raw
            for command in ([args.native], ['node', args.wasm]):
                result = subprocess.run(command, input=payload, capture_output=True, timeout=60)
                if result.returncode:
                    raise ValueError(f'Metadata reader exit {result.returncode}: {result.stderr.decode(errors="replace").strip()}')
                actual = [int(value) for value in result.stdout.split()]
                if actual != expected:
                    raise ValueError('Container metadata differs from independent reference')
            results.append({'path': asset['path'], 'bytes': len(raw), 'group_entries': count,
                            'base_groups': list(info[41:41 + info[40]]),
                            'voice_limits': list(info[:3]),
                            'auxiliary_selections': list(auxiliary),
                            'auxiliary_entries': auxiliary_count,
                            'auxiliary_types': auxiliary_types,
                            'effect_entries': effect_count,
                            'song_entries': song_count,
                            'resolved_song_entries': resolved_songs,
                            'metadata_sha256': hashlib.sha256(json.dumps(expected).encode()).hexdigest()})
    finally:
        disc.file.close()
    with open(args.output, 'w', encoding='utf-8') as output:
        json.dump({'scope': 'All container section/group descriptors and decoded INFO configuration native/Wasm/independent reference; no sample payload export', 'containers': results}, output, indent=2)
    print(f'{len(results)} actual MSM containers validated')

if __name__ == '__main__':
    main()
