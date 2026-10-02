"""Run fixed-input save/resume as separate processes with real DxLib readback.

Only a fresh output directory contains copied Assets, executable and test SaveRoots.
This does not test human input, hearing, SDK-free machines or physical power loss.
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
    original = {p.relative_to(source).as_posix(): digest(p)
                for p in (source / 'Assets').rglob('*') if p.is_file()}
    with validation_report(logs, {'scope': 'Native fixed-input fresh processes, real Application/World/DxLib pixels'}, root=source):
        project = logs / '再配置 ゲーム'
        project.mkdir()
        shutil.copytree(source / 'Assets', project / 'Assets')
        relocated = project / probe.name
        shutil.copyfile(probe, relocated)
        sha = digest(probe)
        if digest(relocated) != sha:
            raise RuntimeError('Relocated native executable differs')
        unrelated = logs / '無関係 作業場所'
        unrelated.mkdir()
        for dimension in (2, 3):
            save_root = logs / f'保存 専用 {dimension}D'
            save_root.mkdir()
            for mode in ('write', 'read'):
                images = logs / f'{dimension}D-{mode}-画像'
                images.mkdir()
                output = run_logged(logs, f'{dimension}D-{mode}', [str(relocated), str(project), str(images), f'--save-{mode}', str(dimension), str(save_root)], cwd=unrelated, timeout=75)
                expected = f'SAVE_COURSE_NATIVE_PASSED dimension={dimension} mode={mode}'
                if expected not in output:
                    raise RuntimeError('Native process exited without independent value/pixel checks: ' + expected)
                phases = ('before', 'saved', 'changed') if mode == 'write' else ('continued',)
                required = {f'{phase}-{layout}.png' for phase in phases for layout in ('single', 'split', 'odd')}
                actual = {path.name for path in images.glob('*.png') if path.stat().st_size > 0}
                if actual != required:
                    raise RuntimeError('Incomplete native readback images: ' + ', '.join(sorted(required - actual)))
                if digest(probe) != sha or digest(relocated) != sha:
                    raise RuntimeError('Native executable changed between processes')
        after = {p.relative_to(source).as_posix(): digest(p)
                 for p in (source / 'Assets').rglob('*') if p.is_file()}
        if original != after:
            raise RuntimeError('Original Assets changed during native save test')
        (logs / 'ProcessEvidence.json').write_text(json.dumps({'exe_sha256': sha, 'relocated_exe': str(relocated), 'project_root': str(project), 'unrelated_cwd': str(unrelated), 'original_assets_unchanged': True, 'processes': ['2D-write', '2D-read', '3D-write', '3D-read'], 'readback_images': 24, 'graphics': 'Real DxLib one-shot readback per image; fixed input; no human operation claim'}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--logs', type=Path, required=True, help='Parent of a new run; previous output is never reused')
    args = parser.parse_args()
    verify(args.probe.resolve(strict=True), args.source.resolve(strict=True), new_run_directory(args.logs.resolve()))


if __name__ == '__main__':
    main()
