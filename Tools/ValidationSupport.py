"""One-run validation records and subprocess logs shared by portable validators."""
from __future__ import annotations
from contextlib import contextmanager
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import subprocess
from typing import Iterator
from uuid import uuid4


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


@contextmanager
def validation_report(logs: Path, summary: dict[str, object]) -> Iterator[dict[str, object]]:
    """Clear stale success before work; leave running, failed or passed explicitly."""
    logs.mkdir(parents=True, exist_ok=True)
    path = logs / 'Summary.json'
    summary.update(status='running', run_id=uuid4().hex, started_utc=timestamp())
    write_summary(path, summary)
    try:
        yield summary
    except BaseException as error:
        summary.update(status='failed', error=f'{type(error).__name__}: {error}')
        raise
    else:
        summary['status'] = 'passed'
    finally:
        summary['finished_utc'] = timestamp()
        write_summary(path, summary)


def run_logged(logs: Path, name: str, command: list[str], *, cwd: Path,
               environment: dict[str, str] | None = None, timeout: int = 180) -> str:
    log = logs / (name + '.log')
    heading = '$ ' + subprocess.list2cmdline(command) + '\n'
    # Write before process launch: even missing executables have a current log.
    log.write_text(heading + 'STATUS=RUNNING\n', encoding='utf-8')
    try:
        result = subprocess.run(command, cwd=cwd, env=environment, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                encoding='utf-8', errors='replace', timeout=timeout)
    except subprocess.TimeoutExpired as error:
        output = error.stdout or ''
        if isinstance(output, bytes):
            output = output.decode('utf-8', errors='replace')
        log.write_text(heading + output + f'\nEXIT_CODE=TIMEOUT\nTIMEOUT_SECONDS={timeout}\n', encoding='utf-8')
        raise
    except OSError as error:
        log.write_text(heading + str(error) + '\nEXIT_CODE=NOT_STARTED\n', encoding='utf-8')
        raise
    log.write_text(heading + result.stdout + f'\nEXIT_CODE={result.returncode}\n', encoding='utf-8')
    print(f"{name}: {'PASS' if result.returncode == 0 else 'FAIL'}", flush=True)
    if result.returncode:
        raise RuntimeError(f'{name} failed; see {log}')
    return result.stdout
