import unittest
from inspect_win_icons import decode_lz


class DecodeTests(unittest.TestCase):
    def test_literal_and_overlapping_ring_reference(self):
        # Three literals at ring958..960, then overlap starting at958.
        self.assertEqual(decode_lz(bytes([7, 65, 66, 67, 190, 195]), 9), b'ABCABCABC')

    def test_zero_initialized_ring(self):
        self.assertEqual(decode_lz(bytes([0, 0, 0]), 3), bytes(3))

    def test_bounds(self):
        for source, size in [(b'', 1), (bytes([1]), 1), (bytes([0, 0, 0]), 2)]:
            with self.assertRaises(ValueError):
                decode_lz(source, size)


if __name__ == '__main__':
    unittest.main()
