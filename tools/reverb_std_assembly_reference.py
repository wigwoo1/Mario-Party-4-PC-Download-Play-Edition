"""Run the preserved ReverbSTD PowerPC instruction loop over synthetic state.

This deliberately interprets the assembly register/memory operations rather
than calling the portable channel algorithm. Only this finite instruction
subset/normal rounding is supported; it is not a complete PowerPC emulator.
"""
import argparse
import ctypes
import hashlib
import json
import math
import re
import struct
from pathlib import Path

def f32(x):
    return struct.unpack('>f', struct.pack('>f', x))[0]

class Reference:
    def __init__(self, source, predelay):
        body = source.split('static asm void HandleReverb(', 1)[1].split('\n#else', 1)[0]
        body = body.split('{', 1)[1]
        self.reference_hash = hashlib.sha256(body.encode()).hexdigest()
        self.code, self.labels = [], {}
        for line in body.splitlines():
            line = line.strip()
            if not line or line.startswith(('#', '/*')) or line == '}':
                continue
            if line.endswith(':'):
                self.labels[line[:-1]] = len(self.code)
            else:
                self.code.append(line)
        self.mem = bytearray(1024 * 1024)
        self.r = [0] * 32
        self.f = [0.] * 32
        self.eq = [False] * 8
        self.symbol = {'value0_3': 0x100, 'value0_6': 0x108, 'i2fMagic': 0x110}
        self.storef(0x100, f32(.3)); self.storef(0x108, f32(.6))
        struct.pack_into('>d', self.mem, 0x110, 4503601774854144.)
        self.base = 0x1000
        cursor = 0x5000
        for group in range(2):
            for line in range(6):
                header = self.base + group * 120 + line * 20
                count = 5 + line + group * 2
                self.store(header, ((line + 1) % count) * 4)
                self.store(header + 4, ((line + 3) % count) * 4)
                self.store(header + 8, count * 4); self.store(header + 12, cursor)
                self.storef(header + 16, f32((line - 2) / 8))
                for i in range(count):
                    self.storef(cursor + i * 4, f32((i * 3 - line * 2 - group) / 16))
                cursor += count * 4
        self.storef(self.base + 0xf0, f32(.45))
        for i in range(6): self.storef(self.base + 0xf4 + i * 4, f32(.2 + i * .02))
        for i in range(3): self.storef(self.base + 0x10c + i * 4, f32((i - 1) / 8))
        self.storef(self.base + 0x118, f32(.65)); self.storef(self.base + 0x11c, f32(.82))
        self.store(self.base + 0x120, predelay)
        for channel in range(3):
            self.store(self.base + 0x124 + channel * 4, cursor if predelay else 0)
            offset = channel % max(predelay - 1, 1)
            self.store(self.base + 0x130 + channel * 4, cursor + offset * 4 if predelay else 0)
            for i in range(predelay): self.storef(cursor + i * 4, f32((i - channel) / 4))
            cursor += predelay * 4
        lib = ctypes.CDLL('ucrtbase.dll')
        self.fma = lib.fmaf
        self.fma.argtypes = [ctypes.c_float] * 3; self.fma.restype = ctypes.c_float

    def load(self, address): return struct.unpack_from('>I', self.mem, address)[0]
    def store(self, address, value): struct.pack_into('>I', self.mem, address, value & 0xffffffff)
    def loadf(self, address): return struct.unpack_from('>f', self.mem, address)[0]
    def storef(self, address, value): struct.pack_into('>f', self.mem, address, value)
    def imm(self, text):
        if '@ha' in text: return (self.symbol[text.split('@')[0]] + 0x8000) >> 16
        if '@l' in text: return self.symbol[text.split('@')[0]] & 0xffff
        return int(text, 0)
    def address(self, operand):
        offset, reg = re.fullmatch(r'(.+)\(r(\d+)\)', operand).groups()
        return (self.r[int(reg)] + self.imm(offset)) & 0xffffffff
    def state(self):
        """Serialize logical values, not host/console pointers."""
        b = self.base
        values = [self.load(b + off) for off in [0xf0, 0x118, 0x11c]]
        values += [self.load(b + 0xf4 + i * 4) for i in range(6)]
        values += [self.load(b + 0x10c + i * 4) for i in range(3)]
        delay = self.load(b + 0x120); values.append(delay)
        for line in range(12):
            header = b + line * 20
            values += [self.load(header + i * 4) for i in (0, 1, 2, 4)]
            size, data = self.load(header + 8), self.load(header + 12)
            values += [self.load(data + i) for i in range(0, size, 4)]
        for channel in range(3):
            data = self.load(b + 0x124 + channel * 4)
            ptr = self.load(b + 0x130 + channel * 4)
            values.append((ptr - data) // 4 if delay else 0)
            values += [self.load(data + i * 4) for i in range(delay)]
        return values

    def setup_arguments(self):
        self.r[1] = 0x90000; self.r[3] = 0x2000; self.r[4] = self.base

    def block(self, samples):
        self.r = [0] * 32; self.f = [0.] * 32; self.eq = [False] * 8
        self.setup_arguments()
        for i, sample in enumerate(samples): self.store(0x2000 + i * 4, sample)
        pc, ctr, steps = 0, 0, 0
        while pc < len(self.code):
            steps += 1
            if steps > 200000: raise ValueError('Assembly step bound exceeded')
            text = self.code[pc]; pc += 1
            split = text.split(None, 1); op = split[0].rstrip('+')
            args = [s.strip() for s in split[1].split(',')] if len(split) > 1 else []
            ri = lambda x: int(x[1:])
            if op in ('nofralloc',): continue
            if op == 'blr': break
            if op in ('bne', 'beq'):
                cond, label = (int(args[0][2:]), args[1]) if len(args) == 2 else (0, args[0])
                if self.eq[cond] == (op == 'beq'): pc = self.labels[label]
            elif op == 'bdnz':
                ctr -= 1
                if ctr: pc = self.labels[args[0]]
            elif op == 'mtctr': ctr = self.r[ri(args[0])]
            elif op in ('cmpw', 'cmpwi'):
                cond, a, c = (int(args[0][2:]), args[1], args[2]) if len(args) == 3 else (0, *args)
                self.eq[cond] = self.r[ri(a)] == (self.r[ri(c)] if op == 'cmpw' else self.imm(c))
            elif op in ('li', 'lis'):
                self.r[ri(args[0])] = (self.imm(args[1]) << (16 if op == 'lis' else 0)) & 0xffffffff
            elif op in ('add', 'addi', 'mulli', 'slwi', 'xoris'):
                target, a, c = args; a = self.r[ri(a)]
                c = self.r[ri(c)] if op == 'add' else self.imm(c)
                value = a + c if op in ('add', 'addi') else a * c if op == 'mulli' else a << c if op == 'slwi' else a ^ (c << 16)
                self.r[ri(target)] = value & 0xffffffff
            elif op == 'mr': self.r[ri(args[0])] = self.r[ri(args[1])]
            elif op in ('lwz', 'lwzu', 'stw', 'stwu', 'lfs', 'lfd', 'stfs', 'stfd', 'stmw', 'lmw'):
                address = self.address(args[1]); reg = ri(args[0])
                if op == 'stmw':
                    for i in range(reg, 32): self.store(address + (i - reg) * 4, self.r[i])
                elif op == 'lmw':
                    for i in range(reg, 32): self.r[i] = self.load(address + (i - reg) * 4)
                elif op.startswith('lw'): self.r[reg] = self.load(address)
                elif op in ('stw', 'stwu'): self.store(address, self.r[reg])
                elif op == 'lfs': self.f[reg] = self.loadf(address)
                elif op == 'lfd': self.f[reg] = struct.unpack_from('>d', self.mem, address)[0]
                elif op == 'stfs': self.storef(address, self.f[reg])
                elif op == 'stfd': struct.pack_into('>d', self.mem, address, self.f[reg])
                if op in ('lwzu', 'stwu'): self.r[int(re.search(r'r(\d+)\)', args[1])[1])] = address
            elif op in ('lfsx', 'stfsx', 'stfiwx'):
                reg, a, c = map(ri, args)
                address = ((self.r[a] if a else 0) + self.r[c]) & 0xffffffff
                if op == 'lfsx': self.f[reg] = self.loadf(address)
                elif op == 'stfsx': self.storef(address, self.f[reg])
                else: self.store(address, self.f[reg][1])
            elif op == 'fmr': self.f[ri(args[0])] = self.f[ri(args[1])]
            elif op in ('fadds', 'fsubs', 'fmuls'):
                d, a, c = map(ri, args); a, c = self.f[a], self.f[c]
                self.f[d] = f32(a + c if op == 'fadds' else a - c if op == 'fsubs' else a * c)
            elif op in ('fmadds', 'fnmsubs'):
                d, a, c, e = map(ri, args)
                self.f[d] = self.fma(self.f[a], self.f[c], self.f[e]) if op == 'fmadds' else -self.fma(self.f[a], self.f[c], -self.f[e])
            elif op in ('ps_merge00', 'ps_mul', 'ps_muls0', 'ps_sum0'):
                scalar = lambda value: value[0] if isinstance(value, tuple) else value
                pair = lambda value: value if isinstance(value, tuple) else (value, value)
                if op == 'ps_merge00':
                    d, a, c = map(ri, args); self.f[d] = (scalar(self.f[a]), scalar(self.f[c]))
                elif op == 'ps_sum0':
                    d, a, c, e = map(ri, args)
                    self.f[d] = (f32(pair(self.f[a])[0] + pair(self.f[c])[1]), pair(self.f[e])[1])
                else:
                    d, a, c = map(ri, args); a = pair(self.f[a]); c = self.f[c]
                    c = (scalar(c), scalar(c)) if op == 'ps_muls0' else pair(c)
                    self.f[d] = (f32(a[0] * c[0]), f32(a[1] * c[1]))
            elif op in ('fctiwz', 'fctiw'):
                value = self.f[ri(args[1])]
                if isinstance(value, tuple): value = value[0]
                converted = -2147483648 if math.isnan(value) or value < -2147483648 else 2147483647 if value >= 2147483648 else round(value) if op == 'fctiw' else int(value)
                self.f[ri(args[0])] = ('integer', converted)
            else: raise ValueError('Unsupported original instruction: ' + text)
        return [self.load(0x2000 + i * 4) for i in range(480)], steps

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', required=True); p.add_argument('--output', required=True); p.add_argument('--report', required=True)
    args = p.parse_args(); source = Path(args.source).read_text()
    words = [0x52565331, 3]; steps = 0
    for predelay in (0, 5, 17):
        ref = Reference(source, predelay); initial = ref.state(); inputs, outputs = [], []
        for block in range(4):
            samples = [((i * 97 + block * 1709) % 32767) - 16384 for i in range(480)]
            output, count = ref.block(samples); steps += count
            inputs += [s & 0xffffffff for s in samples]; outputs += output
        final = ref.state()
        words += [4, len(initial)] + initial + inputs + outputs + final
    data = b''.join(struct.pack('>I', w) for w in words)
    Path(args.output).write_bytes(data)
    report = {'scope': 'Preserved standard-reverb assembly register/memory interpreter over synthetic state; not hardware execution',
              'assembly_sha256': ref.reference_hash, 'cases': 3, 'blocks_per_case': 4,
              'predelay_samples': [0, 5, 17], 'output_samples': 5760, 'instruction_steps': steps,
              'fused_single_arithmetic': 'Windows CRT fmaf, float32 stores; no FPSCR exception emulation',
              'vectors_sha256': hashlib.sha256(data).hexdigest(), 'result': 'GENERATED'}
    Path(args.report).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))

if __name__ == '__main__': main()
