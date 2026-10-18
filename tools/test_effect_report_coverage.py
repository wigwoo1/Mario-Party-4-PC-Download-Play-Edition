"""Exercise report acceptance independently of private audio assets."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class ReportCoverage(unittest.TestCase):
    def setUp(self):
        self.rows = [{'metrics': [index, 1, int(index >= 236)],
                      'invalid_fxid_rejected': index < 236,
                      'selected_source_mode_counts': [int(index >= 236), 0, 0]}
                     for index in range(2308)]

    def run_report(self, rows):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            baseline, report, output = [root / name for name in
                                        ('baseline.json', 'report.json', 'output.json')]
            baseline.write_text(json.dumps({'metrics': self.rows}))
            report.write_text(json.dumps({'metrics': rows}))
            result = subprocess.run([sys.executable,
                str(Path(__file__).with_name('validate_effect_report_coverage.py')),
                '--baseline', str(baseline), '--report', str(report),
                '--output', str(output)], capture_output=True, text=True)
            return result, json.loads(output.read_text()) if output.exists() else None

    def test_complete_metadata(self):
        result, report = self.run_report(self.rows)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(report['source_mode_metadata_rows'], 2308)
        self.assertEqual(report['selected_source_mode_counts'], [2072, 0, 0])

    def test_legacy_metadata_not_claimed(self):
        rows = copy.deepcopy(self.rows)
        for row in rows:
            del row['selected_source_mode_counts']
        result, report = self.run_report(rows)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(report['source_mode_metadata_rows'], 0)
        self.assertIsNone(report['selected_source_mode_counts'])

    def test_malformed_mode_evidence_rejected(self):
        for modes in ([1, 0], [True, 0, 0], [1.0, 0, 0], [-1, 2, 0],
                      [0, 0, 0], '100'):
            with self.subTest(modes=modes):
                rows = copy.deepcopy(self.rows)
                rows[236]['selected_source_mode_counts'] = modes
                result, report = self.run_report(rows)
                self.assertNotEqual(result.returncode, 0)
                self.assertIsNone(report)
                self.assertIn('invalid source mode coverage', result.stderr)

    def test_missing_descriptor_rejected(self):
        result, report = self.run_report(self.rows[:-1])
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(report)
        self.assertIn('Incomplete full descriptor coverage', result.stderr)


if __name__ == '__main__':
    unittest.main()
