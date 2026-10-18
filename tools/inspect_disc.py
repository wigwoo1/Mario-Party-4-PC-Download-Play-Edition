"""Read-only GameCube ISO/CISO preflight. Does not extract or distribute assets."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


class Disc:
    def __init__(self, path):
        self.path = Path(path)
        self.size = self.path.stat().st_size
        with self.path.open('rb') as source:
            header = source.read(0x8000)
        self.blocks = None
        self.format = 'ISO'
        if header[:4] == b'CISO':
            self.format = 'CISO'
            if len(header) != 0x8000:
                raise ValueError('Truncated CISO header')
            self.block_size = struct.unpack_from('<I', header, 4)[0]
            if not 512 <= self.block_size <= 32 * 1024 * 1024:
                raise ValueError('Invalid CISO block size')
            self.blocks = []
            stored = 0
            for flag in header[8:]:
                if flag not in (0, 1):
                    raise ValueError('Invalid CISO block map')
                self.blocks.append(stored if flag else None)
                stored += flag
            if 0x8000 + stored * self.block_size > self.size:
                raise ValueError('Truncated CISO data')
        self.file = self.path.open('rb')

    def read(self, offset, size):
        if offset < 0 or size < 0 or size > 64 * 1024 * 1024:
            raise ValueError('Invalid read range')
        if self.blocks is None:
            self.file.seek(offset)
            result = self.file.read(size)
            if len(result) != size:
                raise ValueError('Truncated disc data')
            return result
        if offset + size > len(self.blocks) * self.block_size:
            raise ValueError('Read beyond CISO map')
        result = bytearray()
        while size:
            block, within = divmod(offset, self.block_size)
            count = min(size, self.block_size - within)
            physical = self.blocks[block]
            if physical is None:
                result.extend(bytes(count))
            else:
                self.file.seek(0x8000 + physical * self.block_size + within)
                data = self.file.read(count)
                if len(data) != count:
                    raise ValueError('Truncated CISO block')
                result.extend(data)
            offset += count
            size -= count
        return bytes(result)

    def inspect(self):
        header = self.read(0, 0x440)
        if struct.unpack_from('>I', header, 0x1c)[0] != 0xc2339f3d:
            raise ValueError('Not a GameCube disc')
        game_id = header[:6].decode('ascii')
        revision = header[7]
        dol_offset, fst_offset, fst_size = struct.unpack_from('>III', header, 0x420)
        dol = self.read(dol_offset, 0x100)
        fst = self.read(fst_offset, fst_size)
        if len(fst) < 12 or fst[0] != 1:
            raise ValueError('Invalid filesystem root')
        count = struct.unpack_from('>I', fst, 8)[0]
        if count < 1 or count * 12 > len(fst):
            raise ValueError('Invalid filesystem table size')
        files = 0
        for i in range(1, count):
            entry, offset, length = struct.unpack_from('>III', fst, i * 12)
            name_offset = count * 12 + (entry & 0xffffff)
            if name_offset >= len(fst) or fst.find(b'\0', name_offset) < 0:
                raise ValueError('Invalid filesystem name')
            if entry >> 24 == 0:
                files += 1
                if length:
                    self.read(offset + length - 1, 1)
            elif entry >> 24 != 1 or not i < length <= count:
                raise ValueError('Invalid filesystem directory')
        dol_hash = hashlib.sha256()
        sections = 0
        for i in range(18):
            offset = struct.unpack_from('>I', dol, i * 4)[0]
            length = struct.unpack_from('>I', dol, 0x90 + i * 4)[0]
            if length:
                dol_hash.update(self.read(dol_offset + offset, length))
                sections += 1
        return {
            'path': str(self.path.resolve()), 'format': self.format,
            'stored_bytes': self.size, 'game_id': game_id, 'revision': revision,
            'version': f'{game_id}_{revision:02d}',
            'title': header[0x20:0x400].split(b'\0')[0].decode('ascii', errors='replace'),
            'supported_usa_revision': game_id == 'GMPE01' and revision in (0, 1),
            'filesystem_entries': count, 'files': files, 'dol_sections': sections,
            'dol_sections_sha256': dol_hash.hexdigest(),
            'validation': 'Structural preflight only; not a full known-good disc hash comparison',
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    disc = None
    try:
        disc = Disc(args.disc)
        report = disc.inspect()
        result = json.dumps(report, indent=2)
        if args.output:
            args.output.write_text(result + '\n', encoding='utf-8')
        print(result)
        return 0 if report['supported_usa_revision'] else 2
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f'Disc preflight failed: {error}\n')
    finally:
        if disc:
            disc.file.close()


if __name__ == '__main__':
    raise SystemExit(main())
