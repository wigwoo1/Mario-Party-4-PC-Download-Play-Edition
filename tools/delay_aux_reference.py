"""Synthetic delay vectors from private authored tuning; no sound payload export."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from inspect_disc import Disc


def signed(word):
    word &= 0xffffffff
    return word if word < 0x80000000 else word - 0x100000000


def sample(block, channel, frame):
    index = channel * 160 + frame
    value = (index * 97 + block * 1709) % 65535 - 32767
    if index % 127 == 0:
        value = -2147483648 if (index + block) % 2 else 2147483647
    return value


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('disc', 'vectors', 'presets', 'report'): p.add_argument('--' + name, required=True)
    args = p.parse_args()
    disc = Disc(args.disc)
    try:
        assert disc.read(0, 6) == b'GMPE01' and disc.read(7, 1) == b'\x01'
        raw = disc.read(550298108 + 160, 640)
    finally: disc.file.close()
    assert raw[0] == 0
    presets = [list(struct.unpack_from('>9I', raw, index * 40 + 4)) for index in range(1, 16)]
    assert all(raw[index * 40] == 3 for index in range(1, 16))
    cases = presets + [[6, 10, 11, 0, 100, 33, 0, 100, 77], [7, 8, 9, 100, 100, 100, 100, 100, 100]]
    words = [0x444c5931, len(cases)]
    total = 0
    for parameters in cases:
        sizes = [(delay - 5 + 4) // 5 for delay in parameters[:3]]
        feedback = [value * 128 // 100 for value in parameters[3:6]]
        output = [value * 128 // 100 for value in parameters[6:9]]
        buffers = [[0] * (size * 160) for size in sizes]
        positions = [0, 0, 0]
        words += parameters + [80]
        for block in range(80):
            for channel in range(3):
                for frame in range(160):
                    index = positions[channel] * 160 + frame
                    previous = buffers[channel][index]
                    # PPC word multiplication truncates before signed shift;
                    # Python // is arithmetic-floor division, including negatives.
                    returned = signed(previous * output[channel]) // 128
                    buffers[channel][index] = signed(sample(block, channel, frame) + signed(previous * feedback[channel]) // 128)
                    words.append(returned & 0xffffffff); total += 1
                positions[channel] = (positions[channel] + 1) % sizes[channel]
        words += positions
        for buffer in buffers: words += [value & 0xffffffff for value in buffer]
    data = b''.join(struct.pack('>I', value) for value in words)
    Path(args.vectors).write_bytes(data); Path(args.presets).write_bytes(raw)
    report = {'scope': 'Independent integer delay model with synthetic boundary inputs and private tuning only; not original hardware or audible execution',
              'authored_types': [raw[index * 40] for index in range(16)], 'authored_delay_presets': 15,
              'preset_metadata_sha256': hashlib.sha256(raw).hexdigest(), 'cases': len(cases), 'blocks_per_case': 80,
              'output_samples': total, 'full_final_payload_state_compared': True, 'vectors_sha256': hashlib.sha256(data).hexdigest(),
              'delay_parameters': presets, 'word_arithmetic': 'Multiply modulo2^32, signed arithmetic shift7, wrapped feedback addition',
              'quantization': 'Original ceil((milliseconds-5)/5) blocks for valid settings; no invented fractional delay'}
    Path(args.report).write_text(json.dumps(report, indent=2) + '\n'); print(json.dumps({key: value for key, value in report.items() if key != 'delay_parameters'}))


if __name__ == '__main__': main()
