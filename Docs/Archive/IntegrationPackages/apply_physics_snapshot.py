#!/usr/bin/env python3
"""Apply the inspected Physics-only Snapshot edits, without touching Git history/index."""
from __future__ import annotations
import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
import integration

HERE = Path(__file__).resolve().parent

def normal(data: bytes) -> str:
    return data.decode('utf-8-sig').replace('\r\n', '\n')

def blob(text: str) -> str:
    data = text.encode('utf-8')
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()

def encode_like(text: str, original: bytes) -> bytes:
    if b'\r\n' in original:
        text = text.replace('\n', '\r\n')
    return (b'\xef\xbb\xbf' if original.startswith(b'\xef\xbb\xbf') else b'') + text.encode('utf-8')

def git(root: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(['git', '-C', str(root), *args], capture_output=True, check=False)

def safe_path(root: Path, name: str) -> Path:
    relative = Path(name)
    if relative.is_absolute() or '..' in relative.parts or not relative.parts:
        raise ValueError(f'Unsafe relative path: {name}')
    current = root
    for part in relative.parts:
        current = current / part
        if current.is_symlink() or (hasattr(current, 'is_junction') and current.is_junction()):
            raise ValueError(f'Refusing symlink/junction: {current}')
        if current.exists():
            info = current.stat()
            if getattr(info, 'st_file_attributes', 0) & 0x400:
                raise ValueError(f'Refusing reparse point: {current}')
    if not current.resolve().is_relative_to(root.resolve()):
        raise ValueError(f'Path leaves root: {name}')
    return current

def atomic_write(path: Path, data: bytes, mode: int | None = None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp = tempfile.mkstemp(prefix='.dxf-snapshot-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        if mode is not None:
            os.chmod(temp, mode)
        os.replace(temp, path)
    finally:
        if os.path.exists(temp):
            os.unlink(temp)

def plan(root: Path) -> list[tuple[str, bytes | None, bytes]]:
    manifest = json.loads((HERE / 'MANIFEST.json').read_text(encoding='utf-8'))
    for name, digest in manifest['package_sha256'].items():
        p = safe_path(HERE, name)
        if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest() != digest:
            raise ValueError(f'Package file missing or corrupt: {name}')
    result = []
    states = []
    for name, expected in integration.BASE_BLOBS.items():
        p = safe_path(root, name)
        before = p.read_bytes()
        text = normal(before)
        if blob(text) == expected:
            after = encode_like(integration.forward(name, text), before)
            states.append('base')
        else:
            try:
                base = integration.reverse(name, text)
            except ValueError as error:
                raise ValueError(f'Unknown edited source; nothing was written: {name}') from error
            if blob(base) != expected:
                raise ValueError(f'Unknown source revision; nothing was written: {name}')
            after = before
            states.append('applied')
        result.append((name, before, after))
    if len(set(states)) != 1:
        raise ValueError('The four Physics source files are partially updated. Restore a consistent set first.')
    for name in manifest['payload_files']:
        source = safe_path(HERE / 'Payload', name)
        p = safe_path(root, name)
        after = source.read_bytes()
        before = p.read_bytes() if p.is_file() else None
        if before is not None:
            if normal(before) != normal(after):
                raise ValueError(f'New file conflicts with existing content: {name}')
            after = before
        elif p.exists():
            raise ValueError(f'Expected a file: {name}')
        result.append((name, before, after))
    return result

def apply(root: Path, write: bool) -> dict:
    root = root.absolute()
    if root.is_symlink() or (hasattr(root, 'is_junction') and root.is_junction()):
        raise ValueError('Root must not be a symlink/junction')
    root = root.resolve(strict=True)
    top = git(root, 'rev-parse', '--show-toplevel')
    if top.returncode != 0 or Path(os.fsdecode(top.stdout).strip()).resolve() != root:
        raise ValueError('--root must be the Git repository root')
    changes = [(n, old, new) for n, old, new in plan(root) if old != new]
    if not changes:
        return {'status': 'already-applied', 'files': 0}
    for name, before, _ in changes:
        if git(root, 'diff', '--cached', '--quiet', '--', name).returncode != 0:
            raise ValueError(f'Staged changes overlap: {name}')
        tracked = git(root, 'ls-files', '--error-unmatch', '--', name).returncode == 0
        if tracked and git(root, 'diff', '--quiet', '--', name).returncode != 0:
            raise ValueError(f'Uncommitted changes overlap: {name}')
        if before is not None and not tracked:
            raise ValueError(f'Untracked file overlaps: {name}')
    report = {'status': 'checked', 'files': len(changes), 'paths': [n for n, _, _ in changes]}
    if not write:
        return report
    backup = Path(tempfile.mkdtemp(prefix=root.name + '-snapshot-backup-', dir=root.parent))
    records = []
    for name, before, after in changes:
        path = safe_path(root, name)
        mode = stat.S_IMODE(path.stat().st_mode) if before is not None else None
        records.append((name, before, after, mode))
        if before is not None:
            saved = backup / 'Before' / name
            saved.parent.mkdir(parents=True, exist_ok=True)
            saved.write_bytes(before)
    (backup / 'changes.json').write_text(json.dumps({
        'created_utc': datetime.now(timezone.utc).isoformat(),
        'root': str(root),
        'new_files': [n for n, old, _, _ in records if old is None],
        'paths': [n for n, _, _, _ in records],
    }, indent=2), encoding='utf-8')
    diff = []
    for name, before, after, _ in records:
        diff.extend(difflib.unified_diff(normal(before or b'').splitlines(keepends=True),
            normal(after).splitlines(keepends=True),
            fromfile='a/' + name if before is not None else '/dev/null', tofile='b/' + name))
    (backup / 'applied.diff').write_text(''.join(diff), encoding='utf-8')
    written = []
    try:
        # Verify the entire planned input once more immediately before writing.
        for name, before, _, _ in records:
            p = safe_path(root, name)
            if (p.read_bytes() if p.is_file() else None) != before:
                raise ValueError(f'Concurrent change before write: {name}')
        for name, before, after, mode in records:
            p = safe_path(root, name)
            if (p.read_bytes() if p.is_file() else None) != before:
                raise ValueError(f'Concurrent change during write: {name}')
            atomic_write(p, after, mode)
            written.append((name, before, after, mode))
        for name, _, after, _ in records:
            if safe_path(root, name).read_bytes() != after:
                raise ValueError(f'Post-write mismatch: {name}')
    except BaseException as error:
        conflicts = []
        for name, before, after, mode in reversed(written):
            p = safe_path(root, name)
            if not p.is_file() or p.read_bytes() != after:
                conflicts.append(name)
                continue
            if before is None:
                p.unlink()
            else:
                atomic_write(p, before, mode)
        raise RuntimeError(f'Apply failed: {error}; backup={backup}; concurrent files preserved={conflicts}') from error
    report.update(status='applied', backup=str(backup))
    return report

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--apply', action='store_true', help='Write after validation; otherwise read-only')
    args = parser.parse_args()
    try:
        print(json.dumps(apply(args.root, args.apply), ensure_ascii=False, indent=2))
        return 0
    except (OSError, ValueError, RuntimeError, UnicodeError) as error:
        print(f'STOP: {error}', file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
