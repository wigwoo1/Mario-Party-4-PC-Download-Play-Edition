import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import replay_session as session


class ReplaySessionTests(unittest.TestCase):
    def test_first_divergence_diagnostic(self):
        with tempfile.TemporaryDirectory() as tmp:
            log = Path(tmp) / 'game.log'
            log.write_text('ordinary output\nInput trace divergence: malformed\n')
            self.assertIsNone(session.first_divergence(log))
            for field in ('sequence', 'frand', 'rand8', 'selected_state'):
                log.write_text(f'ordinary output\nInput trace divergence: sample=123 field={field} expected=4294967295 actual=0\n'
                               'Input trace divergence: sample=124 field=frand expected=1 actual=2\n')
                self.assertEqual(session.first_divergence(log),
                                 {'sample': 123, 'field': field, 'expected': 4294967295, 'actual': 0})

    def test_profile_copy_is_independent_and_refuses_reuse(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'source'
            source.mkdir()
            (source / 'card.raw').write_bytes(b'original save')
            expected = session.copy_profile(source, root / 'copy')
            (root / 'copy' / 'card.raw').write_bytes(b'changed save')
            self.assertEqual(expected, session.fingerprint(source))
            with self.assertRaises(FileExistsError):
                session.copy_profile(source, root / 'copy')
            with self.assertRaises(ValueError):
                session.copy_profile(source, source / 'nested')

    def test_trace_header_and_partial_record_validation(self):
        with tempfile.TemporaryDirectory() as tmp:
            trace = Path(tmp) / 'trace'
            header = b'PBTRACE\x03' + struct.pack('<II', 42, 0)
            trace.write_bytes(header + bytes(68))
            self.assertEqual(session.trace_info(trace)['samples'], 1)
            self.assertEqual(session.trace_info(trace)['seed'], 42)
            for data in [b'', header, header + bytes(67), b'PBTRACE\x01' + header[8:] + bytes(68)]:
                trace.write_bytes(data)
                with self.assertRaises(ValueError):
                    session.trace_info(trace)

    def test_record_replay_and_environment_mismatches(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            runtime = root / 'runtime'
            runtime.mkdir()
            exe = runtime / 'partyboard.exe'
            exe.write_bytes(b'native executable fixture')
            disc = root / 'game.ciso'
            disc.write_bytes(b'synthetic disc fixture')
            profile = root / 'source'
            profile.mkdir()
            (profile / 'card.raw').write_bytes(b'pristine')
            args = argparse.Namespace(mode='record', exe=exe, disc=disc, session=root / 'session',
                                      profile=profile, seed=42, samples=None, input_script=None, fast=False)
            complete_replay = True

            def fake_native(command, **kwargs):
                self.assertEqual(kwargs['env']['SDL_AUDIO_DRIVER'], 'dummy')
                copied_profile = Path(command[command.index('--user-dir') + 1])
                self.assertEqual((copied_profile / 'card.raw').read_bytes(), b'pristine')
                (copied_profile / 'card.raw').write_bytes(b'mutated by game')
                if '--record-input' in command:
                    trace = Path(command[command.index('--record-input') + 1])
                    trace.write_bytes(b'PBTRACE\x03' + struct.pack('<II', 42, 0) + bytes(68))
                    if '--input-script' in command:
                        source = Path(command[command.index('--input-script') + 1])
                        self.assertEqual(source.read_bytes()[:8], b'PBTRACE\x02')
                        kwargs['stdout'].write(b'Input script: complete (1 samples); new recording finished\n')
                elif complete_replay:
                    kwargs['stdout'].write(b'Input trace: replay complete (1 samples)\n')
                else:
                    kwargs['stdout'].write(b'Input trace divergence: sample=0 field=rand8 expected=42 actual=44\n')
                return subprocess.CompletedProcess(command, 0)

            with patch.object(session.subprocess, 'run', side_effect=fake_native) as native:
                self.assertEqual(session.run(args), 0)
                args.mode, args.profile, args.seed = 'replay', None, None
                self.assertEqual(session.run(args), 0)
                self.assertEqual(session.run(args), 0) # Each replay starts pristine.
                self.assertEqual(native.call_count, 3)
                self.assertEqual((profile / 'card.raw').read_bytes(), b'pristine')
                complete_replay = False
                self.assertEqual(session.run(args), 2) # Exit 0 alone is not completion.
                reports = [json.loads(p.read_text()) for p in (args.session / 'replays').glob('*/result.json')]
                failed = [r for r in reports if not r['completed']]
                self.assertEqual(len(failed), 1)
                self.assertEqual(failed[0]['divergence'],
                                 {'sample': 0, 'field': 'rand8', 'expected': 42, 'actual': 44})
                trace = args.session / 'input.pbtrace'
                recorded_bytes = trace.read_bytes()
                trace.write_bytes(recorded_bytes + bytes(68))
                with self.assertRaisesRegex(ValueError, 'trace contents changed'):
                    session.run(args)
                trace.write_bytes(recorded_bytes)
                disc.write_bytes(b'different disc')
                with self.assertRaisesRegex(ValueError, 'Disc differs'):
                    session.run(args)
                disc.write_bytes(b'synthetic disc fixture')
                exe.write_bytes(b'changed executable')
                with self.assertRaisesRegex(ValueError, 'Runtime files differ'):
                    session.run(args)
                exe.write_bytes(b'native executable fixture')
                (args.session / 'baseline' / 'card.raw').write_bytes(b'changed baseline')
                with self.assertRaisesRegex(ValueError, 'Baseline profile differs'):
                    session.run(args)
                self.assertEqual(native.call_count, 4) # Rejected before launching.
                args.mode, args.profile, args.samples = 'record', profile, 2
                args.session = root / 'short-recording'
                self.assertEqual(session.run(args), 2) # Stub records only one of two requested samples.
                args.mode, args.profile, args.samples = 'replay', None, None
                with self.assertRaisesRegex(ValueError, 'Recording failed'):
                    session.run(args)
                self.assertEqual(native.call_count, 5)
                stimulus = root / 'old-trace'
                stimulus.write_bytes(b'PBTRACE\x02' + struct.pack('<II', 17, 0) + bytes(68))
                args.mode, args.profile, args.input_script = 'record', profile, stimulus
                args.session = root / 'scripted-recording'
                self.assertEqual(session.run(args), 0)
                self.assertEqual((args.session / 'stimulus.pbtrace').read_bytes(), stimulus.read_bytes())
                manifest = json.loads((args.session / 'manifest.json').read_text())
                self.assertEqual(manifest['input_script']['seed'], 17)
                args.mode, args.profile = 'replay', None
                with self.assertRaisesRegex(ValueError, 'omit'):
                    session.run(args) # A replay cannot use an input-only script.
                args.input_script = None
                complete_replay = True
                self.assertEqual(session.run(args), 0)


if __name__ == '__main__':
    unittest.main()
