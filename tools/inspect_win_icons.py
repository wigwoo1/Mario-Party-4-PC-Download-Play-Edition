"""Read-only location and compression audit of the original window icon asset."""
import argparse
import json
import struct
from inspect_disc import Disc


def decode_lz(source, size):
    if not 0 <= size <= 1024 * 1024:
        raise ValueError('Invalid decoded icon size')
    ring = bytearray(1024)
    position, cursor, flags = 958, 0, 0
    output = bytearray()
    def take():
        nonlocal cursor
        if cursor >= len(source):
            raise ValueError('Truncated LZ stream')
        value = source[cursor]
        cursor += 1
        return value
    while len(output) < size:
        flags >>= 1
        if not flags & 0x100:
            flags = take() | 0xff00
        if flags & 1:
            values = [take()]
            reference = None
        else:
            low, packed = take(), take()
            reference = low | ((packed & 0xc0) << 2)
            values = range((packed & 0x3f) + 3)
        if len(output) + len(values) > size:
            raise ValueError('LZ output overrun')
        for value in values:
            if reference is not None:
                value = ring[reference & 1023]
                reference += 1
            output.append(value)
            ring[position] = value
            position = (position + 1) & 1023
    return bytes(output)


def inspect(disc, decoded_callback=None):
    header = disc.read(0, 0x440)
    offset, size = struct.unpack_from('>II', header, 0x424)
    fst = disc.read(offset, size)
    count = struct.unpack_from('>I', fst, 8)[0]
    if count < 1 or count * 12 > len(fst):
        raise ValueError('Invalid FST')
    directories = [(count, '')]
    for index in range(1, count):
        while index >= directories[-1][0]:
            directories.pop()
        entry, location, length = struct.unpack_from('>III', fst, index * 12)
        start = count * 12 + (entry & 0xffffff)
        end = fst.find(b'\0', start)
        if start >= len(fst) or end < 0:
            raise ValueError('Invalid FST name')
        name = directories[-1][1] + fst[start:end].decode('ascii')
        if entry >> 24:
            if not index < length <= count:
                raise ValueError('Invalid directory extent')
            directories.append((length, name + '/'))
        elif name == 'data/win.bin':
            archive = disc.read(location, length)
            members = struct.unpack_from('>I', archive)[0]
            if members < 4 or (members + 1) * 4 > len(archive):
                raise ValueError('Invalid window archive')
            icon_offset = struct.unpack_from('>I', archive, 4 + 3 * 4)[0]
            if not (members + 1) * 4 <= icon_offset <= len(archive) - 8:
                raise ValueError('Invalid icon offset')
            raw_size, compression = struct.unpack_from('>II', archive, icon_offset)
            next_offset = struct.unpack_from('>I', archive, 4 + 4 * 4)[0] if members > 4 else len(archive)
            if not icon_offset + 8 <= next_offset <= len(archive):
                raise ValueError('Invalid icon member extent')
            if compression != 1:
                raise ValueError('Expected LZ icon member')
            decoded = decode_lz(archive[icon_offset + 8:next_offset], raw_size)
            banks, patterns, bitmaps, uses, bank_offset, pat_offset, bmp_offset = struct.unpack_from('>4h3I', decoded)
            if decoded_callback:
                decoded_callback(decoded)
            if banks < 0 or bank_offset + banks * 8 > len(decoded):
                raise ValueError('Invalid animation banks')
            if patterns < 0 or pat_offset + patterns * 16 > len(decoded):
                raise ValueError('Invalid animation patterns')
            records = []
            for bank in range(banks):
                frames, unknown, frame_offset = struct.unpack_from('>hhI', decoded, bank_offset + bank * 8)
                if frames < 1 or frame_offset + frames * 12 > len(decoded):
                    raise ValueError('Invalid animation frames')
                sequence = []
                for frame in range(frames):
                    pattern, duration, shift_x, shift_y, flip, _ = struct.unpack_from('>6h', decoded, frame_offset + frame * 12)
                    if duration == -1:
                        sequence.append(dict(end=True))
                        continue
                    if not 0 <= pattern < patterns:
                        raise ValueError('Invalid frame pattern')
                    layers, cx, cy, width, height = struct.unpack_from('>5h', decoded, pat_offset + pattern * 16)
                    layer_offset = struct.unpack_from('>I', decoded, pat_offset + pattern * 16 + 12)[0]
                    if layers < 0 or layer_offset + layers * 32 > len(decoded):
                        raise ValueError('Invalid pattern layers')
                    images = []
                    for layer in range(layers):
                        alpha, layer_flip, bitmap, x, y, w, h = struct.unpack_from('>BB5h', decoded, layer_offset + layer * 32)
                        if not 0 <= bitmap < bitmaps:
                            raise ValueError('Invalid layer bitmap')
                        images.append(dict(bitmap=bitmap, crop=[x,y,w,h], alpha=alpha, flip=layer_flip))
                    sequence.append(dict(pattern=pattern, duration=duration, shift=[shift_x,shift_y], flip=flip, layers=images))
                records.append(dict(bank=bank, frames=frames, sequence=sequence))
            return dict(path=name, archive_bytes=length, members=members,
                        icon_member=3, icon_offset=icon_offset,
                        decoded_bytes=raw_size, compression_type=compression,
                        patterns=patterns, bitmaps=bitmaps, banks=records)
    raise ValueError('Window archive not found')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc')
    args = parser.parse_args()
    disc = Disc(args.disc)
    try:
        print(json.dumps(inspect(disc), indent=2))
    finally:
        disc.file.close()
