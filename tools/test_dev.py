import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import dev


class ToolingTests(unittest.TestCase):
    def test_compdb_preserves_quoted_arguments(self):
        result = dev.compilation_database(
            'mkdir -p build/debug\n'
            'cc -std=c11 -I"vendor/ray lib" -c -o build/debug/main.o src/main.c\n'
            'cc -o build/game build/debug/main.o -lm\n')
        self.assertEqual(len(result), 1)
        self.assertIn('-Ivendor/ray lib', result[0]['arguments'])
        self.assertEqual(result[0]['file'], str(dev.ROOT / 'src/main.c'))

    def test_empty_compdb_is_an_error(self):
        with self.assertRaises(ValueError):
            dev.compilation_database('make: Nothing to be done.\n')

    def test_failed_command_is_not_a_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            result = dev.run_step('failure', [sys.executable, '-c',
                                  'print("diagnostic"); raise SystemExit(7)'],
                                  Path(directory), 10)
            self.assertEqual(result['status'], 'failed')
            self.assertEqual(result['exit_code'], 7)
            self.assertIn('diagnostic', Path(result['log']).read_text())

    def test_timeout_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            result = dev.run_step('timeout', [sys.executable, '-c',
                                  'import time; time.sleep(10)'], Path(directory), 0.05)
            self.assertEqual(result['status'], 'failed')
            self.assertIn('timeout', Path(result['log']).read_text())

    def test_missing_executable_is_blocked(self):
        with tempfile.TemporaryDirectory() as directory:
            result = dev.run_step('missing', [str(Path(directory) / 'missing-tool')],
                                  Path(directory), 1)
            self.assertEqual(result['status'], 'blocked')

    def test_doctor_reports_missing_dependencies(self):
        with patch.dict(dev.os.environ, {}, clear=True), patch('dev.shutil.which', return_value=None):
            checks = dev.doctor()
        self.assertTrue(all(check['status'] == 'blocked' for check in checks))

    def test_help_does_not_need_a_toolchain(self):
        result = subprocess.run([sys.executable, str(dev.ROOT / 'tools/dev.py'), '--help'],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('doctor', result.stdout)

    def test_check_writes_blocked_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(dev, 'ROOT', root), patch.object(dev, 'doctor', return_value=[
                {'name': 'make', 'status': 'blocked'}
            ]), patch.object(sys, 'argv', ['dev.py', 'check']), patch('builtins.print'):
                self.assertEqual(dev.main(), 2)
            report = json.loads((root / 'build/checks/summary.json').read_text())
            self.assertEqual(report['status'], 'blocked')

    def test_check_stops_after_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(dev, 'ROOT', Path(directory)), patch.object(dev, 'doctor', return_value=[]), \
                 patch.object(sys, 'argv', ['dev.py', 'check']), patch('builtins.print'), \
                 patch.object(dev, 'run_step', side_effect=[
                     {'name': 'tooling', 'status': 'passed'},
                     {'name': 'release', 'status': 'failed'}]) as run:
                self.assertEqual(dev.main(), 1)
                self.assertEqual(run.call_count, 2)
            report = json.loads((Path(directory) / 'build/checks/summary.json').read_text())
            self.assertEqual(report['status'], 'failed')

    def test_sanitizer_check_is_included_when_requested(self):
        with tempfile.TemporaryDirectory() as directory:
            def passed(name, command, out, timeout):
                return {'name': name, 'status': 'passed'}

            with patch.object(dev, 'ROOT', Path(directory)), patch.object(dev, 'doctor', return_value=[]), \
                 patch.object(sys, 'argv', ['dev.py', 'check', '--sanitizers']), \
                 patch('builtins.print'), patch.object(dev, 'run_step', side_effect=passed):
                self.assertEqual(dev.main(), 0)
            report = json.loads((Path(directory) / 'build/checks/summary.json').read_text())
            self.assertEqual([check['name'] for check in report['checks']],
                             ['tooling', 'release', 'tests', 'release-tests', 'sanitizers'])


if __name__ == '__main__':
    unittest.main()
