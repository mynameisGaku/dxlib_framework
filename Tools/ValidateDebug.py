"""Fresh, SDK-free Debug/Release builds of the canonical debug validation entry."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from ValidationSupport import run_logged, validation_report

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = {
    'DebugTools', 'DebugPhysicsCapture', 'RenderViews', 'NativeViewsTranslation',
    'RenderContinuation', 'RenderTransparency', 'RenderTransparencyFault',
    'NativeTransparency', 'PhysicsContinuation',
    *(f'JobFault-{name}' for name in ('construction', 'submission', 'capture-wait',
                                   'cross-system', 'fence-allocation')),
}
DISABLED_OPTIONS = ('DXF_BUILD_NATIVE', 'DXF_BUILD_NATIVE_SMOKE', 'DXF_RUN_DEVICE_TESTS',
                    'DXF_BUILD_EXAMPLE', 'DXF_BUILD_STARTER', 'DXF_BUILD_MODEL_VIEWER',
                    'DXF_BUILD_RENDER_DEBUG', 'DXF_INSTALL')


def check_configuration(cache: str, listing: str) -> list[str]:
    """Inspect CMake/CTest outputs, not source-file spelling or a fixed case count."""
    entries = dict(line.split('=', 1) for line in cache.splitlines()
                   if '=' in line and not line.startswith(('#', '//')))
    if any(entries.get(name + ':BOOL') != 'OFF' for name in DISABLED_OPTIONS):
        raise RuntimeError('Native/device/examples/install must all be OFF')
    tests = json.loads(listing)['tests']
    names = [test['name'] for test in tests]
    if len(names) != len(set(names)) or not REQUIRED.issubset(names):
        raise RuntimeError('Missing or duplicate required test registration')
    for test in tests:
        properties = {item['name']: item['value'] for item in test.get('properties', [])}
        if not test.get('command') or properties.get('DISABLED'):
            raise RuntimeError('Tests must be executable and enabled')
        if 'device' in properties.get('LABELS', []):
            raise RuntimeError('Real device test registered in SDK-free validation')
    return names


def check_results(path: Path, names: list[str]) -> None:
    """Reject skipped, missing and failed cases even if the caller returned zero."""
    cases = ET.parse(path).getroot().findall('testcase')
    actual = [case.attrib['name'] for case in cases]
    if sorted(actual) != sorted(names):
        raise RuntimeError('CTest results do not match registration')
    if any(case.find('failure') is not None or case.find('error') is not None
           or case.find('skipped') is not None or case.attrib.get('status') != 'run'
           for case in cases):
        raise RuntimeError('A registered test failed or was skipped')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, default=ROOT / 'Build' / 'DebugValidation')
    parser.add_argument('--logs', type=Path, default=ROOT / 'Build' / 'DebugValidationLogs')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    args.work.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='run-', dir=args.work.resolve()))
    summary = {'work': str(work), 'real_dxlib_sdk': False, 'configurations': {}}
    with validation_report(args.logs, summary):
        for config in ('Debug', 'Release'):
            build = work / config
            prefix = config.lower()
            command = ['cmake', '-S', str(ROOT / 'Tools' / 'DebugValidation'),
                       '-B', str(build), '-DCMAKE_BUILD_TYPE=' + config]
            if sys.platform == 'win32':
                command += ['-A', 'x64']
            run_logged(args.logs, prefix + '-configure', command, cwd=ROOT)
            run_logged(args.logs, prefix + '-build',
                       ['cmake', '--build', str(build), '--config', config,
                        '--parallel', str(args.jobs)], cwd=ROOT, timeout=600)
            # Inspect executable registration only after a successful full build.
            listing = run_logged(args.logs, prefix + '-built-registration',
                                 ['ctest', '--test-dir', str(build), '-C', config,
                                  '--show-only=json-v1'], cwd=ROOT)
            names = check_configuration((build / 'CMakeCache.txt').read_text(encoding='utf-8'), listing)
            results = build / 'results.xml'
            run_logged(args.logs, prefix + '-tests',
                       ['ctest', '--test-dir', str(build), '-C', config, '-j', '1',
                        '--no-tests=error', '--output-on-failure', '--output-junit', str(results)],
                       cwd=ROOT, timeout=600)
            check_results(results, names)
            summary['configurations'][config] = {'tests': names, 'count': len(names)}
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
