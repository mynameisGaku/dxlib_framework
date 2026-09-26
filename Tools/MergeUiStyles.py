"""Deterministically bundle component .dxfui files; runtime parser validates semantics.

The output is optional at run time: built-in typed defaults work without this tool.
Only the declared version header is removed. C++ code, comments and expressions
are never evaluated. Embedded source markers preserve file and line diagnostics.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import tempfile


def merge_styles(root: Path, output: Path, max_bytes: int = 1024 * 1024) -> tuple[bytes, int]:
    root = root.resolve(strict=True)
    output = output.resolve()
    if not root.is_dir() or max_bytes <= 0:
        raise ValueError("a directory and positive byte limit are required")
    paths = sorted((p for p in root.rglob('*.dxfui') if p.resolve() != output),
                   key=lambda p: p.relative_to(root).as_posix())
    if not paths:
        raise ValueError("no component .dxfui files")
    parts = ['dxfui-style 1\n']
    total = 0
    for path in paths:
        if path.is_symlink() or not path.resolve().is_relative_to(root):
            raise ValueError(f"source outside root or symlink: {path}")
        name = path.relative_to(root).as_posix()
        if any(c in name for c in '\"\r\n'):
            raise ValueError(f"invalid source name: {name!r}")
        size = path.stat().st_size
        if size > max_bytes or total + size > max_bytes:
            raise ValueError(f"style source limit exceeded: {name}")
        data = path.read_bytes()
        if len(data) != size:
            raise ValueError(f"source changed during read: {name}")
        total += len(data)
        text = data.decode('utf-8-sig').replace('\r\n', '\n')
        if '\x00' in text or '\r' in text:
            raise ValueError(f"NUL or unsupported line ending: {name}")
        lines = text.splitlines()
        header = next((i for i, line in enumerate(lines)
                       if line.strip() and not line.lstrip().startswith('#')), None)
        if header is None or lines[header].strip() != 'dxfui-style 1':
            raise ValueError(f"{name}: missing or unsupported dxfui-style 1 header")
        # Preserve the header's line as a blank so original line numbers survive.
        lines[header] = ''
        if any(line.lstrip().startswith('source ') for line in lines):
            raise ValueError(f"{name}: source markers are reserved for generated bundles")
        parts.append(f'source "{name}"\n' + '\n'.join(lines) + '\n')
    result = ''.join(parts).replace('\n', '\r\n').encode('utf-8')
    if len(result) > max_bytes:
        raise ValueError("generated bundle exceeds runtime source limit")
    return result, len(paths)


def write_bundle(root: Path, output: Path, max_bytes: int = 1024 * 1024) -> int:
    data, count = merge_styles(root, output, max_bytes)
    output.parent.mkdir(parents=True, exist_ok=True)
    descriptor, name = tempfile.mkstemp(prefix='ui-style-', suffix='.tmp', dir=output.parent)
    try:
        with os.fdopen(descriptor, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, output)
    finally:
        Path(name).unlink(missing_ok=True)
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--max-bytes', type=int, default=1024 * 1024)
    args = parser.parse_args()
    try:
        count = write_bundle(args.root, args.output, args.max_bytes)
    except (OSError, ValueError) as error:
        parser.exit(1, f'UI style bundle failed: {error}\n')
    print(f'{count} source(s) -> {args.output}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
