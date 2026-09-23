"""Create a reproducible source-only archive; never include SDKs, builds, credentials or fonts."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import sys
import zipfile

DIRECTORIES = ('Source', 'External', 'Tests', 'Examples', 'Assets', 'CMake', 'Tools', 'Docs', '.github')
ROOT_FILES = ('CMakeLists.txt', 'CMakePresets.json', 'README.md', 'LICENSE',
              'Setup.cmd', 'GenerateProjectFiles.bat', '.editorconfig', '.clang-format',
              '.gitignore', '.gitattributes')
FONT_SUFFIXES = {'.ttf', '.otf', '.ttc', '.woff', '.woff2', '.eot', '.fon'}
EXCLUDED_SUFFIXES = {'.pyc', '.pyo', '.obj', '.o', '.pdb', '.gcda', '.gcno'}


def collect_paths(root: Path) -> list[Path]:
    root = root.resolve()
    candidates = [root / name for name in ROOT_FILES if (root / name).exists()]
    for directory in DIRECTORIES:
        base = root / directory
        if base.is_symlink():
            raise ValueError(f'Refusing a symlink directory: {base}')
        if base.exists():
            candidates.extend(base.rglob('*'))
    result = []
    for path in candidates:
        relative = path.relative_to(root)
        if relative.parts[:2] == ('Docs', 'Archive') or path.name.endswith(('.log', '.log.err')):
            continue
        if path.is_symlink():
            raise ValueError(f'Refusing a symlink: {path}')
        if not path.is_file() or '__pycache__' in path.parts:
            continue
        if path.suffix.lower() in FONT_SUFFIXES:
            raise ValueError(f'Refusing to distribute a font file: {path}')
        if path.suffix.lower() in EXCLUDED_SUFFIXES:
            continue
        result.append(path)
    return sorted(result, key=lambda path: path.relative_to(root).as_posix())


def make_archive(root: Path, output: Path) -> dict:
    root, output = root.resolve(), output.resolve()
    files = collect_paths(root)
    if output in files:
        raise ValueError('The archive must not overwrite a source file')
    if output.suffix.lower() != '.zip':
        raise ValueError('Output must end in .zip')
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix('.zip.partial')
    records = []

    def add(archive: zipfile.ZipFile, name: str, data: bytes) -> None:
        info = zipfile.ZipInfo('dxlib_framework/' + name, (1980, 1, 1, 0, 0, 0))
        info.create_system = 3
        info.external_attr = 0o100644 << 16
        info.compress_type = zipfile.ZIP_DEFLATED
        archive.writestr(info, data, compresslevel=9)

    try:
        with zipfile.ZipFile(temporary, 'w') as archive:
            for path in files:
                relative = path.relative_to(root).as_posix()
                data = path.read_bytes()
                # Stable line endings and cmd compatibility across Git checkouts.
                if path.suffix.lower() in {'.cmd', '.bat', '.ps1'}:
                    data = data.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
                add(archive, relative, data)
                records.append({'path': relative, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
            manifest = {'format_version': 1, 'sdk_included': False, 'fonts_included': False, 'files': records}
            add(archive, 'DistributionManifest.json', (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode('utf-8'))
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest = make_archive(Path(__file__).resolve().parents[1], args.output)
    digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
    args.output.with_suffix('.zip.sha256').write_text(f'{digest}  {args.output.name}\n', encoding='utf-8')
    print(f'{args.output}: {len(manifest["files"])} source/documentation/test files')
    print(f'SHA-256: {digest}')
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f'PACKAGE FAILED: {error}', file=sys.stderr)
        raise SystemExit(1)
