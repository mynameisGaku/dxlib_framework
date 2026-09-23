"""Validation control tests; no compiler or SDK is required by these unit tests."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
import ValidateDebug as validator


class DebugValidationTests(unittest.TestCase):
    def listing(self):
        return {'tests': [{'name': name, 'command': ['/build/' + name], 'properties': []}
                          for name in sorted(validator.REQUIRED)]}

    def cache(self):
        return '\n'.join(name + ':BOOL=OFF' for name in validator.DISABLED_OPTIONS)

    def test_registration_accepts_superset(self):
        listing = self.listing()
        listing['tests'].append({'name': 'AnotherPortable', 'command': ['/build/another']})
        self.assertEqual(len(validator.check_configuration(self.cache(), json.dumps(listing))),
                         len(validator.REQUIRED) + 1)

    def test_missing_disabled_duplicate_and_device_tests_fail(self):
        for kind in ('missing', 'disabled', 'duplicate', 'device', 'no-command'):
            with self.subTest(kind=kind):
                listing = self.listing()
                if kind == 'missing':
                    listing['tests'].pop()
                elif kind == 'duplicate':
                    listing['tests'].append(listing['tests'][0])
                elif kind == 'no-command':
                    listing['tests'][0].pop('command')
                else:
                    listing['tests'][0]['properties'] = [
                        {'name': 'DISABLED', 'value': True} if kind == 'disabled'
                        else {'name': 'LABELS', 'value': ['device']}]
                with self.assertRaises(RuntimeError):
                    validator.check_configuration(self.cache(), json.dumps(listing))

    def test_native_or_missing_cache_option_fails(self):
        for cache in (self.cache().replace('DXF_BUILD_NATIVE:BOOL=OFF', 'DXF_BUILD_NATIVE:BOOL=ON'), ''):
            with self.assertRaises(RuntimeError):
                validator.check_configuration(cache, json.dumps(self.listing()))

    def test_results_require_exact_successful_cases(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'results.xml'
            path.write_text('<testsuite><testcase name="A" status="run"/></testsuite>')
            validator.check_results(path, ['A'])
            for xml in ('', '<testcase name="B" status="run"/>',
                        '<testcase name="A" status="run"><failure/></testcase>',
                        '<testcase name="A" status="notrun"><skipped/></testcase>'):
                path.write_text('<testsuite>' + xml + '</testsuite>')
                with self.assertRaises(RuntimeError):
                    validator.check_results(path, ['A'])

    def test_build_failure_never_runs_old_executable(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            logs = directory / 'logs'
            calls = []

            def run(logs, name, command, **kwargs):
                calls.append(name)
                if name.endswith('-build'):
                    raise RuntimeError('link failed')
                return ''

            with patch.object(sys, 'argv', ['ValidateDebug.py', '--work', str(directory / 'work'),
                                          '--logs', str(logs)]), patch.object(validator, 'run_logged', side_effect=run):
                with self.assertRaises(RuntimeError):
                    validator.main()
            self.assertEqual(calls, ['debug-configure', 'debug-build'])
            summary = json.loads((logs / 'Summary.json').read_text())
            self.assertEqual(summary['status'], 'failed')
            self.assertEqual(summary['configurations'], {})


if __name__ == '__main__':
    unittest.main()
