"""Write the real-byte source manifest of this checkout to a new file and print its SHA-256.

The digest covers relative paths and current file bytes (the working tree), not the Git index.
HEAD and the Git status are printed separately. An existing output file is never replaced.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import sys
from ValidationSupport import _git, source_manifest

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path, help='new manifest file (must not exist)')
    args = parser.parse_args()
    digest, entries = source_manifest(ROOT)
    with args.output.open('x', encoding='utf-8') as handle:
        handle.write(f'# source_sha256 {digest}\n# git_head {_git(ROOT, "rev-parse", "HEAD")}\n')
        for line in _git(ROOT, 'status', '--porcelain=v1', '--untracked-files=all').splitlines():
            handle.write(f'# git_status {line}\n')
        handle.writelines(f'{sha}  {path}\n' for path, sha in entries)
    print(digest)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except OSError as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
