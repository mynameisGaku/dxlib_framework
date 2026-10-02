"""Run the installed consumer's checkpoint modes as four separate processes.

The C++ consumer checks real Scene/World state and remapped actions. Native mode
also checks object and Texture/Model pixels before emitting each stage marker.
"""
from __future__ import annotations

import hashlib
from pathlib import Path

from ValidationSupport import run_logged


def run_processes(executable: Path, project: Path, save_parent: Path, cwd: Path,
                  logs: Path, prefix: str, *, native: bool = False) -> dict:
    sha = hashlib.sha256(executable.read_bytes()).hexdigest()
    roots = []
    images = []
    for dimension in (2, 3):
        save_root = save_parent / f'専用保存 {dimension}D'
        save_root.mkdir(parents=True, exist_ok=False)
        roots.append(str(save_root.resolve()))
        for mode in ('write', 'read'):
            output = run_logged(logs, f'{prefix}-{dimension}D-{mode}',
                                [str(executable.resolve()), mode, str(dimension),
                                 str(project.resolve()), str(save_root.resolve())],
                                cwd=cwd, timeout=240)
            if f'CHECKPOINT_PROCESS_PASSED {mode} dimension={dimension}' not in output:
                raise RuntimeError('Checkpoint consumer did not complete independent checks')
            if hashlib.sha256(executable.read_bytes()).hexdigest() != sha:
                raise RuntimeError('Checkpoint executable changed between processes')
            if native:
                stages = ('before', 'saved', 'changed') if mode == 'write' else ('continued',)
                for stage in stages:
                    if f'EXTERNAL_SAVE_PIXEL stage={stage}' not in output or f'EXTERNAL_SAVE_ASSET stage={stage}' not in output:
                        raise RuntimeError('Native checkpoint stage lacks object or asset pixel checks')
                    for view in ('single', 'split', 'odd'):
                        image = save_root / f'{stage}-{view}.png'
                        if not image.is_file() or image.stat().st_size == 0:
                            raise RuntimeError('Native checkpoint expected image missing')
                        images.append(str(image.resolve()))
    return {'exe_sha256': sha, 'save_roots': roots, 'processes': 4, 'images': images,
            'cwd': str(cwd.resolve()), 'project_root': str(project.resolve()),
            'scope': 'real Application/DxLib pixels' if native else 'real Navigator/Scene/World; asset backend doubles'}
