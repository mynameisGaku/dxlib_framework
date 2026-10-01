"""Prove data-only changes with the same NativeGameplaySmoke executable.

Only new validation copies are edited. Each process keeps 2D/3D initialization,
split/odd rendering and broken-definition retry; it never rebuilds its executable.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import shutil
from ValidationSupport import run_logged, validation_report, source_manifest

ROOT = Path(__file__).resolve().parents[1]
WORLD = re.compile(r'CONTENT_WORLD ([23])d instances=(\d+) x=([-0-9.]+) y=([-0-9.]+) speedB=([-0-9.]+)')

def alter_data(root: Path) -> None:
    for dimension in (2, 3):
        path = root / f'Assets/Content/course{dimension}d.dxfscene.json'
        data = json.loads(path.read_text(encoding='utf-8'))
        data['instances'][0]['placement']['position'][0] = -4.0
        data['instances'][0]['parameters']['tint'] = [100, 200, 220, 255]
        data['instances'][1]['parameters']['speed'] = -.4
        data['instances'][1]['parameters']['effort'] = 12
        extra = copy.deepcopy(data['instances'][0])
        extra['id'] = 'extraData'
        extra['placement']['position'][0] = 6
        data['instances'].append(extra)
        path.write_bytes((json.dumps(data, ensure_ascii=False, indent=2) + '\n').replace('\n', '\r\n').encode('utf-8'))
        prefab = root / f'Assets/Content/door{dimension}d.dxfprefab.json'
        data = json.loads(prefab.read_text(encoding='utf-8'))
        # An existing different texture; model import remains the verified animated model.
        data['assets']['picture']['path'] = 'Assets/Models/ModelChecker.bmp'
        prefab.write_bytes((json.dumps(data, ensure_ascii=False, indent=2) + '\n').replace('\n', '\r\n').encode('utf-8'))

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--logs', type=Path, required=True)
    args = parser.parse_args()
    exe = args.exe.resolve()
    binary = hashlib.sha256(exe.read_bytes()).hexdigest()
    with validation_report(args.logs, {'binary': str(exe), 'binary_sha256': binary}, root=ROOT) as summary:
        results = {}
        for name in ('A', 'B'):
            target = args.logs.resolve() / ('Data-' + name + '- 日本語')
            target.mkdir()
            shutil.copytree(ROOT / 'Assets', target / 'Assets')
            if name == 'B':
                alter_data(target)
            manifest = [(p.relative_to(target).as_posix(), hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(target.rglob('*')) if p.is_file()]
            (args.logs / f'Data-{name}-manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
            output = args.logs.resolve() / ('Pixels-' + name)
            text = run_logged(args.logs, 'device-' + name, [str(exe), str(target), str(output), '--content-only'], cwd=args.logs.resolve(), timeout=180)
            matches = WORLD.findall(text)
            if len(matches) != 2 or 'CONTENT_FAILED_PREPARE_OLD_SCENE_RETRY_PASSED' not in text:
                raise RuntimeError('Missing real World or broken-data same-session retry evidence')
            results[name] = matches
            if hashlib.sha256(exe.read_bytes()).hexdigest() != binary:
                raise RuntimeError('Executable changed between data-only trials')
        for a, b in zip(results['A'], results['B']):
            if int(a[1]) != 3 or int(b[1]) != 4 or float(a[2]) != -3 or float(b[2]) != -4.0 or float(a[4]) != .8 or float(b[4]) != -.4:
                raise RuntimeError(f'Data was not used for World generation: {a} / {b}')
        for dimension in (2, 3):
            a = args.logs / f'Pixels-A/content{dimension}d-single.png'
            b = args.logs / f'Pixels-B/content{dimension}d-single.png'
            if a.read_bytes() == b.read_bytes():
                raise RuntimeError('Different data produced identical captured rendering')
        summary['world_trials'] = results
        final, _ = source_manifest(ROOT)
        if final != summary['source_sha256']:
            raise RuntimeError('Source or original Assets changed during data trials')
        print('Same executable; 2D/3D data A/B World and pixels differ; broken C retained old scene and retried.', flush=True)

if __name__ == '__main__':
    main()
