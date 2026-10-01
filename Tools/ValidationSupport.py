"""One-run validation records and subprocess logs shared by portable validators.

Every run owns its log directory. A validator either creates a new run directory under a
parent (``new_run_directory``) or uses an explicit directory that must be missing or empty;
an existing, non-empty directory is rejected before any child process starts, so earlier
logs and ``Summary.json`` are never truncated or replaced.
"""
from __future__ import annotations
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from typing import Iterator
from uuid import uuid4

# Directories hashed as the "actual source" of a run (real bytes, not the Git index).
FINGERPRINT_DIRECTORIES = ('Source', 'Tests', 'Examples', 'Tools', 'CMake', 'External', 'Assets')
FINGERPRINT_ROOT_FILES = ('CMakeLists.txt', '.clang-format', 'GenerateProjectFiles.bat', 'BuildWindows.ps1')
FINGERPRINT_EXCLUDED_PARTS = {'__pycache__', '.pytest_cache'}
# Structured check lines printed by device programs: "DXF_CHECK <name>=<verified|not_exercised|failed>".
# "ctest -V" prefixes each line of a test's output with "<test number>: ", which is accepted.
CHECK_LINE = re.compile(r'^(?:\d+: )?DXF_CHECK ([A-Za-z0-9_.-]+)=(verified|not_exercised|failed)\s*$', re.MULTILINE)

# Summary of each open run, keyed by its resolved log directory (run_logged appends its steps there).
_ACTIVE: dict[Path, tuple[Path, dict[str, object]]] = {}


class LogsInUseError(RuntimeError):
    """The requested log directory already holds another run's files."""


def project_version(root: Path) -> str:
    text = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
    match = re.search(r'project\(\s*dxlib_framework\s+VERSION\s+(\d+\.\d+\.\d+)\b', text, re.IGNORECASE)
    if not match:
        raise RuntimeError('Cannot find dxlib_framework version in CMakeLists.txt')
    return match.group(1)


def timestamp() -> str:
    return datetime.now(timezone.utc).isoformat()


def write_summary(path: Path, summary: dict[str, object]) -> None:
    temporary = path.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temporary.replace(path)


def new_run_directory(parent: Path) -> Path:
    """Create and return a new, empty run directory below parent (never reuses an existing name)."""
    parent.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    for _ in range(100):
        candidate = parent / f'{stamp}-{uuid4().hex[:8]}'
        try:
            candidate.mkdir()
        except FileExistsError:
            continue
        return candidate
    raise LogsInUseError(f'Cannot allocate a new run directory below {parent}')


def check_new_logs(logs: Path) -> None:
    """Reject an explicit log directory that already has files (before any process is started)."""
    if logs.exists():
        if not logs.is_dir():
            raise LogsInUseError(f'Log path is not a directory: {logs}')
        if any(logs.iterdir()):
            raise LogsInUseError(f'Log directory already holds another run; choose a new directory: {logs}')


def source_manifest(root: Path) -> tuple[str, list[tuple[str, str]]]:
    """SHA-256 over relative paths and the current file bytes (the working tree, not the Git index)."""
    files: list[Path] = []
    for name in FINGERPRINT_DIRECTORIES:
        base = root / name
        if base.is_dir():
            files.extend(path for path in base.rglob('*')
                         if path.is_file() and not FINGERPRINT_EXCLUDED_PARTS.intersection(path.parts))
    files.extend(root / name for name in FINGERPRINT_ROOT_FILES if (root / name).is_file())
    entries = sorted((path.relative_to(root).as_posix(), hashlib.sha256(path.read_bytes()).hexdigest())
                     for path in files)
    digest = hashlib.sha256()
    for relative, sha in entries:
        digest.update(relative.encode('utf-8') + b'\0' + sha.encode('ascii') + b'\n')
    return digest.hexdigest(), entries


def _git(root: Path, *arguments: str) -> str:
    # Popen directly: validators replace subprocess.run in tests, and Git state is informational only.
    try:
        process = subprocess.Popen(['git', *arguments], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                   text=True, encoding='utf-8', errors='replace')
        output, _ = process.communicate(timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return ''
    # Keep leading spaces: "git status --porcelain" uses them as the index column.
    return output.rstrip() if process.returncode == 0 else ''


def record_source(root: Path, logs: Path, summary: dict[str, object]) -> None:
    """Record HEAD, index/worktree state and the real-byte manifest of this run."""
    digest, entries = source_manifest(root)
    (logs / 'SourceManifest.txt').write_text(''.join(f'{sha}  {path}\n' for path, sha in entries), encoding='utf-8')
    status = _git(root, 'status', '--porcelain=v1', '--untracked-files=all')
    summary.update(source_sha256=digest, source_files=len(entries), git_head=_git(root, 'rev-parse', 'HEAD'),
                   git_status=status.splitlines(), git_clean=not status)


@contextmanager
def validation_report(logs: Path, summary: dict[str, object], *, root: Path | None = None) -> Iterator[dict[str, object]]:
    """Claim a new run directory, then leave running, failed, interrupted or passed explicitly."""
    check_new_logs(logs)
    logs.mkdir(parents=True, exist_ok=True)
    path = logs / 'Summary.json'
    summary.update(status='running', run_id=uuid4().hex, started_utc=timestamp(), logs=str(logs.resolve()),
                   cwd=os.getcwd(), steps=[], checks=[])
    # Exclusive creation: a concurrent run that claimed the same directory makes this fail.
    try:
        with path.open('x', encoding='utf-8') as claim:
            claim.write(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
    except FileExistsError as error:
        raise LogsInUseError(f'Log directory was claimed by another run: {logs}') from error
    key = logs.resolve()
    _ACTIVE[key] = (path, summary)
    try:
        if root is not None:
            record_source(root, logs, summary)
            write_summary(path, summary)
        yield summary
        if root is not None:
            final_digest, entries = source_manifest(root)
            summary['final_source_sha256'] = final_digest
            summary['source_changed'] = final_digest != summary['source_sha256']
            (logs / 'FinalSourceManifest.txt').write_text(''.join(f'{sha}  {path}\n' for path, sha in entries), encoding='utf-8')
            if summary['source_changed']:
                raise RuntimeError('Source manifest changed during validation; this run is not a coherent final result')
    except KeyboardInterrupt:
        summary.update(status='interrupted', error='KeyboardInterrupt')
        raise
    except BaseException as error:
        summary.update(status='failed', error=f'{type(error).__name__}: {error}')
        raise
    else:
        summary['status'] = 'passed'
    finally:
        summary['finished_utc'] = timestamp()
        _ACTIVE.pop(key, None)
        write_summary(path, summary)


def _record_step(logs: Path, step: dict[str, object], output: str) -> None:
    active = _ACTIVE.get(logs.resolve())
    if active is None:
        return
    path, summary = active
    steps = summary.setdefault('steps', [])
    if step not in steps:
        steps.append(step)
    for name, state in CHECK_LINE.findall(output):
        summary.setdefault('checks', []).append({'step': step['name'], 'name': name, 'state': state})
    write_summary(path, summary)


def run_logged(logs: Path, name: str, command: list[str], *, cwd: Path,
               environment: dict[str, str] | None = None, timeout: int = 180) -> str:
    log = logs / (name + '.log')
    heading = '$ ' + subprocess.list2cmdline(command) + '\n'
    step: dict[str, object] = {'name': name, 'command': command, 'cwd': str(cwd), 'log': str(log),
                               'started_utc': timestamp(), 'exit': 'RUNNING'}
    # Exclusive creation before process launch: a repeated step name never truncates the earlier log,
    # and even missing executables leave a current log.
    try:
        with log.open('x', encoding='utf-8') as handle:
            handle.write(heading + 'STATUS=RUNNING\n')
    except FileExistsError as error:
        raise LogsInUseError(f'Step log already exists (repeat the step under a new name): {log}') from error
    _record_step(logs, step, '')
    try:
        result = subprocess.run(command, cwd=cwd, env=environment, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                encoding='utf-8', errors='replace', timeout=timeout)
    except subprocess.TimeoutExpired as error:
        output = error.stdout or ''
        if isinstance(output, bytes):
            output = output.decode('utf-8', errors='replace')
        log.write_text(heading + output + f'\nEXIT_CODE=TIMEOUT\nTIMEOUT_SECONDS={timeout}\n', encoding='utf-8')
        step.update(exit='TIMEOUT', finished_utc=timestamp())
        _record_step(logs, step, output)
        raise
    except OSError as error:
        log.write_text(heading + str(error) + '\nEXIT_CODE=NOT_STARTED\n', encoding='utf-8')
        step.update(exit='NOT_STARTED', finished_utc=timestamp())
        _record_step(logs, step, '')
        raise
    log.write_text(heading + result.stdout + f'\nEXIT_CODE={result.returncode}\n', encoding='utf-8')
    step.update(exit=result.returncode, finished_utc=timestamp())
    _record_step(logs, step, result.stdout)
    print(f"{name}: {'PASS' if result.returncode == 0 else 'FAIL'}", flush=True)
    if result.returncode:
        raise RuntimeError(f'{name} failed; see {log}')
    return result.stdout
