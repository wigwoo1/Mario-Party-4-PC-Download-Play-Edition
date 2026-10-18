"""Bounds regression for metadata used to connect private stream decoding."""
import struct
import unittest
from inspect_audio_assets import inspect


class MemoryDisc:
    def __init__(self, image):
        self.image = image

    def read(self, offset, size):
        result = self.image[offset:offset + size]
        if len(result) != size:
            raise ValueError('Truncated disc')
        return result


def fixture():
    image = bytearray(0x800)
    names = b'\0sound\0test.pdt\0'
    fst = struct.pack('>9I', 0x1000000, 0, 3, 0x1000001, 0, 3, 7, 0x600, 164) + names
    struct.pack_into('>II', image, 0x424, 0x480, len(fst))
    image[0x480:0x480+len(fst)] = fst
    struct.pack_into('>hh7I', image, 0x600, 1, 1, 2, 32000, 2, 32, 36, 68, 100)
    struct.pack_into('>I', image, 0x620, 68)
    struct.pack_into('>6bHII', image, 0x644, 0, 127, 64, 0, 0, 0, 32000, 128, 2)
    struct.pack_into('>IhH', image, 0x654, 100, 0, 0)
    return image


class StreamBounds(unittest.TestCase):
    def msm_fixture(self):
        image = fixture()
        image[0x480 + 36:0x480 + 52] = b'\0sound\0test.msm\0'
        struct.pack_into('>I', image, 0x480 + 32, 256)
        image[0x600:0x700] = bytes(256)
        struct.pack_into('>24I', image, 0x600, 0, 2,
            0, 0, 0, 0, 0, 0, 96, 32, 0, 0, 0, 0,
            128, 96, 224, 32, 0, 0, 0, 0, 0, 0)
        struct.pack_into('>Hbb4I', image, 0x660, 1, 0, 0, 0, 96, 0, 32)
        struct.pack_into('>4I', image, 0x680, 0, 0, 16, 0)
        struct.pack_into('>I', image, 0x690 + 16, 14)
        struct.pack_into('>I', image, 0x690 + 28, 40)
        struct.pack_into('>H', image, 0x6b0, 65535)
        return image

    def test_msm_sample_parameters(self):
        sample = inspect(MemoryDisc(self.msm_fixture()))['assets'][0]['groups'][0]['dsp_samples'][0]
        self.assertEqual(sample['encoded_bytes'], 8)
        self.assertEqual(len(sample['coefficient_pairs']), 8)

    def test_msm_sample_payload_overflow(self):
        image = self.msm_fixture()
        struct.pack_into('>I', image, 0x694, 32)
        with self.assertRaisesRegex(ValueError, 'sample outside'):
            inspect(MemoryDisc(image))

    def test_msm_parameter_overflow(self):
        image = self.msm_fixture()
        struct.pack_into('>I', image, 0x6ac, 80)
        with self.assertRaisesRegex(ValueError, 'parameters outside'):
            inspect(MemoryDisc(image))

    def test_msm_loop_overflow(self):
        image = self.msm_fixture()
        struct.pack_into('>II', image, 0x6a4, 13, 2)
        with self.assertRaisesRegex(ValueError, 'loop outside'):
            inspect(MemoryDisc(image))

    def test_msm_unaligned_loop(self):
        image = self.msm_fixture()
        struct.pack_into('>II', image, 0x6a4, 3, 7)
        sample = inspect(MemoryDisc(image))['assets'][0]['groups'][0]['dsp_samples'][0]
        self.assertEqual((sample['loop_start'], sample['loop_length']), (3, 7))

    def test_msm_pcm16_span(self):
        image = self.msm_fixture()
        struct.pack_into('>I', image, 0x6a0, 0x0200000e)
        sample = inspect(MemoryDisc(image))['assets'][0]['groups'][0]['pcm16_samples'][0]
        self.assertEqual(sample['encoded_bytes'], 28)
        struct.pack_into('>I', image, 0x694, 5)
        with self.assertRaisesRegex(ValueError, 'PCM sample outside'):
            inspect(MemoryDisc(image))

    def test_valid_table(self):
        stream = inspect(MemoryDisc(fixture()))['assets'][0]['streams'][0]
        self.assertEqual(stream['channels'][0]['sample_offset'], 100)
        self.assertEqual(len(stream['channels'][0]['coefficient_pairs']), 8)
        self.assertEqual(stream['decoded_samples'], 112)

    def test_parameter_index_outside_table(self):
        image = fixture()
        struct.pack_into('>h', image, 0x658, 1)
        with self.assertRaisesRegex(ValueError, 'coefficient range'):
            inspect(MemoryDisc(image))

    def test_sample_span_outside_file(self):
        image = fixture()
        struct.pack_into('>I', image, 0x64c, 256)
        with self.assertRaisesRegex(ValueError, 'sample or coefficient'):
            inspect(MemoryDisc(image))

    def test_pack_outside_table(self):
        image = fixture()
        struct.pack_into('>I', image, 0x620, 99)
        with self.assertRaisesRegex(ValueError, 'pack outside'):
            inspect(MemoryDisc(image))

    def test_reversed_sections(self):
        image = fixture()
        struct.pack_into('>I', image, 0x614, 80)
        with self.assertRaisesRegex(ValueError, 'section ordering'):
            inspect(MemoryDisc(image))


if __name__ == '__main__':
    unittest.main()
