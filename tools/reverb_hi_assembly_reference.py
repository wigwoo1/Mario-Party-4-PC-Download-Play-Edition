"""Bounded preserved high-reverb/paired-crosstalk reference; no game audio export."""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import struct
from inspect_disc import Disc
from reverb_std_assembly_reference import Reference, f32


def assembly(source, name):
    body = source.split('static asm void ' + name + '(', 1)[1].split('\n#else', 1)[0].split('{', 1)[1]
    code, labels = [], {}
    for line in body.splitlines():
        line = line.strip()
        if not line or line.startswith(('#', '/*')) or line == '}':
            continue
        if line.endswith(':'): labels[line[:-1]] = len(code)
        else: code.append(line)
    return code, labels, hashlib.sha256(body.encode()).hexdigest()


class HighReference(Reference):
    def __init__(self, source, predelay, crosstalk):
        self.programs = [assembly(source, name) for name in ('HandleReverb', 'DoCrossTalk')]
        self.mem = bytearray(1024 * 1024)
        self.symbol = {'value0_3': 0x100, 'value0_6': 0x108, 'i2fMagic': 0x110}
        self.storef(0x100, f32(.3)); self.storef(0x108, f32(.6))
        struct.pack_into('>d', self.mem, 0x110, 4503601774854144.)
        self.base = 0x1000
        cursor = 0x5000
        for group in range(2):
            for line in range(9):
                header = self.base + group * 180 + line * 20
                count = 5 + line + group * 2
                self.store(header, ((line + 1) % count) * 4)
                self.store(header + 4, ((line + 3) % count) * 4)
                self.store(header + 8, count * 4); self.store(header + 12, cursor)
                self.storef(header + 16, f32((line - 2) / 8))
                for i in range(count): self.storef(cursor + i * 4, f32((i * 3 - line * 2 - group) / 16))
                cursor += count * 4
        self.storef(self.base + 0x168, f32(.45))
        for i in range(9): self.storef(self.base + 0x16c + i * 4, f32(.2 + i * .02))
        for i in range(3): self.storef(self.base + 0x190 + i * 4, f32((i - 1) / 8))
        self.storef(self.base + 0x19c, f32(.65)); self.storef(self.base + 0x1a0, f32(.82))
        self.store(self.base + 0x1a4, predelay); self.storef(self.base + 0x1a8, crosstalk)
        for channel in range(3):
            self.store(self.base + 0x1ac + channel * 4, cursor if predelay else 0)
            self.store(self.base + 0x1b8 + channel * 4, cursor if predelay else 0)
            for i in range(predelay): self.storef(cursor + i * 4, f32((i - channel) / 4))
            # Isolate the original stray pointer write even when beyond a short
            # payload. The C port omits it; it is not normal delay-buffer state.
            cursor += max(predelay * 4, 0x1c0)
        lib = ctypes.CDLL('ucrtbase.dll')
        self.fma = lib.fmaf
        self.fma.argtypes = [ctypes.c_float] * 3; self.fma.restype = ctypes.c_float
        self.channel, self.cross = 0, False

    def setup_arguments(self):
        super().setup_arguments()
        if self.cross:
            self.r[4] = 0x2280
            self.f[1] = f32(self.loadf(self.base + 0x1a8) * f32(.5))
            self.f[2] = f32(f32(1.) - self.f[1])
        else: self.r[5] = self.channel

    def state(self):
        b = self.base
        values = [self.load(b + off) for off in (0x168, 0x19c, 0x1a0, 0x1a8)]
        values += [self.load(b + 0x16c + i * 4) for i in range(9)]
        values += [self.load(b + 0x190 + i * 4) for i in range(3)]
        delay = self.load(b + 0x1a4); values.append(delay)
        for line in range(18):
            header = b + line * 20
            values += [self.load(header + i * 4) for i in (0, 1, 2, 4)]
            size, data = self.load(header + 8), self.load(header + 12)
            values += [self.load(data + i) for i in range(0, size, 4)]
        for channel in range(3):
            data = self.load(b + 0x1ac + channel * 4); ptr = self.load(b + 0x1b8 + channel * 4)
            values.append((ptr - data) // 4 if delay else 0)
            # Slot110 holds an original stray address, never read by owners
            # created with cursor0. Host pointer bytes are intentionally omitted.
            values += [self.load(data + i * 4) for i in range(delay) if i != 110]
        return values

    def block(self, samples):
        output, steps = list(samples), 0
        if self.loadf(self.base + 0x1a8):
            self.cross = True; self.code, self.labels, _ = self.programs[1]
            output, count = super().block(output); steps += count
        self.cross = False; self.code, self.labels, _ = self.programs[0]
        for self.channel in range(3):
            channel = output[self.channel * 160:(self.channel + 1) * 160]
            result, count = super().block(channel + [0] * 320); steps += count
            output[self.channel * 160:(self.channel + 1) * 160] = result[:160]
        return output, steps


def original_disc_evidence(path):
    disc = Disc(path)
    try:
        header = disc.read(0, 0x440)
        assert header[:6] == b'GMPE01' and header[7] == 1
        dol = struct.unpack_from('>I', header, 0x420)[0]; table = disc.read(dol, 256)
        stores = []
        for section in range(7):
            offset = struct.unpack_from('>I', table, section * 4)[0]
            address = struct.unpack_from('>I', table, 0x48 + section * 4)[0]
            length = struct.unpack_from('>I', table, 0x90 + section * 4)[0]
            code = disc.read(dol + offset, length)
            for n in range(0, len(code) - 3, 4):
                word = struct.unpack_from('>I', code, n)[0]
                if word == 0x93be01b8: stores.append(hex(address + n))
        assert stores == ['0x80113868', '0x80113a00']
        metadata = disc.read(550298108 + 160, 40)
        assert struct.unpack_from('>I', metadata)[0] == 0
        return {'disc_id': 'GMPE01', 'revision': 1, 'store_instruction': 'stw r29,0x1b8(r30)',
                'verified_store_addresses': stores, 'aux_preset0_sha256': hashlib.sha256(metadata).hexdigest(),
                'aux_type': 0, 'preset_float_parameters': list(struct.unpack_from('>6f', metadata, 8)),
                'scope': 'Original instruction words and tiny metadata only; no audio payload export'}
    finally: disc.file.close()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'output', 'report', 'disc'): p.add_argument('--' + name, required=True)
    args = p.parse_args(); source = Path(args.source).read_text()
    words = [0x52564831, 10]; steps = 0
    for predelay in (0, 1, 5, 17, 320):
        for crosstalk in (0., f32(.6)):
            ref = HighReference(source, predelay, crosstalk)
            initial = ref.state(); inputs, outputs = [], []
            for block in range(4):
                samples = [((i * 97 + block * 1709) % 32767) - 16384 for i in range(480)]
                for i in range(0, 480, 127): samples[i] = -2147483648 if i % 2 else 2147483647
                output, count = ref.block(samples); steps += count
                inputs += [sample & 0xffffffff for sample in samples]; outputs += output
            words += [4, len(initial)] + initial + inputs + outputs + ref.state()
    data = b''.join(struct.pack('>I', word) for word in words)
    Path(args.output).write_bytes(data)
    result = {'scope': 'Preserved scalar/paired assembly interpreter, synthetic state and constructor-reachable predelay cursors; not hardware execution or complete FPSCR emulation',
              'assembly_sha256': {name: assembly(source, name)[2] for name in ('HandleReverb', 'DoCrossTalk')},
              'cases': 10, 'blocks_per_case': 4, 'predelay_samples': [0, 1, 5, 17, 320], 'crosstalk': [0, f32(.6)],
              'output_samples': 19200, 'instruction_steps': steps, 'vectors_sha256': hashlib.sha256(data).hexdigest(),
              'predelay_policy': 'Preserve fixed owner cursor and observable one-sample delay from original stores, omit stray pointer writes; slot110/host-address bytes excluded from logical-state comparison',
              'rounding': 'Float32/fmaf; paired operations separately rounded; fctiw nearest-even default, no FPSCR exception flags',
              'original_disc_evidence': original_disc_evidence(args.disc)}
    Path(args.report).write_text(json.dumps(result, indent=2) + '\n'); print(json.dumps(result))


if __name__ == '__main__': main()
