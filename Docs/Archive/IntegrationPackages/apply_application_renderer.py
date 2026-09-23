#!/usr/bin/env python3
"""Connect the real Application to FRenderSystem. Dry-run unless --apply is supplied."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

BASE = '4868dc9811d60e227aa0d7c4d2c18410388d3d63'
HERE = Path(__file__).resolve().parent
CPP_SUFFIXES = {'.h', '.hpp', '.cpp', '.cc', '.cxx', '.inl'}
OLD_FILES = (
    'Source/DxLibSupport/Public/Dxf/RenderSystem2D.h',
    'Source/DxLibSupport/Private/Dxf/RenderSystem2D.cpp',
)


def run_git(root: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(['git', '-C', str(root), *args], capture_output=True, check=False)


def read_text(path: Path) -> str:
    return path.read_bytes().decode('utf-8-sig').replace('\r\n', '\n')


def migrate_type(text: str) -> str:
    return re.sub(r'\bFRenderSystem2D\b', 'FRenderSystem', text).replace('RenderSystem2D.h', 'RenderSystem.h')


def connect_application(text: str) -> str:
    if 'm_Renderer.SetExecutionJobs(m_ExecutionJobs)' in text:
        return text
    anchor = 'm_Scenes(m_Assets, m_Audio, m_pGame.Get()), m_Clock(m_Settings.MaxDeltaSeconds)\n{\n}'
    if text.count(anchor) != 1:
        raise ValueError('Application constructor differs from the inspected implementation; no changes written.')
    replacement = '''m_Scenes(m_Assets, m_Audio, m_pGame.Get()), m_Clock(m_Settings.MaxDeltaSeconds)
{
	// 全メンバー構築後に共有実行器を結び付け、Scene側へ所有権を渡さない。
	auto Connected = m_Renderer.SetExecutionJobs(m_ExecutionJobs);
	if (!Connected)
	{
		throw Toolbox::FException(Connected.Error().Message);
	}
}'''
    return text.replace(anchor, replacement)


def connect_cmake(text: str) -> str:
    old = 'Source/DxLibSupport/Private/Dxf/RenderSystem2D.cpp'
    new = 'Source/DxLibSupport/Private/Dxf/RenderSystem.cpp'
    if old in text and new in text:
        raise ValueError('Both Renderer implementations are registered; resolve the duplicate explicitly.')
    if text.count(old) == 1:
        text = text.replace(old, new)
    elif text.count(new) != 1:
        raise ValueError('Expected exactly one renderer source in the root CMake target.')
    extra = 'Source/DxLibSupport/Private/Dxf/RenderPass3D.cpp'
    if extra not in text:
        anchor = '    Source/DxLibSupport/Private/Dxf/Render3DContext.cpp'
        if text.count(anchor) != 1:
            raise ValueError('Cannot locate the real support library source list.')
        text = text.replace(anchor, anchor + '\n    ' + extra)
    entry = '    dxf_add_application_render_integration_tests(dxf::framework)'
    if entry not in text:
        anchor = '    add_test(NAME Framework COMMAND dxf_tests)'
        if text.count(anchor) != 1:
            raise ValueError('Cannot locate the full repository test registration.')
        text = text.replace(anchor, anchor + '\n\n    include(CMake/ApplicationRenderIntegrationTests.cmake)\n' + entry)
    return text


def build_plan(root: Path) -> dict[str, bytes | None]:
    required = (
        'CMakeLists.txt', 'Source/Runtime/Private/Dxf/Application.cpp',
        'Source/Runtime/Public/Dxf/Application.h',
        'Source/DxLibSupport/Public/Dxf/RenderSystem.h',
        'Source/DxLibSupport/Private/Dxf/RenderSystem.cpp',
        'Source/DxLibSupport/Private/Dxf/RenderPass3D.cpp',
        'Source/DxLibSupport/Private/Dxf/RenderPass3D.h',
        'Tests/TestMain.cpp', 'Tests/Support/FakeBackend.h',
    )
    for rel in required:
        if not (root / rel).is_file():
            raise ValueError(f'Required repository file is missing: {rel}')
    plan: dict[str, bytes | None] = {}
    for directory in ('Source', 'Examples', 'Tests'):
        for path in sorted((root / directory).rglob('*')):
            if not path.is_file() or path.suffix not in CPP_SUFFIXES:
                continue
            rel = path.relative_to(root).as_posix()
            if rel in OLD_FILES:
                continue
            text = read_text(path)
            modified = migrate_type(text)
            if rel == 'Source/Runtime/Private/Dxf/Application.cpp':
                modified = connect_application(modified)
            if text != modified:
                plan[rel] = modified.encode('utf-8')
    cmake = read_text(root / 'CMakeLists.txt')
    result = connect_cmake(cmake)
    if result != cmake:
        plan['CMakeLists.txt'] = result.encode('utf-8')
    for rel in OLD_FILES:
        if (root / rel).is_file():
            plan[rel] = None
    for path in sorted((HERE / 'Payload').rglob('*')):
        if not path.is_file():
            continue
        rel = path.relative_to(HERE / 'Payload').as_posix()
        data = path.read_bytes()
        if not (root / rel).exists() or read_text(root / rel) != data.decode('utf-8-sig').replace('\r\n', '\n'):
            plan[rel] = data
    # Keep current API/architecture text in step; historical validation reports are not edited.
    for rel in ('Docs/API.md', 'Docs/Architecture.md'):
        path = root / rel
        if path.is_file():
            text = read_text(path)
            changed = migrate_type(text)
            if changed != text:
                plan[rel] = changed.encode('utf-8')
    return plan


def apply_plan(root: Path, plan: dict[str, bytes | None]) -> Path | None:
    if not plan:
        return None
    before = {rel: (root / rel).read_bytes() if (root / rel).exists() else None for rel in plan}
    backup = Path(tempfile.mkdtemp(prefix='dxf-application-renderer-backup-', dir=root.parent))
    for rel, data in before.items():
        if data is not None:
            path = backup / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    (backup / 'manifest.json').write_text(json.dumps({
        'base': BASE,
        'root': str(root),
        'files': {rel: {'before_sha256': hashlib.sha256(data).hexdigest() if data is not None else None,
                         'after_sha256': hashlib.sha256(plan[rel]).hexdigest() if plan[rel] is not None else None}
                  for rel, data in before.items()},
    }, indent=2), encoding='utf-8')
    written: list[str] = []
    try:
        for rel, data in plan.items():
            target = root / rel
            current = target.read_bytes() if target.exists() else None
            if current != before[rel]:
                raise RuntimeError(f'Concurrent modification before write: {rel}')
            if data is None:
                target.unlink()
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                fd, name = tempfile.mkstemp(prefix='.dxf-write-', dir=target.parent)
                try:
                    with os.fdopen(fd, 'wb') as stream:
                        stream.write(data)
                    os.replace(name, target)
                finally:
                    if os.path.exists(name):
                        os.unlink(name)
            written.append(rel)
    except BaseException:
        for rel in reversed(written):
            target = root / rel
            current = target.read_bytes() if target.exists() else None
            if current != plan[rel]:
                print(f'Concurrent edit preserved; original backup: {backup / rel}')
                continue
            if before[rel] is None:
                target.unlink(missing_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(before[rel])
        raise
    return backup


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    root = args.root.resolve()
    head = run_git(root, 'rev-parse', 'HEAD')
    if head.returncode != 0:
        raise ValueError('Run this tool against the real Git repository, not a ValidationSource folder.')
    ancestry = run_git(root, 'merge-base', '--is-ancestor', BASE, 'HEAD')
    if ancestry.returncode != 0:
        raise ValueError(f'HEAD does not contain {BASE}; do not reset or force this patch.')
    plan = build_plan(root)
    for rel, data in plan.items():
        path = root / rel
        if root not in path.resolve().parents:
            raise ValueError(f'Path escapes repository: {rel}')
        if path.is_symlink() or any(parent.is_symlink() for parent in path.parents if parent != root and root in parent.parents):
            raise ValueError(f'Symlink target is not supported: {rel}')
        cached = run_git(root, 'diff', '--cached', '--quiet', '--', rel)
        if cached.returncode != 0:
            raise ValueError(f'Staged changes overlap: {rel}; save/review them first.')
        previous = run_git(root, 'show', f'HEAD:{rel}')
        if path.exists():
            current = read_text(path)
            if previous.returncode == 0:
                original = previous.stdout.decode('utf-8-sig').replace('\r\n', '\n')
                if current != original and (data is None or current != data.decode('utf-8-sig')):
                    raise ValueError(f'Uncommitted changes overlap: {rel}; no files changed.')
            elif data is None or current != data.decode('utf-8-sig'):
                raise ValueError(f'An untracked file conflicts with the update: {rel}')
        print(('DELETE ' if data is None else 'UPDATE ') + rel)
    if not args.apply:
        print(f'CHECK PASSED: {len(plan)} planned changes. Nothing was written; use --apply to apply.')
        return 0
    backup = apply_plan(root, plan)
    print(f'APPLIED: {len(plan)} changes; backup={backup}. No build, commit or push was performed.')
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, RuntimeError, OSError) as error:
        print(f'STOPPED: {error}')
        raise SystemExit(1)
