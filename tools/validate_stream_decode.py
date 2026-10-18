"""Decode private PDT channels through actual C decoder; retain metrics only."""
import argparse
import hashlib
import json
import struct
import subprocess
from inspect_disc import Disc
from inspect_audio_assets import inspect


def validate(disc, decoder, container=False):
    results = []
    for asset in inspect(disc)['assets']:
        if not asset['path'].lower().endswith('.pdt'):
            continue
        container_data = disc.read(asset['disc_offset'], asset['bytes']) if container else None
        for stream in asset['streams']:
            for index, channel in enumerate(stream['channels']):
                coefficients = [value for pair in channel['coefficient_pairs'] for value in pair]
                payload = struct.pack('>16h', *coefficients) + disc.read(
                    asset['disc_offset'] + channel['sample_offset'], stream['playable_adpcm_bytes'])
                command = decoder + [str(stream['id']), str(index)] if container else decoder
                decoded = subprocess.run(command, input=container_data if container else payload,
                                         capture_output=True, check=True).stdout
                if len(decoded) != stream['decoded_samples'] * 2:
                    raise ValueError('Incorrect decoded PCM length')
                samples = [value[0] for value in struct.iter_unpack('<h', decoded)]
                results.append({'stream': stream['id'], 'channel': index,
                                'samples': len(samples), 'minimum': min(samples, default=0),
                                'maximum': max(samples, default=0),
                                'nonzero_samples': sum(value != 0 for value in samples),
                                'pcm_sha256': hashlib.sha256(decoded).hexdigest()})
    return {'scope': 'Actual asset decode with zero initial histories; no reference parity, loop or output acceptance',
            'channels': results}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc')
    parser.add_argument('--decoder', required=True, nargs='+')
    parser.add_argument('--output', required=True)
    parser.add_argument('--chunked', action='store_true')
    parser.add_argument('--container', action='store_true')
    args = parser.parse_args()
    if args.chunked:
        args.decoder.append('--stream')
    disc = Disc(args.disc)
    try:
        result = validate(disc, args.decoder, args.container)
    finally:
        disc.file.close()
    with open(args.output, 'w', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
        output.write('\n')
