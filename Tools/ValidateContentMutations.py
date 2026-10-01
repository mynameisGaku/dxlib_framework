"""Run bounded Content mutations sequentially, preserving exact bytes and failure history.

No device run is needed here. Native and data-only rendering are separate validations.
Each mutation must build, fail its contract tests, restore, rebuild and pass again.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import time
from ValidationSupport import source_manifest
ROOT = Path(__file__).resolve().parents[1]
PRIVATE = 'Source/SceneContent/Private/Dxf/'

def once(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise RuntimeError(f'Mutation anchor count {text.count(old)}: {old}')
    return text.replace(old, new)

def changes(number: int) -> dict[str, object]:
    runtime = PRIVATE + 'PrefabRuntime.h'
    source = PRIVATE + 'SceneContentSource.cpp'
    reader = PRIVATE + 'ContentSchemaReader.cpp'
    request = PRIVATE + 'SceneContentRequest.cpp'
    if number == 1:
        return {runtime: lambda s: s.replace('B.Position = P.Position + Rotate(B.Position);', 'B.Position = {};').replace('B.Position = P.Position + Q.Rotate(B.Position);', 'B.Position = {};')}
    if number == 2:
        return {runtime: lambda s: once(s, 'return Require(m_Parts[PartExport(N, EContentExportKind::RigidBody)].Rigid);', 'static TObjectHandle<typename T::FBody> Shared; if (!Shared) { Shared = Require(m_Parts[PartExport(N, EContentExportKind::RigidBody)].Rigid); } return Shared;')}
    if number == 3:
        return {runtime: lambda s: s.replace('B.Position = P.Position + Rotate(B.Position);', 'B.Position = P.Position + B.Position;').replace('B.Position = P.Position + Q.Rotate(B.Position);', 'B.Position = P.Position + B.Position;')}
    if number == 4:
        def rotate(s):
            old='const auto& Source = D.Joints[I];'
            return once(s, old, 'auto Source = D.Joints[I]; auto Placement = m_Placement; Placement.Position = {}; auto Arm = D.Parts[Source.BodyA].Body; Arm.Position = Source.Prismatic.FrameA.LocalAnchor; T::Place(Arm, Placement); Source.Prismatic.FrameA.LocalAnchor = Arm.Position; Arm.Position = Source.Prismatic.FrameB.LocalAnchor; T::Place(Arm, Placement); Source.Prismatic.FrameB.LocalAnchor = Arm.Position;')
        return {runtime: rotate}
    if number == 5:
        def cache(s):
            for dimension in (2, 3):
                old=f'return Parse<FSchema{dimension}D>(Reader, Path, Overrides, Limits, Expand, SharedAssets);'
                new=f'auto Result = Parse<FSchema{dimension}D>(Reader, Path, Overrides, Limits, Expand, SharedAssets); static FPrefabDefinition{dimension}D Shared; if (!Overrides.IsEmpty()) {{ Shared = Result; }} else if (!Shared.Parts.IsEmpty()) {{ return Shared; }} return Result;'
                s=once(s,old,new)
            return s
        return {PRIVATE+'SceneContentParser.cpp': cache}
    if number == 6:
        return {PRIVATE+'ContentSchemaExtras.inl': lambda s: once(s, 'ReadParts<T>(R, R.Required(0, "parts"), D, Limits);', 'if (R.Find(0, "joints") >= 0 && R.Find(0, "joints") < R.Find(0, "parts")) { ReadJoints<T>(R, R.Find(0, "joints"), D, Limits); } ReadParts<T>(R, R.Required(0, "parts"), D, Limits);')}
    if number == 7:
        # Lost current endpoint is falsely considered Ready even after slot generation reuse.
        return {runtime: lambda s: once(s, 'if (m_State == EPrefabInstanceState::Ready && m_pWorld != nullptr)', 'if (false && m_State == EPrefabInstanceState::Ready && m_pWorld != nullptr)')}
    if number == 8:
        return {reader: lambda s: once(s, 'if (!Known)', 'if (false && !Known)')}
    if number == 9:
        # Keep cycle and expanded-size guards; removing the configurable depth guard remains finite.
        return {source: lambda s: once(s, 'if (m_Stack.Size() >= m_Limits.MaxPrefabDepth)', 'if (false && m_Stack.Size() >= m_Limits.MaxPrefabDepth)')}
    if number == 10:
        return {request: lambda s: once(s, 'm_Current = State;', 'if (!m_Current) { m_Current = State; }')}
    if number == 11:
        return {request: lambda s: once(s, 'm_Current->Status.Store(static_cast<Toolbox::uint32>(Target));', 'if (Target != ESceneContentRequestState::Canceled) { m_Current->Status.Store(static_cast<Toolbox::uint32>(Target)); }')}
    if number == 12:
        return {PRIVATE+'ContentResources.cpp': lambda s: s.replace('throw Toolbox::FException(Loaded.Error().Message);', 'continue;')}
    if number == 13:
        return {PRIVATE+f'PrefabInstance{d}D.cpp': lambda s: once(s, 'return !IsDestroyRequested();', 'return true;') for d in (2,3)}
    if number == 14:
        return {PRIVATE+'ContentVisuals.cpp': lambda s: once(s, 'RequireDraw(Model.SetTransform(World));', 'RequireDraw(Model.Advance(1.0 / 60.0)); RequireDraw(Model.SetTransform(World));')}
    if number == 15:
        return {runtime: lambda s: once(s, 'if (E.Kind != Kind)', 'if (false && E.Kind != Kind)')}
    if number == 16:
        return {source: lambda s: once(s, ': m_ProjectRoot(Toolbox::Move(Root)), m_Limits(Limits)', ': m_ProjectRoot(Toolbox::CurrentDirectory()), m_Limits(Limits)')}
    if number == 17:
        return {request: lambda s: once(s, 'm_Sequence = State->Sequence;', 'm_Sequence = State->Sequence; m_Last2D = {}; m_Last3D = {};')}
    if number == 18:
        return {runtime: lambda s: once(s, 'm_State = RequiresPhysics() ? EPrefabInstanceState::PendingPhysics : EPrefabInstanceState::Ready;', 'm_State = EPrefabInstanceState::Ready;')}
    raise ValueError(number)

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--config', choices=('Debug','Release'), default='Debug')
    parser.add_argument('--logs', type=Path, required=True)
    parser.add_argument('ids', nargs='*', type=int, default=list(range(1,19)))
    args = parser.parse_args()
    if any(n not in range(1,19) for n in args.ids): parser.error('ids must be 1..18')
    logs=args.logs.resolve()
    logs.mkdir(parents=True, exist_ok=False)
    def execute(command, name, binary=None):
        record={'command':command,'cwd':str(ROOT),'config':args.config,'source_sha256':source_manifest(ROOT)[0],'started_utc':datetime.now(timezone.utc).isoformat()}
        if binary: record['binary_sha256']=hashlib.sha256(binary.read_bytes()).hexdigest()
        start=time.monotonic()
        with (logs/(name+'.log')).open('x', encoding='utf-8') as out:
            try: record['exit']=subprocess.run(command,cwd=ROOT,stdout=out,stderr=subprocess.STDOUT,timeout=300).returncode
            except subprocess.TimeoutExpired: record['exit']='TIMEOUT'
        record.update(elapsed_seconds=time.monotonic()-start,finished_utc=datetime.now(timezone.utc).isoformat())
        (logs/(name+'-result.json')).write_text(json.dumps(record,indent=2),encoding='utf-8')
        return record
    for number in args.ids:
        name=f'B-M{number:02}'
        patches=changes(number)
        saved={ROOT/path:(ROOT/path).read_bytes() for path in patches}
        result={'id':name,'baseline_hashes':{str(p.relative_to(ROOT)):hashlib.sha256(b).hexdigest() for p,b in saved.items()}}
        backup=logs/(name+'-backup');backup.mkdir()
        for p,b in saved.items():
            dest=backup/p.relative_to(ROOT);dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(b)
        target='dxf_interaction_sample_tests' if number==14 else 'dxf_scene_content_tests'
        binary=args.build.resolve()/args.config/(target+'.exe')
        build=['cmake','--build',str(args.build.resolve()),'--config',args.config,'--target',target,'--parallel','4']
        try:
            for relative,patch in patches.items():
                p=ROOT/relative;b=saved[p];text=b.decode('utf-8-sig').replace('\r\n','\n');changed=patch(text)
                if changed==text: raise RuntimeError('Mutation produced no change')
                p.write_bytes((b'\xef\xbb\xbf' if b.startswith(b'\xef\xbb\xbf') else b'')+changed.replace('\n','\r\n').encode('utf-8'))
            result['build']=execute(build,name+'-build')
            if result['build']['exit']==0: result['red']=execute([str(binary)],name+'-red',binary)
        finally:
            for p,b in saved.items(): p.write_bytes(b)
            result['restored_hashes']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in saved}
            result['restored_mtime_ns']={str(p.relative_to(ROOT)):p.stat().st_mtime_ns for p in saved}
            if result['restored_hashes']!=result['baseline_hashes']: raise RuntimeError('Restoration byte mismatch')
            result['restore_build']=execute(build,name+'-restore-build')
            if result['restore_build']['exit']==0: result['green']=execute([str(binary)],name+'-green',binary)
            (logs/(name+'.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
        print(name,'build',result.get('build',{}).get('exit'),'Red',result.get('red',{}).get('exit'),'restore',result['restore_build']['exit'],'Green',result.get('green',{}).get('exit'),flush=True)
        if result.get('build',{}).get('exit')!=0 or result.get('red',{}).get('exit')!=1 or result.get('green',{}).get('exit')!=0:
            raise SystemExit('Mutation did not complete with expected behavioral detection: '+name)
if __name__=='__main__':main()
