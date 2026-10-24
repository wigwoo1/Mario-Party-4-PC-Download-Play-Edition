import struct
import unittest
from audit_windows_package_imports import pe_directories, parse_dependents


class ImportsTest(unittest.TestCase):
    def test_regular_and_delay_are_retained(self):
        text = '''File Type: DLL
Image has the following dependencies:
  dol.dll
  KERNEL32.dll
Image has the following delay load dependencies:
  dxcompiler.dll
Summary
  1000 .data
'''
        self.assertEqual(parse_dependents(text, {'regular', 'delay'}), [
            {'name': 'dol.dll', 'kind': 'regular'},
            {'name': 'KERNEL32.dll', 'kind': 'regular'},
            {'name': 'dxcompiler.dll', 'kind': 'delay'}])

    def test_missing_delay_section_is_rejected(self):
        with self.assertRaises(ValueError):
            parse_dependents('File Type: DLL\nImage has the following dependencies:\nKERNEL32.dll\nSummary', {'regular', 'delay'})

    def test_unsafe_or_unrecognized_names_are_rejected(self):
        for name in ['C:\\private\\plugin.dll', '../plugin.dll', 'unexpected diagnostic']:
            with self.subTest(name=name), self.assertRaises(ValueError):
                parse_dependents('File Type: DLL\nImage has the following dependencies:\n'+name+'\nSummary', {'regular'})

    def test_real_directory_presence_drives_parser_expectations(self):
        data = bytearray(512)
        data[:2] = b'MZ'
        struct.pack_into('<I', data, 60, 64)
        data[64:68] = b'PE\0\0'
        struct.pack_into('<H', data, 68, 0x8664)
        struct.pack_into('<H', data, 84, 240)
        struct.pack_into('<H', data, 88, 0x20b)
        struct.pack_into('<I', data, 196, 16)
        struct.pack_into('<I', data, 208, 4096)
        struct.pack_into('<I', data, 304, 8192)
        self.assertEqual(pe_directories(data), {'regular', 'delay'})
        for invalid in [data[:50], b'X'*512]:
            with self.assertRaises(ValueError):
                pe_directories(invalid)


if __name__ == '__main__':
    unittest.main()
