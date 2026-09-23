#!/usr/bin/env python3
"""Install the pinned Task Scope isolation update. Dry-run unless --apply is supplied."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
TARGETS = frozenset({'Docs/TaskScopeIsolation.md', 'Tests/TaskScopeIsolationTests.cpp', 'Source/Runtime/Private/Dxf/TaskDispatcher.cpp', 'Tools/ScopeTaskValidation/CMakeLists.txt', 'Source/Runtime/Public/Dxf/TaskDispatcher.h', 'Tools/ScopeTaskValidation/Main.cpp'})
TASK_HEADER = 'Source/Runtime/Public/Dxf/TaskDispatcher.h'
MAIN_HEADER = '2c25ef4bffdb9708ca0f824082130256cf82ad18'
SCENE_HEADER = '5608b3bdda80e0a0aa95351fda8294c5a8ea4360'


def normalized(data: bytes) -> bytes:
    """Compare UTF-8 source independent of BOM and Windows line endings."""
    return data.decode('utf-8-sig').replace('\r\n', '\n').encode('utf-8')


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def blob_digest(data: bytes) -> str:
    data = normalized(data)
    return hashlib.sha1(b'blob ' + str(len(data)).encode('ascii') + b'\0' + data).hexdigest()


def safe_path(root: Path, relative: str) -> Path:
    """Reject traversal, symlinks, junctions and non-directory ancestors."""
    rel = PurePosixPath(relative)
    if not relative or ':' in relative or '\\' in relative or rel.is_absolute() or any(p in ('..', '.') for p in rel.parts):
        raise ValueError(f'Unsafe relative path: {relative!r}')
    if rel.as_posix() != relative:
        raise ValueError(f'Non-canonical relative path: {relative!r}')
    current = root
    for index, part in enumerate(rel.parts):
        current = current / part
        if current.is_symlink() or (current.exists() and current.resolve() != current):
            raise ValueError(f'Symlink/junction targets are not supported: {current}')
        if index + 1 < len(rel.parts) and current.exists() and not current.is_dir():
            raise ValueError(f'Expected a directory: {current}')
    if current.exists() and not current.is_file():
        raise ValueError(f'Expected an ordinary file: {current}')
    return current


def git(root: Path, *args: str) -> bytes:
    result = subprocess.run(['git', '-c', 'core.fsmonitor=false', '-C', str(root), *args],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if result.returncode:
        raise ValueError(result.stderr.decode('utf-8', errors='replace').strip() or f'Git exited {result.returncode}')
    return result.stdout


def has_changes(root: Path, relative: str, must_be_tracked: bool) -> bool:
    """Compare content/index, not the stat-cache dirtiness of CRLF worktree files."""
    if must_be_tracked and not git(root, 'ls-files', '-z', '--', relative):
        return True
    return bool(
        git(root, 'diff', '--no-ext-diff', '--no-textconv', '--name-only', '-z', '--', relative)
        or git(root, 'diff', '--cached', '--no-ext-diff', '--no-textconv', '--name-only', '-z', 'HEAD', '--', relative)
        or git(root, 'ls-files', '--others', '--exclude-standard', '-z', '--', relative)
    )


def is_previous_overlay(root: Path, relative: str, before: bytes | None) -> bool:
    """Only the exact previous Scene Task header over an unchanged main index is recognized."""
    if relative != TASK_HEADER or before is None or blob_digest(before) != SCENE_HEADER:
        return False
    if not git(root, 'ls-files', '-z', '--', relative):
        return False
    if git(root, 'diff', '--cached', '--no-ext-diff', '--no-textconv', '--name-only', '-z', 'HEAD', '--', relative):
        return False
    return (blob_digest(git(root, 'show', 'HEAD:' + relative)) == MAIN_HEADER
            and blob_digest(git(root, 'show', ':' + relative)) == MAIN_HEADER)


@dataclass(frozen=True)
class Change:
    relative: str
    before: bytes | None
    after: bytes
    mode: int = 0o644
    previous_overlay: bool = False


def preserve_text_style(data: bytes, before: bytes | None) -> bytes:
    result = normalized(data)
    if before is not None and b'\r\n' in before:
        result = result.replace(b'\n', b'\r\n')
    if before is not None and before.startswith(b'\xef\xbb\xbf'):
        result = b'\xef\xbb\xbf' + result
    return result


def build_plan(root: Path, package: Path = HERE) -> list[Change]:
    root = root.resolve()
    package = package.resolve()
    top = Path(git(root, 'rev-parse', '--show-toplevel').decode('utf-8').strip()).resolve()
    if top != root:
        raise ValueError('--root must point to the repository root, not a subdirectory')
    git(root, 'rev-parse', '--verify', 'HEAD')
    manifest = json.loads((package / 'MANIFEST.json').read_text(encoding='utf-8'))
    entries = manifest['files']
    if manifest['format'] != 2 or set(entries) != TARGETS:
        raise ValueError('Unexpected package manifest or target set')
    # Dependency verification is read-only. No Toolbox files are copied or repaired.
    for rel, metadata in manifest['dependencies'].items():
        path = safe_path(root, rel)
        if not path.is_file() or blob_digest(path.read_bytes()) != metadata['git_blob_sha1']:
            raise ValueError(f'Dependency differs from the tested revision: {rel}')
    plan: list[Change] = []
    selected_profiles: dict[str, str] = {}
    for rel in sorted(TARGETS):
        path = safe_path(root, rel)
        before = path.read_bytes() if path.exists() else None
        options = []
        for metadata in entries[rel]['variants']:
            payload = safe_path(package, metadata['payload_path'])
            data = payload.read_bytes()
            if digest(data) != metadata['payload_sha256']:
                raise ValueError(f'Payload checksum mismatch: {rel}')
            options.append((metadata, data))
        installed = [(meta, data) for meta, data in options
                     if before is not None and normalized(before) == normalized(data)]
        if installed:
            selected_profiles[rel] = installed[0][0]['profile']
            continue
        matched = [(meta, data) for meta, data in options
                   if (before is None and meta['before_git_blob_sha1'] is None)
                   or (before is not None and blob_digest(before) == meta['before_git_blob_sha1'])]
        if len(matched) != 1:
            raise ValueError(f'Target differs from the inspected pre-patch source: {rel}')
        metadata, data = matched[0]
        selected_profiles[rel] = metadata['profile']
        previous_overlay = is_previous_overlay(root, rel, before)
        if has_changes(root, rel, before is not None) and not previous_overlay:
            raise ValueError(f'Overlapping staged/unstaged/untracked change: {rel}')
        mode = stat.S_IMODE(path.stat().st_mode) if before is not None else 0o644
        plan.append(Change(rel, before, preserve_text_style(data, before), mode, previous_overlay))
    return plan


def atomic_write(path: Path, data: bytes, mode: int) -> None:
    """Write a complete sibling temporary file, then replace a single destination."""
    descriptor, temporary = tempfile.mkstemp(prefix='.dxf-scope-', dir=path.parent)
    temp = Path(temporary)
    try:
        with os.fdopen(descriptor, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(temp, mode)
        os.replace(temp, path)
    finally:
        if temp.exists():
            temp.unlink()


def apply_plan(root: Path, plan: list[Change]) -> Path | None:
    root = root.resolve()
    if not plan:
        return None
    # Recheck bytes and index status immediately before the write phase.
    for change in plan:
        path = safe_path(root, change.relative)
        actual = path.read_bytes() if path.exists() else None
        if actual != change.before:
            raise ValueError(f'Target changed after validation: {change.relative}')
        allowed_overlay = change.previous_overlay and is_previous_overlay(root, change.relative, actual)
        if has_changes(root, change.relative, change.before is not None) and not allowed_overlay:
            raise ValueError(f'Working tree/index changed after validation: {change.relative}')
    backup = Path(tempfile.mkdtemp(prefix=root.name + '.scope-isolation-backup-', dir=root.parent))
    backup_records = []
    for change in plan:
        if change.before is not None:
            path = backup / change.relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(change.before)
        backup_records.append({'path': change.relative, 'was_absent': change.before is None,
                               'sha256': digest(change.before) if change.before is not None else None,
                               'mode': change.mode})
    (backup / 'backup.json').write_text(json.dumps({'root': str(root), 'files': backup_records},
                                                indent=2) + '\n', encoding='utf-8')
    written: list[Change] = []
    created_directories: list[Path] = []
    try:
        for change in plan:
            path = safe_path(root, change.relative)
            # Editors/build tools must not modify touched files while this installer runs.
            actual = path.read_bytes() if path.exists() else None
            if actual != change.before:
                raise ValueError(f'Concurrent file modification: {change.relative}')
            allowed_overlay = change.previous_overlay and is_previous_overlay(root, change.relative, actual)
            if has_changes(root, change.relative, change.before is not None) and not allowed_overlay:
                raise ValueError(f'Concurrent index/worktree modification: {change.relative}')
            missing = []
            parent = path.parent
            while not parent.exists():
                missing.append(parent)
                parent = parent.parent
            for directory in reversed(missing):
                directory.mkdir()
                created_directories.append(directory)
            atomic_write(path, change.after, change.mode)
            written.append(change)
    except Exception as error:
        rollback_errors = []
        for change in reversed(written):
            try:
                path = safe_path(root, change.relative)
                # Never overwrite a concurrent edit while rolling back.
                if path.read_bytes() != change.after:
                    raise ValueError(f'Concurrent edit prevents rollback: {change.relative}')
                if change.before is None:
                    path.unlink()
                else:
                    atomic_write(path, change.before, change.mode)
            except Exception as rollback_error:
                rollback_errors.append(str(rollback_error))
        for directory in reversed(created_directories):
            try:
                directory.rmdir()
            except OSError:
                pass
        state = '; '.join(rollback_errors) if rollback_errors else 'Completed-file changes rolled back.'
        raise RuntimeError(f'Apply failed: {error}\n{state}\nBackup: {backup}') from error
    return backup


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    try:
        root = args.root.resolve(strict=True)
        plan = build_plan(root)
        if not plan:
            print('Already applied. No files changed.')
            return 0
        for change in plan:
            label = 'ADD     ' if change.before is None else ('UPGRADE ' if change.previous_overlay else 'REPLACE ')
            print(label + change.relative)
        if not args.apply:
            print(f'Dry-run OK: {len(plan)} files. No files changed. Re-run with --apply to install.')
            return 0
        backup = apply_plan(root, plan)
        print(f'Applied {len(plan)} files. Backup: {backup}')
        print('No git reset, stash, commit, push, or index changes were performed.')
        return 0
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(f'STOP: {error}')
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
