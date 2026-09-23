#!/usr/bin/env python3
"""Plan and apply the 86a37b9 renderer-owner repair and optional debug tools.
No network, build, commit, push, reset, clean or stash is performed.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

BASE = '86a37b9cdb7e3641e41f7838a8c5694de801679c'
CPP = {'.h', '.hpp', '.cpp', '.inl', '.cc', '.cxx'}
OLD_FILES = {
    'Source/DxLibSupport/Public/Dxf/RenderSystem2D.h': '1a90c143dab6bb6158ba99e7650dbf0cbc683b48',
    'Source/DxLibSupport/Private/Dxf/RenderSystem2D.cpp': '26b0b01bc2616d4f83d41ce3cffd9d747ba1d680',
}
REQUIRED = {
    'Source/DxLibSupport/Public/Dxf/RenderSystem.h': 'ee748e86f16f550397fcf062562bd4605797bdd1',
    'Source/DxLibSupport/Private/Dxf/RenderSystem.cpp': '4a72f8fbec1a1f0860e4d131d812c6b08c87d6bf',
    'Source/Runtime/Private/Dxf/Application.cpp': 'f4d6ccefb0d79ce57bf48aee02a2e78981e34db0',
}
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|R"(?P<d>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=d)"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')

def git(root: Path, *arguments: str) -> str:
    result = subprocess.run(['git', '-C', str(root), *arguments], capture_output=True, text=True,
                            encoding='utf-8', errors='strict', timeout=60)
    if result.returncode:
        raise RuntimeError(f"git {' '.join(arguments)} failed: {result.stderr.strip()}")
    return result.stdout.rstrip('\n')

def blob(data: bytes) -> str:
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()

def normalized_blob(data: bytes) -> str:
    return blob(data.replace(b'\r\n', b'\n'))

def mask(text: str) -> str:
    return TOKEN.sub(lambda match: re.sub(r'[^\n]', ' ', match.group()), text)

def migrate_cpp(text: str, path: str) -> str:
    # Exact type/include migration; there is no alias or compatibility owner.
    text = re.sub(r'\bFRenderSystem2D\b', 'FRenderSystem', text)
    text = text.replace('RenderSystem2D.h', 'RenderSystem.h')
    code = mask(text)
    names = set(re.findall(r'\bFRenderContext\s*[&*]?\s*(\w+)', code))
    names.update(re.findall(r'\bauto\s*[&*]?\s*(\w+)\s*=\s*[^;\n]*\bGetContext\s*\(\s*\)', code))
    patterns = [r'(?P<recv>\bGetContext\s*\(\s*\))\s*\.\s*(?P<name>DrawText|FillRectangle|Draw)\s*\(']
    if names:
        patterns.append(r'(?P<recv>\b(?:' + '|'.join(map(re.escape, sorted(names))) +
                        r'))\s*(?P<op>\.|->)\s*(?P<name>DrawText|FillRectangle|Draw)\s*\(')
    edits = set()
    for pattern in patterns:
        for match in re.finditer(pattern, code):
            method = 'DrawSprite' if match['name'] == 'Draw' else match['name']
            receiver = text[match.start('recv'):match.end('recv')]
            edits.add((match.start(), match.end(), receiver + (match.groupdict().get('op') or '.') +
                       'Get2D().' + method + '('))
    for start, end, replacement in sorted(edits, reverse=True):
        text = text[:start] + replacement + text[end:]
    for name in names:
        if re.search(r'\b' + re.escape(name) + r'\s*(?:\.|->)\s*SubmitGenerated\s*\(', mask(text)):
            raise RuntimeError(f'{path}: root SubmitGenerated needs a reviewed argument/binding migration; no files changed')
    return text

def bind_application(text: str) -> str:
    pattern = re.compile(r'(m_Clock\(m_Settings\.MaxDeltaSeconds\)\s*)\{\s*\}')
    replacement = r'''\1{
	// 全メンバー構築後に共有実行器を接続する。Contextへ別のThreadPoolを作らない。
	auto Bound = m_Renderer.GetContext().SetExecutionJobs_Internal(&m_ExecutionJobs);
	if (!Bound)
	{
		throw Toolbox::FException(Bound.Error().Message);
	}
}'''
    result, count = pattern.subn(replacement, text)
    if count != 1:
        raise RuntimeError('Application constructor no longer matches the audited integration point')
    return result

def migrate_cmake(text: str) -> str:
    old = 'Source/DxLibSupport/Private/Dxf/RenderSystem2D.cpp'
    new = 'Source/DxLibSupport/Private/Dxf/RenderSystem.cpp'
    if text.count(old) != 1 or new in text:
        raise RuntimeError('Unexpected renderer source registration')
    text = text.replace(old, new)
    anchor = '\nif(DXF_INSTALL)\n'
    if text.count(anchor) != 1 or 'include(CMake/DebugTools.cmake)' in text:
        raise RuntimeError('Unexpected root CMake integration point')
    return text.replace(anchor, '\n# Audited renderer owner and optional debug observations.\ninclude(CMake/DebugTools.cmake)\n' + anchor, 1)

def read_source(path: Path) -> str:
    return path.read_bytes().decode('utf-8-sig').replace('\r\n', '\n')

def checked_path(root: Path, relative: str) -> Path:
    path = Path(relative)
    if path.is_absolute() or '..' in path.parts or not path.parts:
        raise RuntimeError(f'Unsafe path: {relative}')
    resolved = (root / path).resolve()
    if not resolved.is_relative_to(root.resolve()):
        raise RuntimeError(f'Path escapes repository: {relative}')
    cursor = root / path
    while cursor != root:
        if cursor.is_symlink():
            raise RuntimeError(f'Symlink modification refused: {relative}')
        cursor = cursor.parent
    return root / path

def build_plan(root: Path, payload: Path, tracked: list[str]) -> dict[str, bytes | None]:
    for relative, expected in (OLD_FILES | REQUIRED).items():
        path = checked_path(root, relative)
        if not path.is_file() or normalized_blob(path.read_bytes()) != expected:
            raise RuntimeError(f'Unexpected base contents: {relative}')
    plan: dict[str, bytes | None] = {name: None for name in OLD_FILES}
    manifest = json.loads((payload / 'manifest.json').read_text(encoding='utf-8'))
    for entry in manifest['files']:
        relative = entry['path']
        target = checked_path(root, relative)
        if target.exists():
            raise RuntimeError(f'New path already exists: {relative}')
        source = checked_path(payload / 'Changes', relative)
        data = source.read_bytes()
        if hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise RuntimeError(f'Damaged payload: {relative}')
        plan[relative] = data
    for relative in tracked:
        if not relative or relative in plan:
            continue
        path = checked_path(root, relative)
        if not path.is_file():
            continue
        if Path(relative).parts[0] in {'Source', 'Tests', 'Examples'} and path.suffix in CPP:
            before = read_source(path)
            after = migrate_cpp(before, relative)
            if before != after:
                plan[relative] = after.encode('utf-8')
        elif path.name == 'CMakeLists.txt' or path.suffix == '.cmake':
            if relative != 'CMakeLists.txt':
                before = read_source(path)
                after = before.replace('/RenderSystem2D.cpp', '/RenderSystem.cpp')
                if after != before:
                    plan[relative] = after.encode('utf-8')
    app = 'Source/Runtime/Private/Dxf/Application.cpp'
    text = (plan[app].decode('utf-8') if app in plan else read_source(root / app))
    plan[app] = bind_application(text).encode('utf-8')
    plan['CMakeLists.txt'] = migrate_cmake(read_source(root / 'CMakeLists.txt')).encode('utf-8')
    # Only active API documentation is migrated. Old validation logs retain their historical meaning.
    for relative in ['README.md', 'Docs/API.md', 'Docs/Architecture.md']:
        path = root / relative
        if path.is_file():
            before = read_source(path)
            after = before.replace('FRenderSystem2D', 'FRenderSystem').replace('RenderSystem2D.h', 'RenderSystem.h')
            if after != before:
                plan[relative] = after.encode('utf-8')
    verify_includes(root, plan, tracked)
    return plan

def verify_includes(root: Path, plan: dict[str, bytes | None], tracked: list[str]) -> None:
    prospective = {name: (root / name).read_bytes() for name in tracked if name and (root / name).is_file()}
    for name, data in plan.items():
        if data is None:
            prospective.pop(name, None)
        else:
            prospective[name] = data
    includes = {name.split('/Public/', 1)[1] for name in prospective if name.startswith('Source/') and '/Public/' in name}
    for name, data in prospective.items():
        path = Path(name)
        if path.suffix not in CPP or path.parts[0] not in {'Source', 'Examples', 'Tests'}:
            continue
        text = data.decode('utf-8-sig')
        if re.search(r'\bFRenderSystem2D\b', mask(text)):
            raise RuntimeError(f'Old renderer still referenced: {name}')
        for header in re.findall(r'^\s*#\s*include\s*"((?:Dxf|Toolbox)/[^"\n]+)"', text, re.M):
            if header not in includes and not any(p.endswith('/Private/' + header) for p in prospective):
                raise RuntimeError(f'Unresolved project header: {name} -> {header}')
    # New translation units must actually appear in the included module registration.
    cmake = prospective['CMake/DebugTools.cmake'].decode('utf-8')
    for name in prospective:
        if name.startswith('Source/Debug/') and name.endswith('.cpp') and name not in cmake:
            raise RuntimeError(f'Unregistered debug source: {name}')


def apply_transaction(root: Path, plan: dict[str, bytes | None], backup: Path) -> None:
    # Snapshot all modified paths before the first write; this is not a git reset.
    before = {relative: (root / relative).read_bytes() if (root / relative).exists() else None for relative in plan}
    backup.mkdir(parents=True, exist_ok=False)
    for relative, data in before.items():
        if data is not None:
            target = backup / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    (backup / 'RESTORE_MANIFEST.json').write_text(json.dumps({'base': BASE, 'new_paths': [p for p, b in before.items() if b is None],
        'changed_paths': list(plan)}, ensure_ascii=False, indent=2), encoding='utf-8')
    completed = []
    try:
        for relative, data in plan.items():
            target = checked_path(root, relative)
            current = target.read_bytes() if target.exists() else None
            if current != before[relative]:
                raise RuntimeError(f'Concurrent file edit detected: {relative}')
            target.parent.mkdir(parents=True, exist_ok=True)
            if data is None:
                target.unlink()
            else:
                with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as handle:
                    temporary = Path(handle.name)
                    handle.write(data)
                try:
                    temporary.replace(target)
                finally:
                    temporary.unlink(missing_ok=True)
            completed.append(relative)
    except BaseException as error:
        conflicts = []
        for relative in reversed(completed):
            target = root / relative
            current = target.read_bytes() if target.exists() else None
            if current != plan[relative]:
                # Do not overwrite edits made by the user after our own write.
                conflicts.append(relative)
                continue
            if before[relative] is None:
                target.unlink(missing_ok=True)
            else:
                target.write_bytes(before[relative])
        if conflicts:
            raise RuntimeError('Concurrent edits preserved during rollback; inspect backup ' +
                               str(backup) + ': ' + ', '.join(conflicts)) from error
        raise

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        if git(root, 'rev-parse', '--show-toplevel').replace('\\', '/') != root.as_posix():
            raise RuntimeError('Run against the repository root, not a subdirectory')
        if git(root, 'rev-parse', 'HEAD') != BASE:
            raise RuntimeError(f'This payload requires HEAD {BASE}; never reset a newer checkout to apply it')
        if git(root, 'status', '--porcelain'):
            raise RuntimeError('Worktree is not clean. Preserve existing edits first; no files changed')
        tracked = git(root, 'ls-files', '-z').split('\0')
        plan = build_plan(root, Path(__file__).resolve().parent, tracked)
        for relative, data in sorted(plan.items()):
            print(('DELETE ' if data is None else 'WRITE  ') + relative)
        print(f'{len(plan)} planned paths; current HEAD={BASE}')
        if args.apply:
            stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
            backup = root.parent / ('dxf-debug-tools-backup-' + stamp)
            apply_transaction(root, plan, backup)
            print('Applied. Backup:', backup)
            print('No build, git commit or push was performed. Regenerate the solution before building.')
        else:
            print('Dry run only. Rerun with --apply to perform this exact plan.')
        return 0
    except (OSError, UnicodeError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print('Stopped:', error, file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
