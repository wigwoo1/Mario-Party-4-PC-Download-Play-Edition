"""Inspect private disc audio metadata without extracting sample payloads."""
import argparse
import json
import struct
from inspect_disc import Disc


def inspect(disc):
    header = disc.read(0, 0x440)
    fst_offset, fst_size = struct.unpack_from('>II', header, 0x424)
    fst = disc.read(fst_offset, fst_size)
    if len(fst) < 12 or fst[0] != 1:
        raise ValueError('Invalid filesystem root')
    count = struct.unpack_from('>I', fst, 8)[0]
    if not 1 <= count <= len(fst) // 12:
        raise ValueError('Invalid filesystem count')
    directories = [('', count)]
    assets = []
    for index in range(1, count):
        while index >= directories[-1][1]:
            directories.pop()
        entry, offset, size = struct.unpack_from('>III', fst, index * 12)
        name_offset = count * 12 + (entry & 0xffffff)
        end = fst.find(b'\0', name_offset)
        if name_offset >= len(fst) or end < 0:
            raise ValueError('Invalid filesystem name')
        name = fst[name_offset:end].decode('ascii')
        path = directories[-1][0] + '/' + name
        if entry >> 24 == 1:
            if not index < size <= directories[-1][1]:
                raise ValueError('Invalid directory range')
            directories.append((path, size))
        elif entry >> 24 != 0:
            raise ValueError('Invalid entry type')
        elif path.lower().endswith(('.msm', '.pdt', '.son', '.adp')):
            asset = {'path': path, 'bytes': size, 'disc_offset': offset}
            if path.lower().endswith('.msm'):
                if size < 0x60:
                    raise ValueError('Truncated MSM header')
                words = struct.unpack('>24I', disc.read(offset, 0x60))
                if words[1] != 2:
                    raise ValueError('Unsupported MSM version')
                names = ('end', 'info', 'auxParam', 'grpInfo', 'mus', 'se',
                         'grpData', 'samp', 'dummyMus', 'grpSet')
                sections = {}
                for position, label in enumerate(names):
                    start, length = words[2 + position * 2:4 + position * 2]
                    if start > size or length > size - start:
                        raise ValueError('MSM section outside file: ' + label)
                    sections[label] = {'offset': start, 'bytes': length}
                asset.update(version=words[1], sections=sections)
                groups = []
                group_table = sections['grpInfo']
                if group_table['bytes'] % 32:
                    raise ValueError('Invalid MSM group table size')
                for group_index in range(group_table['bytes'] // 32):
                    raw = disc.read(offset + group_table['offset'] + group_index * 32, 32)
                    group_id, stack, subgroup, data_offset, data_size, sample_offset, sample_size = struct.unpack_from('>Hbb4I', raw)
                    for relative, length, label in ((data_offset, data_size, 'grpData'), (sample_offset, sample_size, 'samp')):
                        if relative > sections[label]['bytes'] or length > sections[label]['bytes'] - relative:
                            raise ValueError('MSM group outside section')
                    if not data_size:
                        continue
                    if data_size < 16:
                        raise ValueError('Truncated MSM group header')
                    group_base = offset + sections['grpData']['offset'] + data_offset
                    pool, project, directory, songs = struct.unpack('>4I', disc.read(group_base, 16))
                    if any(value > data_size for value in (pool, project, directory, songs)):
                        raise ValueError('MSM group header offset outside data')
                    formats = {}
                    sample_metadata = []
                    pcm_metadata = []
                    sample_count = 0
                    cursor = directory
                    while cursor + 2 <= data_size:
                        if struct.unpack('>H', disc.read(group_base + cursor, 2))[0] == 65535:
                            break
                        if cursor + 32 > data_size:
                            raise ValueError('Truncated MSM sample directory')
                        entry_data = disc.read(group_base + cursor, 32)
                        sample_length = struct.unpack_from('>I', entry_data, 16)[0]
                        format_id = str(sample_length >> 24)
                        sample_info = struct.unpack_from('>I', entry_data, 12)[0]
                        if format_id == '2':
                            relative_sample = struct.unpack_from('>I', entry_data, 4)[0]
                            decoded_samples = sample_length & 0xffffff
                            encoded_bytes = decoded_samples * 2
                            loop_start, loop_length = struct.unpack_from('>II', entry_data, 20)
                            if relative_sample > sample_size or encoded_bytes > sample_size - relative_sample:
                                raise ValueError('MSM PCM sample outside group payload')
                            if loop_start > decoded_samples or loop_length > decoded_samples - loop_start:
                                raise ValueError('MSM PCM loop outside decoded span')
                            pcm_metadata.append({'id': struct.unpack_from('>H', entry_data)[0],
                                'decoded_samples': decoded_samples, 'encoded_bytes': encoded_bytes,
                                'sample_offset': relative_sample, 'loop_start': loop_start,
                                'loop_length': loop_length, 'frequency': sample_info & 0xffffff,
                                'root_key': sample_info >> 24})
                        if format_id == '0':
                            sample_id = struct.unpack_from('>H', entry_data)[0]
                            relative_sample = struct.unpack_from('>I', entry_data, 4)[0]
                            decoded_samples = sample_length & 0xffffff
                            loop_start, loop_length = struct.unpack_from('>II', entry_data, 20)
                            if loop_start > decoded_samples or loop_length > decoded_samples - loop_start:
                                raise ValueError('MSM sample loop outside decoded span')
                            encoded_bytes = ((decoded_samples + 13) // 14) * 8
                            if relative_sample > sample_size or encoded_bytes > sample_size - relative_sample:
                                raise ValueError('MSM sample outside group payload')
                            extra = directory + struct.unpack_from('>I', entry_data, 28)[0]
                            if extra > data_size or 40 > data_size - extra:
                                raise ValueError('MSM ADPCM parameters outside group data')
                            params = disc.read(group_base + extra, 40)
                            num_coef, initial_ps, loop_ps, loop_y0, loop_y1 = struct.unpack_from('>HBBhh', params)
                            sample_metadata.append({'id': sample_id, 'decoded_samples': decoded_samples,
                                'encoded_bytes': encoded_bytes, 'sample_offset': relative_sample,
                                'loop_start': loop_start, 'loop_length': loop_length,
                                'frequency': sample_info & 0xffffff, 'root_key': sample_info >> 24,
                                'num_coefficients': num_coef, 'initial_ps': initial_ps, 'loop_ps': loop_ps,
                                'loop_y0': loop_y0, 'loop_y1': loop_y1,
                                'coefficient_pairs': [list(struct.unpack_from('>hh', params, 8 + i * 4)) for i in range(8)]})
                        formats[format_id] = formats.get(format_id, 0) + 1
                        sample_count += 1
                        cursor += 32
                    else:
                        raise ValueError('Unterminated MSM sample directory')
                    groups.append({'index': group_index, 'id': group_id, 'sample_count': sample_count,
                                   'sample_formats': formats, 'sample_bytes': sample_size,
                                   'dsp_samples': sample_metadata, 'pcm16_samples': pcm_metadata})
                asset['groups'] = groups
            elif path.lower().endswith('.pdt'):
                if size < 32:
                    raise ValueError('Truncated PDT header')
                version, streams, channels, frequency, buffers, listing, adpcm, packs, samples = struct.unpack(
                    '>hh7I', disc.read(offset, 32))
                if version != 1 or streams < 0:
                    raise ValueError('Unsupported PDT header')
                if not 32 <= listing <= adpcm <= packs <= samples <= size:
                    raise ValueError('Invalid PDT section ordering')
                if streams * 4 > adpcm - listing:
                    raise ValueError('Truncated PDT stream list')
                table = disc.read(offset + listing, streams * 4)
                entries = []
                for stream_id in range(streams):
                    pack_offset = struct.unpack_from('>I', table, stream_id * 4)[0]
                    if not pack_offset:
                        continue
                    if not packs <= pack_offset <= samples - 32:
                        raise ValueError('PDT stream pack outside table')
                    data = disc.read(offset + pack_offset, 32)
                    flags, volume, pan, span, aux_a, aux_b, rate, loop_end, loop_start = struct.unpack_from('>6bHII', data)
                    stream_channels = []
                    # Match msmStreamSlotInit's nibble-to-byte conversion and
                    # DMA alignment; these are not raw PCM sample coordinates.
                    playable_bytes = (loop_end >> 1) & ~31
                    loop_byte_start = (loop_start >> 1) & ~7
                    if loop_byte_start > playable_bytes:
                        raise ValueError('Invalid PDT loop range')
                    for channel in range(2 if flags & 1 else 1):
                        sample_offset, param_index, _ = struct.unpack_from('>IhH', data, 16 + channel * 8)
                        if not samples <= sample_offset < size or param_index < 0:
                            raise ValueError('Invalid PDT stream channel')
                        if playable_bytes > size - sample_offset or (param_index + 1) * 32 > packs - adpcm:
                            raise ValueError('PDT sample or coefficient range outside file')
                        coefficients = struct.unpack('>16h', disc.read(offset + adpcm + param_index * 32, 32))
                        stream_channels.append({'sample_offset': sample_offset,
                                                'adpcm_parameter_index': param_index,
                                                'coefficient_pairs': [list(coefficients[i:i+2]) for i in range(0, 16, 2)]})
                    entries.append({'id': stream_id, 'flags': flags, 'frequency': rate,
                                    'loop_end': loop_end, 'loop_start': loop_start,
                                    'playable_adpcm_bytes': playable_bytes,
                                    'aligned_loop_byte_start': loop_byte_start,
                                    'decoded_samples': playable_bytes // 8 * 14,
                                    'channels': stream_channels})
                asset.update(version=version, stream_slots=streams, max_channels=channels,
                             sample_frequency=frequency, max_buffers=buffers,
                             adpcm_parameter_bytes=packs-adpcm, streams=entries)
            assets.append(asset)
    return {'scope': 'Container metadata only; no decoded audio or output verification',
            'assets': assets}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    disc = Disc(args.disc)
    try:
        result = inspect(disc)
    finally:
        disc.file.close()
    with open(args.output, 'w', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
        output.write('\n')
