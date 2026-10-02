"""Fresh Application processes resume a logical checkpoint and independent settings.

Only newly allocated output directories hold the relocated executable, Assets and saves.
The graphics/file boundaries are doubles; this CPU check is not native pixel evidence.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil

from ValidationSupport import new_run_directory, run_logged, validation_report


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(probe: Path, source: Path, logs: Path) -> None:
    original = {path.relative_to(source).as_posix(): digest(path)
                for path in (source / 'Assets').rglob('*') if path.is_file()}
    with validation_report(logs, {'scope': 'CPU fresh processes; fixed input; real Application/World/Jobs; backend doubles'}, root=source):
        project = logs / '再配置 ゲーム'
        assets = project / 'Assets'
        assets.mkdir(parents=True)
        shutil.copytree(source / 'Assets/SaveCourse', assets / 'SaveCourse')
        shutil.copytree(source / 'Assets/Models', assets / 'Models')
        for name in ('player.bmp', 'confirm.wav'):
            shutil.copyfile(source / 'Assets' / name, assets / name)
        relocated = project / probe.name
        shutil.copyfile(probe, relocated)
        sha = digest(probe)
        if digest(relocated) != sha:
            raise RuntimeError('Relocated executable differs before execution')
        unrelated = logs / '無関係 作業場所'
        unrelated.mkdir()
        for dimension in (2, 3):
            save_root = logs / f'保存 専用 {dimension}D'
            save_root.mkdir()
            for mode in ('write', 'read'):
                output = run_logged(logs, f'{dimension}D-{mode}', [str(relocated), mode, str(dimension), str(save_root), str(project)], cwd=unrelated, timeout=45)
                expected = f'SAVE_COURSE_PROCESS_PASSED dimension={dimension} mode={mode}'
                if expected not in output:
                    raise RuntimeError(f'Process exited without independent expected-value checks: {expected}')
                if digest(probe) != sha or digest(relocated) != sha:
                    raise RuntimeError('Executable bytes changed between save and resume processes')
        after = {path.relative_to(source).as_posix(): digest(path)
                 for path in (source / 'Assets').rglob('*') if path.is_file()}
        if original != after:
            raise RuntimeError('Original Assets changed during save test')
        (logs / 'ProcessEvidence.json').write_text(json.dumps({'exe_sha256': sha, 'relocated_exe': str(relocated), 'project_root': str(project), 'unrelated_cwd': str(unrelated), 'original_assets_unchanged': True, 'processes': ['2D-write', '2D-read', '3D-write', '3D-read'], 'graphics': 'CPU backend doubles; no native pixel claim'}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--logs', type=Path, required=True, help='Parent for a new run; previous output is never reused')
    args = parser.parse_args()
    verify(args.probe.resolve(strict=True), args.source.resolve(strict=True), new_run_directory(args.logs.resolve()))


if __name__ == '__main__':
    main()
