"""Build, install, relocate, and consume the package without SDK/network access."""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from ValidationSupport import project_version, run_logged, validation_report

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, default=ROOT / 'Build' / 'PackageValidation')
    parser.add_argument('--logs', type=Path, default=ROOT / 'Docs' / 'Validation' / 'Package')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    args.work.mkdir(parents=True, exist_ok=True)
    args.logs.mkdir(parents=True, exist_ok=True)
    # Each run is clean; never accept an old install as evidence for a failed build.
    work = Path(tempfile.mkdtemp(prefix='run-', dir=args.work.resolve()))

    summary: dict[str, object] = {'real_dxlib_sdk': False}

    def run(name: str, command: list[str]) -> None:
        run_logged(args.logs, name, command, cwd=ROOT, timeout=240)

    with validation_report(args.logs, summary):
        summary['version'] = project_version(ROOT)
        run('build-configure', ['cmake', '-S', str(ROOT), '-B', str(work / 'Build'), '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Debug', '-DDXF_BUILD_TESTS=OFF', '-DDXF_BUILD_NATIVE=OFF', '-DDXF_INSTALL=ON'])
        run('build', ['cmake', '--build', str(work / 'Build'), '--parallel', str(args.jobs)])
        run('install', ['cmake', '--install', str(work / 'Build'), '--prefix', str(work / 'Original')])
        relocated = work / 'Relocated package'
        shutil.move(str(work / 'Original'), str(relocated))
        consumer = work / 'Consumer'
        consumer.mkdir()
        (consumer / 'Main.cpp').write_text('''#include "Dxf/GameScene.h"
#include "Dxf/RenderQueue2D.h"
int main()
{
    Dxf::DGameScene Scene;
    Scene.Shutdown_Internal();
    Dxf::FRenderQueue2D Queue;
    return Queue.Submit(Dxf::FRectangleCommand{}) ? 1 : 0;
}
''', encoding='utf-8')
        (consumer / 'Support.cpp').write_text('''#include "Dxf/RenderQueue2D.h"
#include "Dxf/ModelImport.h"
int main()
{
    if (Dxf::ImportFbxModel(nullptr, 0)) { return 2; }
    Dxf::FRenderQueue2D Queue;
    return Queue.Submit(Dxf::FRectangleCommand{}) ? 1 : 0;
}
''', encoding='utf-8')
        (consumer / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(RelocatedConsumer LANGUAGES CXX)
find_package(dxlib_framework CONFIG REQUIRED)
add_executable(Consumer Main.cpp)
target_link_libraries(Consumer PRIVATE dxf::framework)
add_executable(SupportOnly Support.cpp)
target_link_libraries(SupportOnly PRIVATE dxf::support)
if(NOT TARGET dxf::toolbox OR NOT TARGET dxf::foundation OR NOT TARGET dxf::support OR NOT TARGET dxf::runtime OR NOT TARGET dxf::gameplay)
    message(FATAL_ERROR "Layer targets are missing from the installed package")
endif()
''', encoding='utf-8')
        run('consumer-configure', ['cmake', '-S', str(consumer), '-B', str(work / 'ConsumerBuild'), '-G',
            'Ninja', '-DCMAKE_BUILD_TYPE=Debug', f'-DCMAKE_PREFIX_PATH={relocated.as_posix()}'])
        run('consumer-build', ['cmake', '--build', str(work / 'ConsumerBuild'), '--parallel', str(args.jobs)])
        suffix = '.exe' if sys.platform == 'win32' else ''
        run('consumer-run', [str(work / 'ConsumerBuild' / ('Consumer' + suffix))])
        run('support-only-run', [str(work / 'ConsumerBuild' / ('SupportOnly' + suffix))])
        summary.update(install=True, relocation=True, external_consumer=True,
                       support_without_runtime=True)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
