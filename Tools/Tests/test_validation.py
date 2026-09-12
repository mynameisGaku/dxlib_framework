"""Failure-path tests: a previous successful validation must never remain current."""
from pathlib import Path
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))


def load_script(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class ValidationFailureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.24)\n'
            'project(dxlib_framework VERSION 9.8.7 LANGUAGES CXX)\n', encoding='utf-8')
        self.logs = self.root / 'Docs' / 'Validation'
        self.logs.mkdir(parents=True)

    def seed_success(self, directory):
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'Summary.json').write_text(json.dumps({
            'status': 'passed', 'version': 'old-version',
            'profiles': {'old-run': {'framework_cases': 999}}, 'install': True}), encoding='utf-8')

    def assert_failed_current_run(self, directory):
        data = json.loads((directory / 'Summary.json').read_text(encoding='utf-8'))
        self.assertEqual(data['status'], 'failed')
        self.assertEqual(data['version'], '9.8.7')
        self.assertNotIn('old-run', data.get('profiles', {}))
        self.assertFalse(data.get('install', False))
        self.assertTrue(data.get('error'))
        self.assertTrue(data.get('started_utc'))
        self.assertTrue(data.get('finished_utc'))

    def test_failed_portable_configure_replaces_stale_success(self):
        module = load_script('Validate')
        self.seed_success(self.logs)
        with patch.object(module, 'ROOT', self.root), patch.object(sys, 'argv', ['Validate.py']), \
             patch.object(subprocess, 'run', side_effect=[subprocess.CompletedProcess(['python'], 0, 'no-stl passed'),
                                                         subprocess.CompletedProcess(['cmake'], 1, 'configure failed')]):
            with self.assertRaises(RuntimeError):
                module.main()
        self.assert_failed_current_run(self.logs)

    def test_failed_package_configure_replaces_stale_success(self):
        module = load_script('ValidatePackage')
        logs = self.logs / 'Package'
        self.seed_success(logs)
        with patch.object(module, 'ROOT', self.root), patch.object(sys, 'argv', ['ValidatePackage.py']), \
             patch.object(subprocess, 'run', return_value=subprocess.CompletedProcess(['cmake'], 1, 'configure failed')):
            with self.assertRaises(RuntimeError):
                module.main()
        self.assert_failed_current_run(logs)

    def test_timeout_keeps_partial_log_and_marks_current_run_failed(self):
        module = load_script('Validate')
        self.seed_success(self.logs)
        error = subprocess.TimeoutExpired(['cmake'], 180, output=b'partial compiler output\n')
        with patch.object(module, 'ROOT', self.root), patch.object(sys, 'argv', ['Validate.py']), \
             patch.object(subprocess, 'run', side_effect=[subprocess.CompletedProcess(['python'], 0, 'no-stl passed'), error]):
            with self.assertRaises(subprocess.TimeoutExpired):
                module.main()
        log = self.logs / 'debug-configure.log'
        self.assertTrue(log.exists(), 'Timeout lost the command and partial output')
        text = log.read_text(encoding='utf-8')
        self.assertIn('partial compiler output', text)
        self.assertIn('EXIT_CODE=TIMEOUT', text)
        self.assert_failed_current_run(self.logs)


if __name__ == '__main__':
    unittest.main()
