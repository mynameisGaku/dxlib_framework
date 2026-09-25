"""Build, install, relocate, and consume the package without network access.

The default run (Native OFF, Debug) needs no DxLib SDK. --config selects Debug or Release. --native also builds the
Windows adapter against the SDK named by --sdk-root (the official VC package or a Tools/DxLibFbx build); the script
never downloads or builds an SDK. --run-device starts the relocated native application on this machine's display.
The consumer sources are in Tools/PackageConsumer and use only the relocated package (find_package).
"""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from ValidationSupport import project_version, run_logged, validation_report

ROOT = Path(__file__).resolve().parents[1]
CONSUMER_SOURCES = ('CMakeLists.txt', 'Main.cpp', 'Support.cpp', 'Physics.cpp', 'NativeApp.cpp')


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--work', type=Path, default=ROOT / 'Build' / 'PackageValidation')
    parser.add_argument('--logs', type=Path, default=None,
                        help='log directory (default: Docs/Validation/Package, or a per-configuration subdirectory)')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--config', choices=('Debug', 'Release'), default='Debug')
    parser.add_argument('--native', action='store_true', help='also build and consume dxf::native')
    parser.add_argument('--sdk-root', type=Path, default=None,
                        help='existing DxLib SDK: an official VC package root or a Tools/DxLibFbx build (DxLibFbx.json)')
    parser.add_argument('--run-device', action='store_true',
                        help='start the relocated native application (opens a window and uses the graphics device)')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if args.native and args.sdk_root is None:
        parser.error('--native needs --sdk-root')
    if args.sdk_root is not None and not args.native:
        parser.error('--sdk-root is only used with --native')
    if args.run_device and not args.native:
        parser.error('--run-device needs --native')
    if args.native and sys.platform != 'win32':
        parser.error('--native needs Windows/MSVC')
    if args.logs is None:
        # 従来の既定（Native OFF・Debug）は同じ場所。その他の構成は別の場所へ分ける。
        name = f"{args.config}-{'Native' if args.native else 'Portable'}"
        args.logs = ROOT / 'Docs' / 'Validation' / 'Package'
        if name != 'Debug-Portable':
            args.logs = args.logs / name
    return args


def sdk_arguments(sdk_root: Path | None) -> list[str]:
    """CMake arguments that point FindDxLib at the SDK the caller named (never the source tree's ThirdParty)."""
    if sdk_root is None:
        return []
    root = sdk_root.resolve()
    if not root.is_dir():
        raise RuntimeError(f'--sdk-root is not a directory: {root}')
    variable = 'DXF_DXLIB_CUSTOM_ROOT' if (root / 'DxLibFbx.json').is_file() else 'DXLIB_ROOT'
    return [f'-D{variable}={root.as_posix()}', '-DDXF_DXLIB_AUTO_SOURCE_BUILD=OFF']


def absolute_paths_in_exports(package: Path, forbidden: list[Path]) -> list[str]:
    """Installed CMake files that still name the source tree or the original build directory."""
    found = []
    needles = []
    for path in forbidden:
        text = str(path.resolve())
        needles.extend({text.lower(), text.replace('\\', '/').lower()})
    for file in package.rglob('*.cmake'):
        content = file.read_text(encoding='utf-8', errors='replace').lower()
        for needle in needles:
            if needle in content:
                found.append(f'{file.relative_to(package)}: {needle}')
    return found


def main() -> int:
    args = parse_arguments()
    args.work.mkdir(parents=True, exist_ok=True)
    args.logs.mkdir(parents=True, exist_ok=True)
    # Each run is clean; never accept an old install as evidence for a failed build.
    work = Path(tempfile.mkdtemp(prefix='run-', dir=args.work.resolve()))
    summary: dict[str, object] = {'real_dxlib_sdk': args.native, 'config': args.config, 'native': args.native,
                                  'device_run': False}

    def run(name: str, command: list[str], timeout: int = 1800, cwd: Path = ROOT) -> str:
        return run_logged(args.logs, name, command, cwd=cwd, timeout=timeout)

    with validation_report(args.logs, summary):
        summary['version'] = project_version(ROOT)
        sdk = sdk_arguments(args.sdk_root if args.native else None)
        if args.native:
            summary['sdk_kind'] = 'source build' if 'DXF_DXLIB_CUSTOM_ROOT' in sdk[0] else 'official VC package'
        run('build-configure', ['cmake', '-S', str(ROOT), '-B', str(work / 'Build'), '-G', 'Ninja',
            f'-DCMAKE_BUILD_TYPE={args.config}', '-DDXF_BUILD_TESTS=OFF',
            f"-DDXF_BUILD_NATIVE={'ON' if args.native else 'OFF'}", '-DDXF_BUILD_NATIVE_SMOKE=OFF',
            '-DDXF_INSTALL=ON'] + sdk)
        run('build', ['cmake', '--build', str(work / 'Build'), '--parallel', str(args.jobs)])
        run('install', ['cmake', '--install', str(work / 'Build'), '--prefix', str(work / 'Original')])
        relocated = work / 'Relocated package'
        shutil.move(str(work / 'Original'), str(relocated))
        # 再配置した後のexportに、元のソースツリーや元のBuildの絶対パスが残っていないこと。
        leaked = absolute_paths_in_exports(relocated, [ROOT, work / 'Build', work / 'Original'])
        (args.logs / 'export-paths.log').write_text('\n'.join(leaked) + f'\nLEAKED={len(leaked)}\n', encoding='utf-8')
        if leaked:
            raise RuntimeError(f'installed CMake files name build-time paths; see {args.logs / "export-paths.log"}')
        print('export-paths: PASS', flush=True)
        consumer = work / 'Consumer'
        consumer.mkdir()
        for name in CONSUMER_SOURCES:
            shutil.copyfile(ROOT / 'Tools' / 'PackageConsumer' / name, consumer / name)
        run('consumer-configure', ['cmake', '-S', str(consumer), '-B', str(work / 'ConsumerBuild'), '-G', 'Ninja',
            f'-DCMAKE_BUILD_TYPE={args.config}', f'-DCMAKE_PREFIX_PATH={relocated.as_posix()}',
            f"-DDXF_VALIDATE_NATIVE={'ON' if args.native else 'OFF'}"] + sdk)
        run('consumer-build', ['cmake', '--build', str(work / 'ConsumerBuild'), '--parallel', str(args.jobs)])
        suffix = '.exe' if sys.platform == 'win32' else ''
        run('consumer-run', [str(work / 'ConsumerBuild' / ('Consumer' + suffix))], timeout=240)
        run('support-only-run', [str(work / 'ConsumerBuild' / ('SupportOnly' + suffix))], timeout=240)
        run('physics-only-run', [str(work / 'ConsumerBuild' / ('PhysicsOnly' + suffix))], timeout=240)
        summary.update(install=True, relocation=True, external_consumer=True, export_paths_relocatable=True,
                       support_without_runtime=True, physics_without_debug_or_support=True,
                       character_physics_only=True, character_gameplay_components=True)
        if args.native:
            # 実行ファイルだけを別のディレクトリへ置き、開発用の.dxfpathsや元のBuildに頼らず起動する。
            deployed = work / 'Deployed'
            deployed.mkdir()
            shutil.copyfile(work / 'ConsumerBuild' / 'NativeApp.exe', deployed / 'NativeApp.exe')
            summary['native_app_built'] = True
            if args.run_device:
                output = run('native-app-run', [str(deployed / 'NativeApp.exe'), str(deployed.resolve())],
                             timeout=240, cwd=deployed)
                if 'NATIVE_CONSUMER_PASSED' not in output:
                    raise RuntimeError('native application did not report success')
                summary['device_run'] = True
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
