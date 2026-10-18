"""Reject malformed coverage independently of the private asset runner."""
import unittest
from validate_actual_effect import parse_source_modes

class SourceModes(unittest.TestCase):
    def test_legacy_missing(self):
        self.assertIsNone(parse_source_modes(b'other diagnostic\n'))
    def test_valid(self):
        self.assertEqual(parse_source_modes(b'other\nSRC_MODES 2 1 3\n'),[2,1,3])
    def test_invalid(self):
        for value in (b'SRC_MODES 1 2\n',b'SRC_MODES 1 2 3 4\n',
                      b'SRC_MODES -1 0 0\n',b'SRC_MODES x 0 0\n',
                      b'SRC_MODES 1 0 0\nSRC_MODES 1 0 0\n'):
            with self.subTest(value=value),self.assertRaises(ValueError):
                parse_source_modes(value)

if __name__=='__main__':
    unittest.main()
