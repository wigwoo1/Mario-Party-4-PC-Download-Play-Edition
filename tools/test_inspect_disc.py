import struct
import tempfile
import unittest
from pathlib import Path

from inspect_disc import Disc


class DiscTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'test.ciso'

    def open_disc(self, contents):
        self.path.write_bytes(contents)
        disc = Disc(self.path)
        self.addCleanup(disc.file.close)
        return disc

    def test_sparse_cross_block_read(self):
        header = bytearray(0x8000)
        header[:4] = b'CISO'
        struct.pack_into('<I', header, 4, 512)
        header[8:11] = bytes([1, 0, 1])
        disc = self.open_disc(header + b'A' * 512 + b'B' * 512)
        self.assertEqual(disc.read(500, 536), b'A' * 12 + bytes(512) + b'B' * 12)
        with self.assertRaises(ValueError):
            disc.read(len(disc.blocks) * 512 - 1, 2)

    def test_truncated_ciso(self):
        header = bytearray(0x8000)
        header[:4] = b'CISO'
        struct.pack_into('<I', header, 4, 512)
        header[8] = 1
        self.path.write_bytes(header)
        with self.assertRaisesRegex(ValueError, 'Truncated CISO data'):
            Disc(self.path)

    def test_iso_revision_and_bad_fst(self):
        data = bytearray(0x1000)
        data[:8] = b'GMPE01\x00\x01'
        struct.pack_into('>I', data, 0x1c, 0xc2339f3d)
        struct.pack_into('>III', data, 0x420, 0x500, 0x700, 12)
        struct.pack_into('>III', data, 0x700, 0x1000000, 0, 1)
        disc = self.open_disc(data)
        self.assertEqual(disc.inspect()['version'], 'GMPE01_01')
        disc.file.close()
        struct.pack_into('>I', data, 0x708, 99999)
        disc = self.open_disc(data)
        with self.assertRaisesRegex(ValueError, 'filesystem table size'):
            disc.inspect()


if __name__ == '__main__':
    unittest.main()
