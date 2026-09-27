"""Failure-path tests: a previous validation run is never overwritten and never read as the current one."""
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


def snapshot(directory):
    return {path.name: path.read_bytes() for path in directory.iterdir()}


class ValidationFailureTests(unittest.TestCase):
    """A failed run is recorded in its own new run directory; an earlier success stays byte-identical."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.24)\n'
            'project(dxlib_framework VERSION 9.8.7 LANGUAGES CXX)\n', encoding='utf-8')

    def seed_success(self, directory):
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'Summary.json').write_text(json.dumps({
            'status': 'passed', 'version': 'old-version',
            'profiles': {'old-run': {'framework_cases': 999}}, 'install': True}), encoding='utf-8')
        (directory / 'debug-configure.log').write_text('SENTINEL old log\n', encoding='utf-8')
        return snapshot(directory)

    def only_run(self, parent, excluded=None):
        runs = [path for path in parent.iterdir() if path.is_dir() and path != excluded]
        self.assertEqual(len(runs), 1)
        return runs[0]

    def assert_failed_current_run(self, directory):
        data = json.loads((directory / 'Summary.json').read_text(encoding='utf-8'))
        self.assertEqual(data['status'], 'failed')
        self.assertEqual(data['version'], '9.8.7')
        self.assertNotIn('old-run', data.get('profiles', {}))
        self.assertFalse(data.get('install', False))
        self.assertTrue(data.get('error'))
        self.assertTrue(data.get('started_utc'))
        self.assertTrue(data.get('finished_utc'))
        self.assertTrue(data.get('source_sha256'))
        return data

    def test_failed_portable_configure_writes_a_new_run_and_keeps_the_old_success(self):
        module = load_script('Validate')
        old = self.root / 'Build' / 'ValidationLogs' / 'Portable' / 'old'
        before = self.seed_success(old)
        with patch.object(module, 'ROOT', self.root), patch.object(sys, 'argv', ['Validate.py']), \
             patch.object(subprocess, 'run', side_effect=[subprocess.CompletedProcess(['python'], 0, 'no-stl passed'),
                                                         subprocess.CompletedProcess(['cmake'], 1, 'configure failed')]):
            with self.assertRaises(RuntimeError):
                module.main()
        self.assertEqual(snapshot(old), before)
        data = self.assert_failed_current_run(self.only_run(old.parent, old))
        self.assertEqual([step['exit'] for step in data['steps']], [0, 1])

    def test_failed_package_configure_writes_a_new_run(self):
        module = load_script('ValidatePackage')
        with patch.object(module, 'ROOT', self.root), patch.object(sys, 'argv', ['ValidatePackage.py']), \
             patch.object(subprocess, 'run', return_value=subprocess.CompletedProcess(['cmake'], 1, 'configure failed')):
            with self.assertRaises(RuntimeError):
                module.main()
        parent = self.root / 'Build' / 'ValidationLogs' / 'Package' / 'Debug-Portable'
        self.assert_failed_current_run(self.only_run(parent))

    def test_timeout_keeps_partial_log_and_marks_current_run_failed(self):
        module = load_script('Validate')
        error = subprocess.TimeoutExpired(['cmake'], 180, output=b'partial compiler output\n')
        with patch.object(module, 'ROOT', self.root), patch.object(sys, 'argv', ['Validate.py']), \
             patch.object(subprocess, 'run', side_effect=[subprocess.CompletedProcess(['python'], 0, 'no-stl passed'), error]):
            with self.assertRaises(subprocess.TimeoutExpired):
                module.main()
        run = self.only_run(self.root / 'Build' / 'ValidationLogs' / 'Portable')
        log = run / 'debug-configure.log'
        self.assertTrue(log.exists(), 'Timeout lost the command and partial output')
        text = log.read_text(encoding='utf-8')
        self.assertIn('partial compiler output', text)
        self.assertIn('EXIT_CODE=TIMEOUT', text)
        data = self.assert_failed_current_run(run)
        self.assertEqual(data['steps'][-1]['exit'], 'TIMEOUT')

    def test_explicit_log_directory_with_files_is_rejected_before_any_process(self):
        module = load_script('ValidatePackage')
        old = self.root / 'Old logs 日本語'
        before = self.seed_success(old)
        with patch.object(module, 'ROOT', self.root), \
             patch.object(sys, 'argv', ['ValidatePackage.py', '--logs', str(old)]), \
             patch.object(subprocess, 'run') as run:
            with self.assertRaises(RuntimeError):
                module.main()
            run.assert_not_called()
        self.assertEqual(snapshot(old), before)


class RunDirectoryTests(unittest.TestCase):
    """The shared run contract: exclusive run directories, no truncated steps, recorded state."""

    def setUp(self):
        self.support = load_script('ValidationSupport')
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / 'Root with space 日本語'
        (self.root / 'Source').mkdir(parents=True)
        (self.root / 'Source' / 'a.cpp').write_bytes(b'int a;\r\n')

    def test_new_run_directories_never_reuse_a_name(self):
        parent = self.root / 'Runs'
        first = self.support.new_run_directory(parent)
        second = self.support.new_run_directory(parent)
        self.assertNotEqual(first, second)
        self.assertEqual(list(first.iterdir()), [])

    def test_second_claim_of_the_same_directory_fails_and_keeps_the_first_summary(self):
        logs = self.root / 'Logs'
        with self.support.validation_report(logs, {}):
            before = (logs / 'Summary.json').read_bytes()
            with self.assertRaises(self.support.LogsInUseError):
                with self.support.validation_report(logs, {}):
                    pass
            self.assertEqual((logs / 'Summary.json').read_bytes(), before)

    def test_concurrent_claim_of_an_empty_directory_has_one_winner(self):
        logs = self.root / 'Logs'
        logs.mkdir()
        # The other run created Summary.json between our emptiness check and our claim.
        original = self.support.check_new_logs

        def race(path):
            original(path)
            (path / 'Summary.json').write_text('{"status": "running", "run_id": "other"}', encoding='utf-8')

        with patch.object(self.support, 'check_new_logs', side_effect=race):
            with self.assertRaises(self.support.LogsInUseError):
                with self.support.validation_report(logs, {}):
                    pass
        self.assertIn('other', (logs / 'Summary.json').read_text(encoding='utf-8'))

    def test_repeated_step_name_does_not_truncate_the_earlier_log(self):
        logs = self.root / 'Logs'
        with patch.object(subprocess, 'run', return_value=subprocess.CompletedProcess(['x'], 0, 'first output')):
            with self.support.validation_report(logs, {}):
                self.support.run_logged(logs, 'step', ['x'], cwd=self.root)
                before = (logs / 'step.log').read_bytes()
                with self.assertRaises(self.support.LogsInUseError):
                    self.support.run_logged(logs, 'step', ['x'], cwd=self.root)
                self.assertEqual((logs / 'step.log').read_bytes(), before)
                self.support.run_logged(logs, 'step-retry-2', ['x'], cwd=self.root)

    def test_interruption_is_recorded_as_interrupted_not_passed(self):
        logs = self.root / 'Logs'
        with self.assertRaises(KeyboardInterrupt):
            with self.support.validation_report(logs, {}):
                raise KeyboardInterrupt
        data = json.loads((logs / 'Summary.json').read_text(encoding='utf-8'))
        self.assertEqual(data['status'], 'interrupted')

    def test_not_started_process_is_a_failure_with_its_log(self):
        logs = self.root / 'Logs'
        with self.assertRaises(OSError):
            with self.support.validation_report(logs, {}):
                self.support.run_logged(logs, 'missing', [str(self.root / 'missing.exe')], cwd=self.root)
        data = json.loads((logs / 'Summary.json').read_text(encoding='utf-8'))
        self.assertEqual(data['status'], 'failed')
        self.assertEqual(data['steps'][0]['exit'], 'NOT_STARTED')
        self.assertIn('EXIT_CODE=NOT_STARTED', (logs / 'missing.log').read_text(encoding='utf-8'))

    def test_structured_checks_are_recorded_from_successful_output(self):
        logs = self.root / 'Logs'
        output = 'DXF_CHECK os_pointer_capture=verified\nDXF_CHECK window_focus=not_exercised\nother\n'
        with patch.object(subprocess, 'run', return_value=subprocess.CompletedProcess(['x'], 0, output)):
            with self.support.validation_report(logs, {}):
                self.support.run_logged(logs, 'device', ['x'], cwd=self.root)
        data = json.loads((logs / 'Summary.json').read_text(encoding='utf-8'))
        self.assertEqual(data['status'], 'passed')
        self.assertEqual([(check['name'], check['state']) for check in data['checks']],
                         [('os_pointer_capture', 'verified'), ('window_focus', 'not_exercised')])
        self.assertIn('DXF_CHECK os_pointer_capture=verified', (logs / 'device.log').read_text(encoding='utf-8'))

    def test_source_manifest_hashes_the_working_bytes(self):
        first, entries = self.support.source_manifest(self.root)
        self.assertEqual([path for path, _ in entries], ['Source/a.cpp'])
        (self.root / 'Source' / 'a.cpp').write_bytes(b'int a;\n')
        second, _ = self.support.source_manifest(self.root)
        self.assertNotEqual(first, second)
        (self.root / 'Source' / '__pycache__').mkdir()
        (self.root / 'Source' / '__pycache__' / 'x.pyc').write_bytes(b'x')
        self.assertEqual(self.support.source_manifest(self.root)[0], second)


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

    def test_default_is_portable_debug_with_a_per_configuration_run_parent(self):
        args = self.parse()
        self.assertEqual((args.config, args.native, args.run_device), ('Debug', False, False))
        self.assertIsNone(args.logs)
        self.assertEqual(args.logs_parent, self.root / 'Build' / 'ValidationLogs' / 'Package' / 'Debug-Portable')

    def test_other_configurations_use_their_own_log_parent(self):
        self.assertEqual(self.parse('--config', 'Release').logs_parent,
                         self.root / 'Build' / 'ValidationLogs' / 'Package' / 'Release-Portable')
        if sys.platform == 'win32':
            args = self.parse('--native', '--sdk-root', str(self.root))
            self.assertEqual(args.logs_parent, self.root / 'Build' / 'ValidationLogs' / 'Package' / 'Debug-Native')

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
