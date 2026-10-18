from pathlib import Path
import tempfile
import unittest
from native_pair_session import first_trace_difference, peer_result, scenario_coverage, save_failed_session


class TraceDifferenceTests(unittest.TestCase):
    def test_failure_report_cleans_up_only_owned_running_processes(self):
        class Process:
            def __init__(self, code): self.returncode, self.killed = code, False
            def poll(self): return self.returncode
            def kill(self): self.killed, self.returncode = True, 1
            def wait(self, timeout): return self.returncode
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            running, exited = Process(None), Process(2)
            report = save_failed_session(root, {'host': running, 'join': exited}, 900, TimeoutError('deadline'))
            self.assertTrue(running.killed)
            self.assertFalse(exited.killed)
            self.assertFalse(report['completed'] or report['verification_completed'])
            self.assertEqual(report['error_type'], 'TimeoutError')
            self.assertIn('evidence_error', report['peers']['host'])
            self.assertTrue((root / 'result.json').exists())

    def test_completion_requires_native_capability_handshake(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'input.pbtrace').write_bytes(b'PBTRACE\x03' + bytes(8) + bytes(68))
            complete = ('Input script: complete (1 samples)\n'
                        'Net diagnostic: closed after 1 paired samples\n')
            (root / 'game.log').write_text(complete)
            self.assertFalse(peer_result(root, 0, 1)['completed'])
            (root / 'game.log').write_text('Net diagnostic: capabilities acknowledged mask=0\n' + complete)
            result = peer_result(root, 0, 1)
            self.assertTrue(result['capabilities_acknowledged'] and result['completed'])
            self.assertFalse(peer_result(root, 2, 1)['completed'])

    def test_scenario_requires_order_and_repeated_visits(self):
        expected = ['mgmodedll.dll', 'm407dll.dll', 'resultDll.dll', 'mgmodedll.dll']
        self.assertTrue(scenario_coverage(['bootDll.dll'] + expected, expected)['passed'])
        self.assertTrue(scenario_coverage([m.upper() for m in expected], expected)['passed'])
        self.assertFalse(scenario_coverage(expected[:1], expected)['passed'])
        self.assertEqual(scenario_coverage(expected[:-1], expected)['remaining'], ['mgmodedll.dll'])
        self.assertFalse(scenario_coverage(list(reversed(expected)), expected)['passed'])
        self.assertFalse(scenario_coverage(expected, ['mgmodedll.dll'] * 3)['passed'])
        self.assertTrue(scenario_coverage([], [])['passed'])

    def test_incomplete_peer_keeps_failure_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'game.log').write_text('objdll> m407dll.dll prolog start\n'
                                          'Net diagnostic: sample 601 stopped (2)\n')
            for data in (None, b'PBTRACE\x03' + bytes(8) + bytes(3)):
                if data is not None:
                    (root / 'input.pbtrace').write_bytes(data)
                result = peer_result(root, 2, 900)
                self.assertFalse(result['completed'])
                self.assertIsNone(result['trace'])
                self.assertTrue(result['trace_error'])
                self.assertEqual(result['stop'], {'sample': 601, 'reason': 2})
                self.assertEqual(result['modules'], ['m407dll.dll'])

    def test_fields_and_lengths(self):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory) / 'a', Path(directory) / 'b'
            original = b'PBTRACE\x03' + bytes(8) + bytes(68 * 2)
            a.write_bytes(original); b.write_bytes(original)
            self.assertIsNone(first_trace_difference(a, b))
            for offset, field in ((0, 'sequence'), (8, 'frand'), (12, 'rand8'),
                                  (19, 'rumble_capability'), (30, 'controllers'), (64, 'selected_state')):
                changed = bytearray(original); changed[16 + 68 + offset] = 128
                b.write_bytes(changed)
                result = first_trace_difference(a, b)
                self.assertEqual((result['sample'], result['field']), (1, field))
            b.write_bytes(original[:-68])
            self.assertEqual(first_trace_difference(a, b), {'sample': 1, 'field': 'length'})
            b.write_bytes(b'bad header')
            self.assertEqual(first_trace_difference(a, b), {'field': 'header'})


if __name__ == '__main__':
    unittest.main()
