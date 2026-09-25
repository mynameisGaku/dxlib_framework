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


class PackageOptionTests(unittest.TestCase):
    """The package validator selects configurations explicitly and never starts SDK or device work by default."""

    def setUp(self):
        self.module = load_script('ValidatePackage')
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def parse(self, *arguments):
        with patch.object(self.module, 'ROOT', self.root), patch.object(sys, 'argv', ['ValidatePackage.py', *arguments]):
            return self.module.parse_arguments()

    def test_default_is_portable_debug_in_the_historical_log_directory(self):
        args = self.parse()
        self.assertEqual((args.config, args.native, args.run_device), ('Debug', False, False))
        self.assertEqual(args.logs, self.root / 'Docs' / 'Validation' / 'Package')

    def test_other_configurations_use_their_own_log_directory(self):
        self.assertEqual(self.parse('--config', 'Release').logs,
                         self.root / 'Docs' / 'Validation' / 'Package' / 'Release-Portable')
        if sys.platform == 'win32':
            args = self.parse('--native', '--sdk-root', str(self.root))
            self.assertEqual(args.logs, self.root / 'Docs' / 'Validation' / 'Package' / 'Debug-Native')

    def test_native_needs_an_explicit_sdk_and_device_runs_need_native(self):
        for arguments in (['--native'], ['--run-device'], ['--sdk-root', str(self.root)]):
            with self.assertRaises(SystemExit), patch.object(sys, 'stderr'):
                self.parse(*arguments)

    def test_sdk_kind_follows_the_named_directory(self):
        official = self.module.sdk_arguments(self.root)
        self.assertTrue(official[0].startswith('-DDXLIB_ROOT='))
        (self.root / 'DxLibFbx.json').write_text('{}', encoding='utf-8')
        custom = self.module.sdk_arguments(self.root)
        self.assertTrue(custom[0].startswith('-DDXF_DXLIB_CUSTOM_ROOT='))
        self.assertIn('-DDXF_DXLIB_AUTO_SOURCE_BUILD=OFF', custom)

    def test_export_check_reports_build_time_paths(self):
        package = self.root / 'package'
        (package / 'lib' / 'cmake').mkdir(parents=True)
        (package / 'lib' / 'cmake' / 'Clean.cmake').write_text('set(A "${_IMPORT_PREFIX}/lib")\n', encoding='utf-8')
        self.assertEqual(self.module.absolute_paths_in_exports(package, [self.root / 'source']), [])
        (package / 'lib' / 'cmake' / 'Leaked.cmake').write_text(
            'set(B "' + (self.root / 'source').as_posix() + '/include")\n', encoding='utf-8')
        self.assertEqual(len(self.module.absolute_paths_in_exports(package, [self.root / 'source'])), 1)


if __name__ == '__main__':
    unittest.main()
