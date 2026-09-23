"""Reproduce the documented Starter port in a new external copy, using an existing DxLib SDK."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from PackageRelease import collect_paths
from ValidationSupport import run_logged, validation_report

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, before: str, after: str) -> None:
    data = path.read_bytes()
    old = before.encode('utf-8')
    if data.count(old) != 1:
        raise ValueError(f'Expected one documented edit anchor in {path}: {before}')
    path.write_bytes(data.replace(old, after.encode('utf-8')))


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-parent', type=Path, required=True)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=6)
    args = parser.parse_args()
    parent = args.work_parent.resolve(strict=True)
    sdk = args.sdk_root.resolve(strict=True)
    if parent == ROOT or ROOT in parent.parents or args.jobs < 1:
        parser.error('Use an existing directory outside the repository and jobs >= 1')
    if not (sdk / 'DxLibFbx.json').is_file():
        parser.error('sdk-root must be the existing source-built DxLib SDK')
    copy = Path(tempfile.mkdtemp(prefix='dxf-starter-copy-', dir=parent))
    logs = copy / 'validation'
    summary: dict = {'source': str(ROOT), 'copy': str(copy), 'sdk': str(sdk), 'native': True,
                     'sdk_free_machine': False, 'physical_keys': False, 'listening': False}
    print(f'Copy: {copy}', flush=True)
    with validation_report(logs, summary):
        files = collect_paths(ROOT)
        manifest = {str(p.relative_to(ROOT).as_posix()): digest(p) for p in files}
        summary['source_manifest_sha256'] = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()
        (logs / 'source-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
        for src in files:
            dst = copy / src.relative_to(ROOT)
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
        starter = copy / 'Examples/Starter/Source'
        names = ('SandboxMenuScene.h', 'SandboxMenuScene.cpp', 'SandboxGame.h', 'SandboxGame.cpp')
        for name in names:
            shutil.copy2(copy / 'Examples/Sandbox' / name, starter / name)
        entry = starter / 'WindowsMain.cpp'
        replace_once(entry, '#include "BootScene.h"', '#include "BootScene.h"\r\n#include "SandboxMenuScene.h"')
        replace_once(entry, 'Toolbox::MakeUnique<Starter::ABootScene>()', 'Toolbox::MakeUnique<Dxf::Sandbox::ASandboxMenuScene>()')
        original = 'add_executable(Starter WIN32 Examples/Starter/Source/WindowsMain.cpp Examples/Starter/Source/BootScene.cpp)'
        ported = original[:-1] + ' Examples/Starter/Source/SandboxMenuScene.cpp Examples/Starter/Source/SandboxGame.cpp)'
        replace_once(copy / 'CMakeLists.txt', original, ported)
        build = copy / 'Build/Starter'

        def run(name: str, command: list[str], cwd: Path = copy, env: dict | None = None) -> None:
            run_logged(logs, name, command, cwd=cwd, environment=env, timeout=300)

        run('configure', ['cmake', '-S', str(copy), '-B', str(build), '-A', 'x64',
                         '-DDXF_BUILD_NATIVE=ON', '-DDXF_BUILD_STARTER=ON', '-DDXF_BUILD_EXAMPLE=OFF',
                         '-DDXF_BUILD_TESTS=OFF', '-DDXF_BUILD_NATIVE_SMOKE=OFF', f'-DDXF_DXLIB_CUSTOM_ROOT={sdk}'])
        # Check the actual compiler inputs, not just the target's display name.
        project = (build / 'Starter.vcxproj').read_text(encoding='utf-8-sig')
        sources = re.findall(r'<ClCompile Include="([^"]+)"', project)
        expected = {str(starter / n).lower() for n in ('WindowsMain.cpp', 'BootScene.cpp', *names[1::2])}
        if {str(Path(p)).lower() for p in sources} != expected:
            raise RuntimeError(f'Starter compiles unexpected sources: {sources}')
        summary['starter_sources'] = sources
        (logs / 'Starter-documented.vcxproj').write_text(project, encoding='utf-8')
        foreign = copy / 'unrelated-cwd'
        foreign.mkdir()
        for config in ('Debug', 'Release'):
            run(f'build-{config}', ['cmake', '--build', str(build), '--config', config, '--target', 'Starter', '--parallel', str(args.jobs)])
            exe = build / config / 'Starter.exe'
            summary[f'documented_exe_{config}'] = {'path': str(exe), 'sha256': digest(exe)}
            env = dict(os.environ, DXF_STARTER_TEST_EXE=str(exe))
            # Only close the window belonging to the child we created. No keyboard injection.
            run(f'normal-entry-{config}', ['powershell', '-NoProfile', '-Command',
                '$ErrorActionPreference="Stop"; $p=Start-Process -FilePath $env:DXF_STARTER_TEST_EXE -WindowStyle Hidden -PassThru; '
                'try { if (!$p.WaitForInputIdle(10000)) { throw "No window" }; Start-Sleep -Milliseconds 800; '
                '$p.Refresh(); if (!$p.CloseMainWindow()) { throw "No closeable window" }; '
                'if (!$p.WaitForExit(10000)) { throw "Exit timed out" }; exit $p.ExitCode } '
                'finally { if (!$p.HasExited) { $p.Kill() } }'], foreign, env)

        documented_entry = entry.read_bytes()
        documented_cmake = (copy / "CMakeLists.txt").read_bytes()
        (logs / "WindowsMain-documented.cpp").write_bytes(documented_entry)
        # Instrument only the disposable entry point, after the uninstrumented build and launch succeeded.
        for name in ('StarterCopyProbe.h', 'StarterCopyProbe.cpp'):
            shutil.copy2(copy / 'Tools' / name, starter / name)
        replace_once(entry, '#include "SandboxMenuScene.h"', '#include "SandboxMenuScene.h"\r\n#include "StarterCopyProbe.h"')
        replace_once(entry, 'Dxf::FApplication Application(Backends.GetServices(), Settings);',
                     'const auto Services = Backends.GetServices();\r\n\t\tDxf::FApplication Application({Services.Platform, StarterCopyInput(), Services.Textures, Services.Sounds, Services.Fonts, Services.Renderer, Services.pModels}, Settings);')
        replace_once(entry, 'Runner.Run(Application,', 'RunStarterCopyProbe(Application, Settings.ProjectRoot,')
        # GUI errors must fail the automated run, not wait indefinitely on a modal dialog.
        entry.write_bytes(re.sub(rb'MessageBoxW\([^\r\n]+\);', b'Toolbox::Err << "Starter probe failed";', entry.read_bytes()))
        replace_once(copy / 'CMakeLists.txt', ported, ported[:-1] + ' Examples/Starter/Source/StarterCopyProbe.cpp)')
        with (copy / 'CMakeLists.txt').open('ab') as file:
            file.write(b'\r\nif(TARGET Starter)\r\n target_compile_definitions(Starter PRIVATE DX_NON_USING_NAMESPACE_DXLIB NOMINMAX)\r\n target_compile_options(Starter PRIVATE /UUNICODE /U_UNICODE)\r\nendif()\r\n')
        asset = copy / 'Assets/player.bmp'
        saved = asset.with_suffix('.bmp.saved')
        shutil.copy2(asset, saved)
        original_bytes = asset.read_bytes()
        for config in ('Debug', 'Release'):
            run(f'probe-build-{config}', ['cmake', '--build', str(build), '--config', config, '--target', 'Starter', '--parallel', str(args.jobs)])
            exe = build / config / 'Starter.exe'
            probe_exe = exe.with_name("Starter-probe.exe")
            shutil.copy2(exe, probe_exe)
            summary[f'probe_exe_{config}'] = {'path': str(probe_exe), 'sha256': digest(probe_exe)}
            for index, cwd in enumerate((copy, foreign)):
                try:
                    # Only a verified file inside the new copy is removed, never the repository asset.
                    if asset.resolve().parent != copy / 'Assets' or asset.read_bytes() != original_bytes:
                        raise RuntimeError('Unexpected copy asset path or contents')
                    asset.unlink()
                    run(f'probe-{config}-{index}', [str(exe)], cwd)
                    report = (copy / 'probe-result.txt').read_text(encoding='utf-8')
                    if not report.startswith('PASS:') or asset.read_bytes() != original_bytes:
                        raise RuntimeError('Probe did not restore the asset and pass')
                    shutil.copy2(copy / 'probe-result.txt', logs / f'probe-{config}-{index}.txt')
                    shutil.copytree(exe.parent / 'probe', logs / f'images-{config}-{index}')
                finally:
                    asset.write_bytes(original_bytes)
        # Leave an ordinary playable Starter in the copy; preserve probe binaries separately.
        (logs / 'WindowsMain-probe.cpp').write_bytes(entry.read_bytes())
        entry.write_bytes(documented_entry)
        (copy / 'CMakeLists.txt').write_bytes(documented_cmake)
        for config in ('Debug', 'Release'):
            run(f'restore-build-{config}', ['cmake', '--build', str(build), '--config', config, '--target', 'Starter', '--parallel', str(args.jobs)])
            summary[f'final_exe_{config}'] = {'path': str(build / config / 'Starter.exe'), 'sha256': digest(build / config / 'Starter.exe')}
        for name in names:
            if digest(starter / name) != manifest[f'Examples/Sandbox/{name}']:
                raise RuntimeError(f'The probe modified game code: {name}')
        # Protect original Starter and assets, and detect source drift during this run.
        for src in files:
            if digest(src) != manifest[src.relative_to(ROOT).as_posix()]:
                raise RuntimeError(f'Source changed during validation: {src}')
        summary.update(documented_builds=2, normal_entries=2, instrumented_builds=2,
                       instrumented_runs=4, original_source_unchanged=True)
    print(f'PASS: {logs / "Summary.json"}', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
