#!/usr/bin/env python3
"""Atomically plan/apply the 1bb6548 rendering migration; never build or push implicitly."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

BASE = "1bb654826cb6b23de4837d744ab2634b25fb9979"
SUFFIXES = {".h", ".hpp", ".cpp", ".inl", ".cc", ".cxx"}
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|R"(?P<d>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=d)"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')

def git(root: Path, *args: str) -> str:
    result = subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(f"git {' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout.strip()

def blob(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()

def mask(source: str) -> str:
    return TOKEN.sub(lambda m: re.sub(r'[^\n]', ' ', m.group()), source)

def migrate_cpp(source: str, path: str) -> str:
    # A dimension-independent owner is renamed, not retained under a compatibility alias.
    source = re.sub(r'\bFRenderSystem2D\b', 'FRenderSystem', source)
    source = source.replace('RenderSystem2D.h', 'RenderSystem.h')
    code = mask(source)
    names = set(re.findall(r'\bFRenderContext\s*[&*]?\s*(\w+)', code))
    names.update(re.findall(r'\bauto\s*[&*]?\s*(\w+)\s*=\s*[^;\n]*\bGetContext\s*\(\s*\)', code))
    # Recognize the direct getter expression without assuming unrelated objects are render contexts.
    edits: list[tuple[int, int, str]] = []
    methods = {'Draw': 'DrawSprite', 'DrawText': 'DrawText', 'FillRectangle': 'FillRectangle'}
    patterns = [r'(?P<recv>\bGetContext\s*\(\s*\))\s*\.\s*(?P<name>DrawText|FillRectangle|Draw)\s*\(']
    if names:
        alternatives = '|'.join(re.escape(n) for n in sorted(names))
        patterns.append(r'(?P<recv>\b(?:' + alternatives + r'))\s*(?P<op>\.|->)\s*(?P<name>DrawText|FillRectangle|Draw)\s*\(')
    for pattern in patterns:
        for match in re.finditer(pattern, code):
            op = match.groupdict().get('op') or '.'
            recv = source[match.start('recv'):match.end('recv')]
            edits.append((match.start(), match.end(), recv + op + 'Get2D().' + methods[match['name']] + '('))
    for start, end, replacement in sorted(set(edits), reverse=True):
        source = source[:start] + replacement + source[end:]
    code = mask(source)
    # Existing context-batch tests are shipped already migrated. Unknown root-level batches are
    # intentionally rejected rather than inventing a binding or silently dropping a Jobs argument.
    for name in names:
        if re.search(r'\b' + re.escape(name) + r'\s*(?:\.|->)\s*SubmitGenerated\s*\(', code):
            raise RuntimeError(f"{path}: unrecognized root SubmitGenerated requires explicit migration; no files written")
    return source

def replace_once(source: str, old: str, new: str, label: str) -> str:
    if source.count(old) != 1:
        raise RuntimeError(f"{label}: expected exactly one integration anchor; no files written")
    return source.replace(old, new, 1)

def migrate_root_cmake(source: str) -> str:
    source = source.replace('RenderSystem2D.cpp', 'RenderSystem.cpp')
    source = replace_once(source, 'add_library(dxf_support STATIC', '''add_library(dxf_support STATIC
    Source/DxLibSupport/Private/Dxf/RenderGeometry3D.cpp
    Source/DxLibSupport/Private/Dxf/Render2DContext.cpp
    Source/DxLibSupport/Private/Dxf/Render3DContext.cpp
    Source/DxLibSupport/Private/Dxf/DebugDrawAdapters.cpp''', 'dxf_support')
    source = replace_once(source, 'set(DXF_NATIVE_SOURCES', '''set(DXF_NATIVE_SOURCES
    Source/Native/Private/Dxf/DxLibGeometryBackend.cpp''', 'DXF_NATIVE_SOURCES')
    source = replace_once(source, 'if(DXF_BUILD_TESTS)\n', '''if(DXF_BUILD_TESTS)
    include(CMake/RenderViewsTests.cmake)
    include(CMake/RenderContinuationTests.cmake)
    enable_testing()
    dxf_add_render_continuation_tests(dxf::support)
    dxf_add_render_views_tests(dxf::support)
''', 'test registration')
    return source

def bind_application(source: str) -> str:
    # Bind after every member (including the pool) has finished construction.
    pattern = re.compile(r'(m_Clock\(m_Settings\.MaxDeltaSeconds\)\s*)\{\s*\}')
    replacement = r'''\1{
	// Contextの次元別窓口が、Application所有の共有実行器を借用する。
	auto Bound = m_Renderer.SetExecutionJobs(m_ExecutionJobs);
	if (!Bound)
	{
		throw Toolbox::FException(Bound.Error().Message);
	}
}'''
    result, count = pattern.subn(replacement, source)
    if count != 1:
        raise RuntimeError('Application constructor changed; cannot safely bind shared Jobs; no files written')
    return result

def build_plan(root: Path, payload: Path) -> dict[str, bytes | None]:
    manifest = json.loads((payload / 'manifest.json').read_text(encoding='utf-8'))
    plan: dict[str, bytes | None] = {}
    for entry in manifest['files']:
        rel = entry['path']
        current = root / rel
        if current.is_symlink():
            raise RuntimeError(f"Refusing symlink target: {rel}")
        expected = entry.get('before_blob')
        if expected is not None:
            if not current.is_file():
                raise RuntimeError(f"Missing base file: {rel}")
            data = current.read_bytes()
            # Working-tree CRLF conversion is allowed for text; contents remain exact.
            if blob(data) != expected and blob(data.replace(b'\r\n', b'\n')) != expected:
                raise RuntimeError(f"Unexpected contents: {rel}; no files written")
        elif current.exists():
            raise RuntimeError(f"New path already exists: {rel}; no files written")
        if entry.get('delete'):
            plan[rel] = None
        else:
            data = (payload / 'ChangedFiles' / rel).read_bytes()
            if hashlib.sha256(data).hexdigest() != entry['sha256']:
                raise RuntimeError(f"Damaged payload: {rel}")
            plan[rel] = data
    # Scan only tracked project source, never build outputs, SDKs, or user files elsewhere.
    tracked = git(root, 'ls-files', '-z').split('\0')
    for rel in tracked:
        p = Path(rel)
        if not rel or p.parts[0] not in {'Source', 'Examples', 'Tests'} or p.suffix not in SUFFIXES:
            continue
        if rel in plan:
            continue
        old = (root / rel).read_text(encoding='utf-8-sig')
        new = migrate_cpp(old, rel)
        if old != new:
            plan[rel] = new.encode('utf-8')
    rel = 'Source/Runtime/Private/Dxf/Application.cpp'
    app = plan.get(rel) or (root / rel).read_bytes()
    plan[rel] = bind_application(app.decode('utf-8-sig')).encode('utf-8')
    plan['CMakeLists.txt'] = migrate_root_cmake((root / 'CMakeLists.txt').read_text(encoding='utf-8-sig')).encode('utf-8')
    # Extend the existing test double only; real native targets never see this header.
    fake = 'Tests/FakeDxLib/DxLib.h'
    text = (root / fake).read_text(encoding='utf-8-sig')
    if 'RenderViewsApi.h' in text or re.search(r'\b(?:DrawLine3D|DrawCircle|VECTOR)\b', mask(text)):
        raise RuntimeError('The native test double already has conflicting shape APIs; no files written')
    plan[fake] = (text.rstrip() + '\n#include "RenderViewsApi.h"\n').encode('utf-8')
    for rel in ['README.md', 'Docs/API.md', 'Docs/Architecture.md']:
        p = root / rel
        if p.exists():
            text = p.read_text(encoding='utf-8-sig')
            text = text.replace('FRenderSystem2D', 'FRenderSystem').replace('RenderSystem2D.h', 'RenderSystem.h')
            for receiver in ['Render', 'Context', 'Renderer.GetContext()']:
                for old, new in [('DrawText','DrawText'),('FillRectangle','FillRectangle'),('Draw','DrawSprite')]:
                    text = re.sub(re.escape(receiver) + r'\.' + old + r'\s*\(', receiver+'.Get2D().'+new+'(', text)
            text += '\n\n## 次元別描画APIへの移行\n\nこの版は `FRenderContext` の直接描画APIを削除しています。最新の入口・制約・例は [RenderViews](Docs/Rendering/ViewsAndDebug.md)（リポジトリroot基準）を参照してください。並列生成はApplicationに結び付いた実行器を使います。過去の監査ログは実行時点の記録として保持します。\n'
            if rel.startswith('Docs/'):
                text = text.replace('(Docs/Rendering/ViewsAndDebug.md)', '(Rendering/ViewsAndDebug.md)')
            plan[rel] = text.encode('utf-8')
    return plan

def validate_plan(root: Path, plan: dict[str, bytes | None]) -> None:
    # Reject unresolved project includes across the complete prospective tracked tree.
    prospective: dict[str, bytes] = {}
    for rel in git(root, 'ls-files', '-z').split('\0'):
        if rel and (root / rel).is_file(): prospective[rel] = (root / rel).read_bytes()
    for rel, data in plan.items():
        if data is None: prospective.pop(rel, None)
        else: prospective[rel] = data
    include_names = {str(Path(rel).relative_to(Path(*Path(rel).parts[:3]))) for rel in prospective
                     if len(Path(rel).parts)>3 and Path(rel).parts[0]=='Source' and Path(rel).parts[2]=='Public'}
    for rel, data in prospective.items():
        p = Path(rel)
        if not p.parts or p.parts[0] not in {'Source','Tests','Examples'} or p.suffix not in SUFFIXES: continue
        text = data.decode('utf-8-sig')
        for name in re.findall(r'^\s*#\s*include\s*"((?:Dxf|Toolbox)/[^"\n]+)"', text, re.M):
            if name not in include_names:
                # Some private includes intentionally use Dxf/ paths supplied only to that target.
                if not any(path.endswith('/Private/'+name) for path in prospective):
                    raise RuntimeError(f"Unresolved project include after migration: {rel} -> {name}")
        code = mask(text)
        if 'FRenderSystem2D' in code:
            raise RuntimeError(f"Unmigrated renderer type in {rel}")

def apply_transaction(root: Path, plan: dict[str, bytes | None], backup: Path) -> None:
    backup.mkdir(parents=True, exist_ok=False)
    before = {}
    for rel in plan:
        p = root / rel
        data = p.read_bytes() if p.exists() else None
        before[rel] = data
        if data is not None:
            b=backup/rel;b.parent.mkdir(parents=True,exist_ok=True);b.write_bytes(data)
    (backup/'plan.json').write_text(json.dumps({'paths':list(plan),'new_paths':[p for p,b in before.items() if b is None]},ensure_ascii=False,indent=2),encoding='utf-8')
    written=[]
    try:
        for rel, data in plan.items():
            p=root/rel;p.parent.mkdir(parents=True,exist_ok=True)
            if data is None: p.unlink()
            else:
                with tempfile.NamedTemporaryFile(dir=p.parent, delete=False) as f:
                    temp=Path(f.name);f.write(data)
                try: temp.replace(p)
                finally: temp.unlink(missing_ok=True)
            written.append(rel)
    except BaseException:
        # Roll back exactly our completed operations, never reset or clean a repository.
        for rel in reversed(written):
            p=root/rel
            if before[rel] is None: p.unlink(missing_ok=True)
            else: p.write_bytes(before[rel])
        raise

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path.cwd())
    parser.add_argument('--apply',action='store_true',help='Write the verified plan; default only checks')
    args=parser.parse_args()
    root=args.root.resolve();payload=Path(__file__).resolve().parent
    try:
        if git(root,'rev-parse','HEAD') != BASE: raise RuntimeError('HEAD is not the supported 1bb6548 base; no files written')
        if git(root,'status','--porcelain'): raise RuntimeError('The worktree is not clean; preserve/commit your work first; no files written')
        plan=build_plan(root,payload);validate_plan(root,plan)
        print(f'Prepared {len(plan)} file operations against {BASE}.')
        for rel in plan: print(('DELETE ' if plan[rel] is None else 'WRITE  ')+rel)
        if args.apply:
            backup=root.parent/('dxf-render-backup-'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
            apply_transaction(root,plan,backup)
            print('Applied. Backup:',backup)
            print('No build, commit, or push has been performed. Run the repository validation commands.')
        else: print('Check only: no repository files changed. Add --apply to write this plan.')
        return 0
    except Exception as error:
        print('STOP:',error,file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
