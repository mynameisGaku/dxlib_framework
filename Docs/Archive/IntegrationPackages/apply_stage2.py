#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path
import shutil, subprocess, sys

BASE = "c75e7bb85a33865ba6b4d07c0064646d2c402408"
ROOT = Path.cwd()
PAYLOAD = Path(__file__).resolve().parent


def run(*args: str) -> str:
    result = subprocess.run(args, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        raise RuntimeError("$ " + " ".join(args) + "\n" + result.stdout)
    return result.stdout.strip()


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8-sig")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one anchor, found {count}: {old[:120]!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def insert_after_function(path: Path, signature: str, addition: str) -> None:
    text = path.read_text(encoding="utf-8-sig")
    start = text.find(signature)
    if start < 0:
        raise RuntimeError(f"{path}: function signature not found: {signature}")
    brace = text.find("{", start)
    if brace < 0:
        raise RuntimeError(f"{path}: function opening brace not found: {signature}")
    depth = 0
    i = brace
    state = "code"
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if ch == '"': state = "string"
            elif ch == "'": state = "char"
            elif ch == '/' and nxt == '/': state = "line"; i += 1
            elif ch == '/' and nxt == '*': state = "block"; i += 1
            elif ch == '{': depth += 1
            elif ch == '}':
                depth -= 1
                if depth == 0:
                    end = i + 1
                    text = text[:end] + "\n" + addition.rstrip() + "\n" + text[end:]
                    path.write_text(text, encoding="utf-8")
                    return
        elif state == "string":
            if ch == '\\': i += 1
            elif ch == '"': state = "code"
        elif state == "char":
            if ch == '\\': i += 1
            elif ch == "'": state = "code"
        elif state == "line":
            if ch == '\n': state = "code"
        elif state == "block":
            if ch == '*' and nxt == '/': state = "code"; i += 1
        i += 1
    raise RuntimeError(f"{path}: unterminated function: {signature}")


def copy_payload(relative: str) -> None:
    source = PAYLOAD / relative
    target = ROOT / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)


def ensure_stage1() -> None:
    if (ROOT / "Source/Toolbox/Public/Toolbox/JobSystem.h").exists():
        return
    patch = PAYLOAD / "dxlib_framework_c75e7bb_multithreading_stage1.patch"
    if not patch.exists():
        raise RuntimeError("Stage-1 patch is missing from the bundle")
    subprocess.run(["git", "apply", "--check", str(patch)], cwd=ROOT, check=True)
    subprocess.run(["git", "apply", str(patch)], cwd=ROOT, check=True)


def patch_cmake() -> None:
    path = ROOT / "CMakeLists.txt"
    replace_once(path,
'''add_library(dxf_physics STATIC
    Source/Physics/Private/PhysicsWorld2D.cpp
    Source/Physics/Private/PhysicsWorld3D.cpp)''',
'''add_library(dxf_physics STATIC
    Source/Physics/Private/ParallelPhysicsCore.cpp
    Source/Physics/Private/PhysicsWorld2D.cpp
    Source/Physics/Private/PhysicsWorld3D.cpp)''')
    path = ROOT / "CMake/PhysicsTests.cmake"
    replace_once(path,
'''        "${_root}/Tests/Physics/ContinuousTests.cpp"
        "${_root}/Tests/Physics/StabilityTests.cpp"
        "${_root}/Tests/Physics/TestCases.h")''',
'''        "${_root}/Tests/Physics/ContinuousTests.cpp"
        "${_root}/Tests/Physics/StabilityTests.cpp"
        "${_root}/Tests/Physics/ParallelTests.cpp"
        "${_root}/Tests/Physics/TestCases.h")''')
    replace_once(path,
'''    target_include_directories(${Target} PRIVATE "${_root}/Tests/Physics")''',
'''    target_include_directories(${Target} PRIVATE "${_root}/Tests/Physics" "${_root}/Source/Physics/Private")''')


def patch_test_registration() -> None:
    path = ROOT / "Tests/Physics/TestCases.h"
    replace_once(path,
'''const FCase* GetStabilityCases(Toolbox::size_t& Count) noexcept;
} // namespace PhysicsTest''',
'''const FCase* GetStabilityCases(Toolbox::size_t& Count) noexcept;
/**
 * 並列BroadPhase・Island・World実行の回帰ケースを返す。
 * @param Count 返す配列の要素数。
 */
const FCase* GetParallelCases(Toolbox::size_t& Count) noexcept;
} // namespace PhysicsTest''')
    path = ROOT / "Tools/PhysicsValidation/Main.cpp"
    replace_once(path,
'''\tCases = PhysicsTest::GetStabilityCases(Count);
\tFailed += RunGroup_Internal(Cases, Count);
\treturn Failed == 0 ? 0 : 1;''',
'''\tCases = PhysicsTest::GetStabilityCases(Count);
\tFailed += RunGroup_Internal(Cases, Count);
\tCases = PhysicsTest::GetParallelCases(Count);
\tFailed += RunGroup_Internal(Cases, Count);
\treturn Failed == 0 ? 0 : 1;''')


def patch_header(name: str) -> None:
    path = ROOT / f"Source/Physics/Public/Dxf/{name}.h"
    replace_once(path, '#include "Dxf/BodyType.h"', '#include "Dxf/BodyType.h"\n#include "Dxf/PhysicsExecution.h"')
    dimension = "平面" if name == "RigidBody2D" else "立体"
    replace_once(path,
                 f' * 力・重力・Impulseで動く{dimension}剛体を所有し、接触拘束を解く。\n * 単一スレッドで使用し、DxLibや描画を知らない。',
                 f' * 力・重力・Impulseで動く{dimension}剛体を所有し、接触拘束を解く。\n * 公開APIは呼び出し側で直列化し、Step内部だけ任意のJob Systemへ並列化できる。\n * DxLibや描画を知らない。')
    replace_once(path,
'''\tbool IsColliderAlive(''' + ("FColliderId2D" if name == "RigidBody2D" else "FColliderId3D") + ''' Id) const noexcept;
\t/**
\t * 指定秒数だけ物理状態を進める。''',
'''\tbool IsColliderAlive(''' + ("FColliderId2D" if name == "RigidBody2D" else "FColliderId3D") + ''' Id) const noexcept;
\t/**
\t * Step内部で利用するJob Systemと並列化対象を設定する。
\t * JobSystemは非所有で、Step中とWorld生存中は呼び出し側が寿命を保証する。
\t * @param Settings 並列実行設定。
\t */
\tvoid SetExecutionSettings(const FPhysicsExecutionSettings& Settings) noexcept;
\t/**
\t * 現在の並列実行設定を返す。
\t */
\tFPhysicsExecutionSettings GetExecutionSettings() const noexcept;
\t/**
\t * 直近StepのBroadPhase・Manifold・Island診断を返す。
\t */
\tFPhysicsExecutionDiagnostics GetExecutionDiagnostics() const noexcept;
\t/**
\t * 指定秒数だけ物理状態を進める。''')


def common_cpp_prefix(path: Path) -> None:
    replace_once(path, '#include "Toolbox/ContinuousCollision.h"\n#include "Toolbox/Vector.h"',
                 '#include "Toolbox/ContinuousCollision.h"\n#include "Toolbox/Vector.h"\n#include "ParallelPhysicsCore.h"')


def patch_next_world(path: Path, dimension: str) -> None:
    old = '''\tstatic Toolbox::uint64 NextWorld_Internal()
\t{
\t\t// 単一スレッド利用を前提とした通し番号。
\t\tstatic Toolbox::uint64 Next = 1;
\t\tconst Toolbox::uint64 Issued = Next;
\t\tNext += 1;
\t\treturn Issued;
\t}'''
    new = '''\tstatic Toolbox::uint64 NextWorld_Internal()
\t{
\t\t// World生成自体が別スレッドで重なってもIDを重複させない。
\t\tstatic Toolbox::FAtomicCounter Next(1);
\t\treturn Next.FetchAdd(1);
\t}'''
    replace_once(path, old, new)


def patch_execution_fields(path: Path, sleep_type: str) -> None:
    replace_once(path,
                 f'''\t// 休止の条件。\n\t{sleep_type} Sleep;''',
                 f'''\t// 休止の条件。\n\t{sleep_type} Sleep;\n\t// Step内部だけで利用する非所有Job Systemと並列化設定。\n\tFPhysicsExecutionSettings Execution;\n\t// 直近Stepで集計した並列実行診断。\n\tFPhysicsExecutionDiagnostics ExecutionDiagnostics;''')


def broadphase_method_2d() -> str:
    return r'''\t// Sweep-and-Prune候補から接触多様体列を決定的順序で作る。
\tvoid GenerateManifolds_Internal(Toolbox::TVector<FManifold2D>& Out)
\t{
\t\tOut.Clear();
\t\tToolbox::TVector<PhysicsPrivate::FBroadPhaseEntry> Entries;
\t\tEntries.Reserve(Colliders.Size());
\t\tfor (Toolbox::size_t Index = 0; Index < Colliders.Size(); ++Index)
\t\t{
\t\t\tconst FColliderRecord2D& Record = Colliders[Index];
\t\t\tconst FBodyRecord2D* Body = Record.bAlive ? Find_Internal(Record.Body) : nullptr;
\t\t\tif (Body == nullptr)
\t\t\t{
\t\t\t\tcontinue;
\t\t\t}
\t\t\tPhysicsPrivate::FBroadPhaseEntry Entry;
\t\t\tEntry.ColliderIndex = Index;
\t\t\tEntry.BodyIndex = Record.Body.Index;
\t\t\tEntry.BodyGeneration = Record.Body.Generation;
\t\t\tEntry.bDynamic = Body->Type == EBodyType::Dynamic;
\t\t\tif (Record.Shape.Index() == 0)
\t\t\t{
\t\t\t\tconst Toolbox::FCircle2D Circle = ToWorld_Internal(*Body, Record.Shape.Get<0>());
\t\t\t\tEntry.Bounds.MinX = Toolbox::f64(Circle.Center.X) - Circle.Radius;
\t\t\t\tEntry.Bounds.MinY = Toolbox::f64(Circle.Center.Y) - Circle.Radius;
\t\t\t\tEntry.Bounds.MaxX = Toolbox::f64(Circle.Center.X) + Circle.Radius;
\t\t\t\tEntry.Bounds.MaxY = Toolbox::f64(Circle.Center.Y) + Circle.Radius;
\t\t\t}
\t\t\telse
\t\t\t{
\t\t\t\tconst Toolbox::FOrientedBox2D Box = ToWorld_Internal(*Body, Record.Shape.Get<1>());
\t\t\t\tToolbox::FVector2 U;
\t\t\t\tToolbox::FVector2 V;
\t\t\t\tBoxAxes_Internal(Box, U, V);
\t\t\t\tconst Toolbox::f64 ExtentX = Toolbox::Abs(Toolbox::f64(U.X) * Box.HalfExtents.X) +
\t\t\t\t                               Toolbox::Abs(Toolbox::f64(V.X) * Box.HalfExtents.Y);
\t\t\t\tconst Toolbox::f64 ExtentY = Toolbox::Abs(Toolbox::f64(U.Y) * Box.HalfExtents.X) +
\t\t\t\t                               Toolbox::Abs(Toolbox::f64(V.Y) * Box.HalfExtents.Y);
\t\t\t\tEntry.Bounds.MinX = Toolbox::f64(Box.Center.X) - ExtentX;
\t\t\t\tEntry.Bounds.MinY = Toolbox::f64(Box.Center.Y) - ExtentY;
\t\t\t\tEntry.Bounds.MaxX = Toolbox::f64(Box.Center.X) + ExtentX;
\t\t\t\tEntry.Bounds.MaxY = Toolbox::f64(Box.Center.Y) + ExtentY;
\t\t\t}
\t\t\tEntries.PushBack(Entry);
\t\t}
\t\tToolbox::TVector<PhysicsPrivate::FBroadPhasePair> Pairs;
\t\tToolbox::FJobSystem* BroadJobs = Execution.bParallelBroadPhase ? Execution.JobSystem : nullptr;
\t\tPhysicsPrivate::FBroadPhase::Generate(Entries, Contact.ContactSlop, BroadJobs, Pairs);
\t\tExecutionDiagnostics.CandidatePairCount += Pairs.Size();
\t\tToolbox::TVector<FManifold2D> Results(Pairs.Size());
\t\tauto GenerateOne = [&](Toolbox::size_t PairIndex)
\t\t{
\t\t\tconst PhysicsPrivate::FBroadPhasePair& Pair = Pairs[PairIndex];
\t\t\tconst FColliderRecord2D& RecordA = Colliders[Pair.FirstColliderIndex];
\t\t\tconst FColliderRecord2D& RecordB = Colliders[Pair.SecondColliderIndex];
\t\t\tconst FBodyRecord2D* BodyA = Find_Internal(RecordA.Body);
\t\t\tconst FBodyRecord2D* BodyB = Find_Internal(RecordB.Body);
\t\t\tif (BodyA == nullptr || BodyB == nullptr)
\t\t\t{
\t\t\t\treturn;
\t\t\t}
\t\t\tconst FColliderId2D IdA = {RecordA.Body, Pair.FirstColliderIndex, RecordA.Generation};
\t\t\tconst FColliderId2D IdB = {RecordB.Body, Pair.SecondColliderIndex, RecordB.Generation};
\t\t\tFManifold2D& Manifold = Results[PairIndex];
\t\t\tif (ColliderLess_Internal(IdB, IdA))
\t\t\t{
\t\t\t\tAppendPairManifold_Internal(RecordB, IdB, RecordA, IdA, *BodyB, *BodyA, Manifold);
\t\t\t}
\t\t\telse
\t\t\t{
\t\t\t\tAppendPairManifold_Internal(RecordA, IdA, RecordB, IdB, *BodyA, *BodyB, Manifold);
\t\t\t}
\t\t};
\t\tToolbox::FJobSystem* NarrowJobs = Execution.bParallelNarrowPhase ? Execution.JobSystem : nullptr;
\t\tif (NarrowJobs != nullptr && NarrowJobs->GetExecutionThreadCount() > 1)
\t\t{
\t\t\tif (!Toolbox::ParallelFor(*NarrowJobs, Pairs.Size(), GenerateOne, 8))
\t\t\t{
\t\t\t\tthrow Toolbox::FException("Parallel 2D narrow phase failed");
\t\t\t}
\t\t}
\t\telse
\t\t{
\t\t\tfor (Toolbox::size_t Index = 0; Index < Pairs.Size(); ++Index)
\t\t\t{
\t\t\t\tGenerateOne(Index);
\t\t\t}
\t\t}
\t\tfor (Toolbox::size_t Index = 0; Index < Results.Size(); ++Index)
\t\t{
\t\t\tif (!Results[Index].Points.IsEmpty())
\t\t\t{
\t\t\t\tOut.PushBack(Toolbox::Move(Results[Index]));
\t\t\t}
\t\t}
\t\tExecutionDiagnostics.ManifoldCount += Out.Size();
\t}'''


def broadphase_method_3d() -> str:
    return r'''\t// Sweep-and-Prune候補から接触多様体列を決定的順序で作る。
\tvoid GenerateManifolds_Internal(Toolbox::TVector<FManifold3D>& Out)
\t{
\t\tOut.Clear();
\t\tToolbox::TVector<PhysicsPrivate::FBroadPhaseEntry> Entries;
\t\tEntries.Reserve(Colliders.Size());
\t\tfor (Toolbox::size_t Index = 0; Index < Colliders.Size(); ++Index)
\t\t{
\t\t\tconst FColliderRecord3D& Record = Colliders[Index];
\t\t\tconst FBodyRecord3D* Body = Record.bAlive ? Find_Internal(Record.Body) : nullptr;
\t\t\tif (Body == nullptr)
\t\t\t{
\t\t\t\tcontinue;
\t\t\t}
\t\t\tPhysicsPrivate::FBroadPhaseEntry Entry;
\t\t\tEntry.ColliderIndex = Index;
\t\t\tEntry.BodyIndex = Record.Body.Index;
\t\t\tEntry.BodyGeneration = Record.Body.Generation;
\t\t\tEntry.bDynamic = Body->Type == EBodyType::Dynamic;
\t\t\tEntry.Bounds.bUseZ = true;
\t\t\tif (Record.Shape.Index() == 0)
\t\t\t{
\t\t\t\tconst Toolbox::FSphere Sphere = ToWorld_Internal(*Body, Record.Shape.Get<0>());
\t\t\t\tEntry.Bounds.MinX = Toolbox::f64(Sphere.Center.X) - Sphere.Radius;
\t\t\t\tEntry.Bounds.MinY = Toolbox::f64(Sphere.Center.Y) - Sphere.Radius;
\t\t\t\tEntry.Bounds.MinZ = Toolbox::f64(Sphere.Center.Z) - Sphere.Radius;
\t\t\t\tEntry.Bounds.MaxX = Toolbox::f64(Sphere.Center.X) + Sphere.Radius;
\t\t\t\tEntry.Bounds.MaxY = Toolbox::f64(Sphere.Center.Y) + Sphere.Radius;
\t\t\t\tEntry.Bounds.MaxZ = Toolbox::f64(Sphere.Center.Z) + Sphere.Radius;
\t\t\t}
\t\t\telse
\t\t\t{
\t\t\t\tconst Toolbox::FOBB Box = ToWorld_Internal(*Body, Record.Shape.Get<1>());
\t\t\t\tconst Toolbox::f64 Half[3] = {Box.HalfExtents.X, Box.HalfExtents.Y, Box.HalfExtents.Z};
\t\t\t\tToolbox::f64 ExtentX = 0;
\t\t\t\tToolbox::f64 ExtentY = 0;
\t\t\t\tToolbox::f64 ExtentZ = 0;
\t\t\t\tfor (Toolbox::size_t Axis = 0; Axis < 3; ++Axis)
\t\t\t\t{
\t\t\t\t\tExtentX += Toolbox::Abs(Toolbox::f64(Box.Axes[Axis].X) * Half[Axis]);
\t\t\t\t\tExtentY += Toolbox::Abs(Toolbox::f64(Box.Axes[Axis].Y) * Half[Axis]);
\t\t\t\t\tExtentZ += Toolbox::Abs(Toolbox::f64(Box.Axes[Axis].Z) * Half[Axis]);
\t\t\t\t}
\t\t\t\tEntry.Bounds.MinX = Toolbox::f64(Box.Center.X) - ExtentX;
\t\t\t\tEntry.Bounds.MinY = Toolbox::f64(Box.Center.Y) - ExtentY;
\t\t\t\tEntry.Bounds.MinZ = Toolbox::f64(Box.Center.Z) - ExtentZ;
\t\t\t\tEntry.Bounds.MaxX = Toolbox::f64(Box.Center.X) + ExtentX;
\t\t\t\tEntry.Bounds.MaxY = Toolbox::f64(Box.Center.Y) + ExtentY;
\t\t\t\tEntry.Bounds.MaxZ = Toolbox::f64(Box.Center.Z) + ExtentZ;
\t\t\t}
\t\t\tEntries.PushBack(Entry);
\t\t}
\t\tToolbox::TVector<PhysicsPrivate::FBroadPhasePair> Pairs;
\t\tToolbox::FJobSystem* BroadJobs = Execution.bParallelBroadPhase ? Execution.JobSystem : nullptr;
\t\tPhysicsPrivate::FBroadPhase::Generate(Entries, Contact.ContactSlop, BroadJobs, Pairs);
\t\tExecutionDiagnostics.CandidatePairCount += Pairs.Size();
\t\tToolbox::TVector<FManifold3D> Results(Pairs.Size());
\t\tauto GenerateOne = [&](Toolbox::size_t PairIndex)
\t\t{
\t\t\tconst PhysicsPrivate::FBroadPhasePair& Pair = Pairs[PairIndex];
\t\t\tconst FColliderRecord3D& RecordA = Colliders[Pair.FirstColliderIndex];
\t\t\tconst FColliderRecord3D& RecordB = Colliders[Pair.SecondColliderIndex];
\t\t\tconst FBodyRecord3D* BodyA = Find_Internal(RecordA.Body);
\t\t\tconst FBodyRecord3D* BodyB = Find_Internal(RecordB.Body);
\t\t\tif (BodyA == nullptr || BodyB == nullptr)
\t\t\t{
\t\t\t\treturn;
\t\t\t}
\t\t\tconst FColliderId3D IdA = {RecordA.Body, Pair.FirstColliderIndex, RecordA.Generation};
\t\t\tconst FColliderId3D IdB = {RecordB.Body, Pair.SecondColliderIndex, RecordB.Generation};
\t\t\tFManifold3D& Manifold = Results[PairIndex];
\t\t\tif (ColliderLess_Internal(IdB, IdA))
\t\t\t{
\t\t\t\tAppendPairManifold_Internal(RecordB, IdB, RecordA, IdA, *BodyB, *BodyA, Manifold);
\t\t\t}
\t\t\telse
\t\t\t{
\t\t\t\tAppendPairManifold_Internal(RecordA, IdA, RecordB, IdB, *BodyA, *BodyB, Manifold);
\t\t\t}
\t\t};
\t\tToolbox::FJobSystem* NarrowJobs = Execution.bParallelNarrowPhase ? Execution.JobSystem : nullptr;
\t\tif (NarrowJobs != nullptr && NarrowJobs->GetExecutionThreadCount() > 1)
\t\t{
\t\t\tif (!Toolbox::ParallelFor(*NarrowJobs, Pairs.Size(), GenerateOne, 8))
\t\t\t{
\t\t\t\tthrow Toolbox::FException("Parallel 3D narrow phase failed");
\t\t\t}
\t\t}
\t\telse
\t\t{
\t\t\tfor (Toolbox::size_t Index = 0; Index < Pairs.Size(); ++Index)
\t\t\t{
\t\t\t\tGenerateOne(Index);
\t\t\t}
\t\t}
\t\tfor (Toolbox::size_t Index = 0; Index < Results.Size(); ++Index)
\t\t{
\t\t\tif (!Results[Index].Points.IsEmpty())
\t\t\t{
\t\t\t\tOut.PushBack(Toolbox::Move(Results[Index]));
\t\t\t}
\t\t}
\t\tExecutionDiagnostics.ManifoldCount += Out.Size();
\t}'''


def solver_helpers(kind: str) -> str:
    suffix = "2D" if kind == "2D" else "3D"
    manifold = f"FManifold{suffix}"
    body = f"FBodyRecord{suffix}"
    point = "Toolbox::FVector2" if kind == "2D" else "Toolbox::FVector3"
    return f'''\t// ManifoldからDynamic接触Islandを構築する。\n\tvoid BuildIslands_Internal(const Toolbox::TVector<{manifold}>& Manifolds,\n\t                          Toolbox::TVector<PhysicsPrivate::FPhysicsIsland>& Out)\n\t{{\n\t\tToolbox::TVector<PhysicsPrivate::FIslandEdge> Edges;\n\t\tEdges.Reserve(Manifolds.Size());\n\t\tfor (Toolbox::size_t Index = 0; Index < Manifolds.Size(); ++Index)\n\t\t{{\n\t\t\tconst {manifold}& Manifold = Manifolds[Index];\n\t\t\tconst {body}* BodyA = Find_Internal(Manifold.BodyA);\n\t\t\tconst {body}* BodyB = Find_Internal(Manifold.BodyB);\n\t\t\tif (BodyA == nullptr || BodyB == nullptr)\n\t\t\t{{\n\t\t\t\tcontinue;\n\t\t\t}}\n\t\t\tPhysicsPrivate::FIslandEdge Edge;\n\t\t\tEdge.BodyA = Manifold.BodyA.Index;\n\t\t\tEdge.BodyB = Manifold.BodyB.Index;\n\t\t\tEdge.ConstraintIndex = Index;\n\t\t\tEdge.bDynamicA = BodyA->Type == EBodyType::Dynamic;\n\t\t\tEdge.bDynamicB = BodyB->Type == EBodyType::Dynamic;\n\t\t\tEdges.PushBack(Edge);\n\t\t}}\n\t\tPhysicsPrivate::FIslandManager::Build(Slots.Size(), Edges, Out);\n\t\tExecutionDiagnostics.IslandCount += Out.Size();\n\t}}\n\t// 明示的に起きたBodyまたは接触相手の運動をIsland全体へ伝播する。\n\tvoid WakeIslands_Internal(const Toolbox::TVector<{manifold}>& Manifolds,\n\t                         const Toolbox::TVector<PhysicsPrivate::FPhysicsIsland>& Islands) noexcept\n\t{{\n\t\tfor (Toolbox::size_t IslandIndex = 0; IslandIndex < Islands.Size(); ++IslandIndex)\n\t\t{{\n\t\t\tconst PhysicsPrivate::FPhysicsIsland& Island = Islands[IslandIndex];\n\t\t\tbool bWake = false;\n\t\t\tfor (Toolbox::size_t BodySlot = 0; BodySlot < Island.BodyIndices.Size(); ++BodySlot)\n\t\t\t{{\n\t\t\t\tconst {body}& Record = Slots[Island.BodyIndices[BodySlot]];\n\t\t\t\tif (Record.bAlive && Record.Type == EBodyType::Dynamic && !Record.bSleeping)\n\t\t\t\t{{\n\t\t\t\t\tbWake = true;\n\t\t\t\t\tbreak;\n\t\t\t\t}}\n\t\t\t}}\n\t\t\tfor (Toolbox::size_t ConstraintSlot = 0; !bWake && ConstraintSlot < Island.ConstraintIndices.Size(); ++ConstraintSlot)\n\t\t\t{{\n\t\t\t\tconst {manifold}& Manifold = Manifolds[Island.ConstraintIndices[ConstraintSlot]];\n\t\t\t\tconst {body}* BodyA = Find_Internal(Manifold.BodyA);\n\t\t\t\tconst {body}* BodyB = Find_Internal(Manifold.BodyB);\n\t\t\t\tif (BodyA == nullptr || BodyB == nullptr)\n\t\t\t\t{{\n\t\t\t\t\tcontinue;\n\t\t\t\t}}\n\t\t\t\tfor (Toolbox::size_t PointIndex = 0; PointIndex < Manifold.Points.Size(); ++PointIndex)\n\t\t\t\t{{\n\t\t\t\t\tif (ShouldWakeForMotion_Internal(*BodyA, *BodyB, Manifold.Points[PointIndex].Position))\n\t\t\t\t\t{{\n\t\t\t\t\t\tbWake = true;\n\t\t\t\t\t\tbreak;\n\t\t\t\t\t}}\n\t\t\t\t}}\n\t\t\t}}\n\t\t\tif (bWake)\n\t\t\t{{\n\t\t\t\tfor (Toolbox::size_t BodySlot = 0; BodySlot < Island.BodyIndices.Size(); ++BodySlot)\n\t\t\t\t{{\n\t\t\t\t\t{body}& Record = Slots[Island.BodyIndices[BodySlot]];\n\t\t\t\t\tif (Record.bAlive && Record.Type == EBodyType::Dynamic)\n\t\t\t\t\t{{\n\t\t\t\t\t\tWake_Internal(Record);\n\t\t\t\t\t}}\n\t\t\t\t}}\n\t\t\t}}\n\t\t}}\n\t}}\n\t// Dynamic Bodyを共有しないIslandだけをWorkerへ分割して速度拘束を解く。\n\tvoid SolveVelocitiesParallel_Internal(Toolbox::TVector<{manifold}>& Manifolds,\n\t                                     const Toolbox::TVector<PhysicsPrivate::FPhysicsIsland>& Islands)\n\t{{\n\t\tToolbox::FJobSystem* Jobs = Execution.bParallelIslandSolver ? Execution.JobSystem : nullptr;\n\t\tif (Jobs == nullptr || Jobs->GetExecutionThreadCount() <= 1 || Islands.Size() < 2)\n\t\t{{\n\t\t\tSolveVelocities_Internal(Manifolds);\n\t\t\treturn;\n\t\t}}\n\t\tfor (Toolbox::uint32 Iteration = 0; Iteration < Contact.VelocityIterations; ++Iteration)\n\t\t{{\n\t\t\tauto SolveIsland = [&](Toolbox::size_t IslandIndex)\n\t\t\t{{\n\t\t\t\tconst PhysicsPrivate::FPhysicsIsland& Island = Islands[IslandIndex];\n\t\t\t\tfor (Toolbox::size_t ConstraintSlot = 0; ConstraintSlot < Island.ConstraintIndices.Size(); ++ConstraintSlot)\n\t\t\t\t{{\n\t\t\t\t\t{manifold}& Manifold = Manifolds[Island.ConstraintIndices[ConstraintSlot]];\n\t\t\t\t\t{body}* BodyA = Find_Internal(Manifold.BodyA);\n\t\t\t\t\t{body}* BodyB = Find_Internal(Manifold.BodyB);\n\t\t\t\t\tif (BodyA == nullptr || BodyB == nullptr)\n\t\t\t\t\t{{\n\t\t\t\t\t\tcontinue;\n\t\t\t\t\t}}\n\t\t\t\t\tfor (Toolbox::size_t PointIndex = 0; PointIndex < Manifold.Points.Size(); ++PointIndex)\n\t\t\t\t\t{{\n\t\t\t\t\t\tSolvePoint_Internal(*BodyA, *BodyB, Manifold.Points[PointIndex], Contact.RestitutionThreshold);\n\t\t\t\t\t}}\n\t\t\t\t}}\n\t\t\t}};\n\t\t\tif (!Toolbox::ParallelFor(*Jobs, Islands.Size(), SolveIsland, 1))\n\t\t\t{{\n\t\t\t\tthrow Toolbox::FException("Parallel {suffix} island solver failed");\n\t\t\t}}\n\t\t}}\n\t}}\n\t// Wake・Warm Start・Island Solver・Cache保存を一つの決定的な接触フェーズとして実行する。\n\tvoid SolveContacts_Internal(Toolbox::TVector<{manifold}>& Manifolds)\n\t{{\n\t\tToolbox::TVector<PhysicsPrivate::FPhysicsIsland> Islands;\n\t\tBuildIslands_Internal(Manifolds, Islands);\n\t\tWakeIslands_Internal(Manifolds, Islands);\n\t\tfor (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)\n\t\t{{\n\t\t\tWarmStart_Internal(Manifolds[ManifoldIndex]);\n\t\t}}\n\t\tSolveVelocitiesParallel_Internal(Manifolds, Islands);\n\t\tStoreCache_Internal(Manifolds);\n\t}}'''


def patch_angular_wake(path: Path, kind: str) -> None:
    text = path.read_text(encoding="utf-8-sig")
    if kind == "2D":
        anchor = '''\t\tif (BodyB.bSleeping)\n\t\t{\n\t\t\tPrevVelB = {};\n\t\t\tPrevSpinB = 0;\n\t\t}\n\t\tconst Toolbox::f64 ArmAX'''
        repl = '''\t\tif (BodyB.bSleeping)\n\t\t{\n\t\t\tPrevVelB = {};\n\t\t\tPrevSpinB = 0;\n\t\t}\n\t\tconst Toolbox::f64 SpinA = PrevSpinA < 0 ? -Toolbox::f64(PrevSpinA) : PrevSpinA;\n\t\tconst Toolbox::f64 SpinB = PrevSpinB < 0 ? -Toolbox::f64(PrevSpinB) : PrevSpinB;\n\t\tif (SpinA > Sleep.AngularSpeedLimit || SpinB > Sleep.AngularSpeedLimit)\n\t\t{\n\t\t\treturn true;\n\t\t}\n\t\tconst Toolbox::f64 ArmAX'''
    else:
        anchor = '''\t\tif (BodyB.bSleeping)\n\t\t{\n\t\t\tPrevVelB = {};\n\t\t\tPrevSpinB = {};\n\t\t}\n\t\tconst FVector3D ArmA'''
        repl = '''\t\tif (BodyB.bSleeping)\n\t\t{\n\t\t\tPrevVelB = {};\n\t\t\tPrevSpinB = {};\n\t\t}\n\t\tconst Toolbox::f64 SpinA = Toolbox::Sqrt(Toolbox::f64(PrevSpinA.X) * PrevSpinA.X + Toolbox::f64(PrevSpinA.Y) * PrevSpinA.Y + Toolbox::f64(PrevSpinA.Z) * PrevSpinA.Z);\n\t\tconst Toolbox::f64 SpinB = Toolbox::Sqrt(Toolbox::f64(PrevSpinB.X) * PrevSpinB.X + Toolbox::f64(PrevSpinB.Y) * PrevSpinB.Y + Toolbox::f64(PrevSpinB.Z) * PrevSpinB.Z);\n\t\tif (SpinA > Sleep.AngularSpeedLimit || SpinB > Sleep.AngularSpeedLimit)\n\t\t{\n\t\t\treturn true;\n\t\t}\n\t\tconst FVector3D ArmA'''
    count = text.count(anchor)
    if count != 1:
        raise RuntimeError(f"{path}: angular wake anchor count {count}")
    path.write_text(text.replace(anchor, repl, 1), encoding="utf-8")


def patch_generate(path: Path, kind: str) -> None:
    sig = f"\tvoid GenerateManifolds_Internal(Toolbox::TVector<FManifold{kind}>& Out)"
    text = path.read_text(encoding="utf-8-sig")
    if text.count(sig) != 1:
        raise RuntimeError(f"{path}: manifold signature count {text.count(sig)}")
    text = text.replace(sig, f"\tvoid GenerateManifoldsBruteForce_Internal(Toolbox::TVector<FManifold{kind}>& Out)", 1)
    path.write_text(text, encoding="utf-8")
    insert_after_function(path, f"\tvoid GenerateManifoldsBruteForce_Internal(Toolbox::TVector<FManifold{kind}>& Out)",
                          broadphase_method_2d() if kind == "2D" else broadphase_method_3d())


def patch_solver(path: Path, kind: str) -> None:
    sig = f"\tvoid SolveVelocities_Internal(Toolbox::TVector<FManifold{kind}>& Manifolds)"
    insert_after_function(path, sig, solver_helpers(kind))
    text = path.read_text(encoding="utf-8-sig")
    old = '''\t\tfor (Toolbox::size_t ManifoldIndex = 0; ManifoldIndex < Manifolds.Size(); ++ManifoldIndex)\n\t\t{\n\t\t\tWarmStart_Internal(Manifolds[ManifoldIndex]);\n\t\t}\n\t\tSolveVelocities_Internal(Manifolds);\n\t\tStoreCache_Internal(Manifolds);'''
    count = text.count(old)
    if count < 2:
        raise RuntimeError(f"{path}: expected Step and SolveNow contact sequences, found {count}")
    text = text.replace(old, '\t\tSolveContacts_Internal(Manifolds);')
    path.write_text(text, encoding="utf-8")


def patch_apply_impulse(path: Path, kind: str) -> None:
    text = path.read_text(encoding="utf-8-sig")
    if kind == "2D":
        old = '''\t\tBodyA.Velocity += {static_cast<Toolbox::f32>(PushX * InverseMassA),\n\t\t                   static_cast<Toolbox::f32>(PushY * InverseMassA)};\n\t\tBodyA.AngularVelocity = static_cast<Toolbox::f32>(Toolbox::f64(BodyA.AngularVelocity) +\n\t\t                                                   (ArmAX * PushY - ArmAY * PushX) * InverseInertiaA);\n\t\tBodyB.Velocity += {static_cast<Toolbox::f32>(-PushX * InverseMassB),\n\t\t                   static_cast<Toolbox::f32>(-PushY * InverseMassB)};\n\t\tBodyB.AngularVelocity = static_cast<Toolbox::f32>(Toolbox::f64(BodyB.AngularVelocity) -\n\t\t                                                   (ArmBX * PushY - ArmBY * PushX) * InverseInertiaB);'''
        new = '''\t\tif (InverseMassA > 0 || InverseInertiaA > 0)\n\t\t{\n\t\t\tBodyA.Velocity += {static_cast<Toolbox::f32>(PushX * InverseMassA),\n\t\t\t                   static_cast<Toolbox::f32>(PushY * InverseMassA)};\n\t\t\tBodyA.AngularVelocity = static_cast<Toolbox::f32>(Toolbox::f64(BodyA.AngularVelocity) +\n\t\t\t                                                   (ArmAX * PushY - ArmAY * PushX) * InverseInertiaA);\n\t\t}\n\t\tif (InverseMassB > 0 || InverseInertiaB > 0)\n\t\t{\n\t\t\tBodyB.Velocity += {static_cast<Toolbox::f32>(-PushX * InverseMassB),\n\t\t\t                   static_cast<Toolbox::f32>(-PushY * InverseMassB)};\n\t\t\tBodyB.AngularVelocity = static_cast<Toolbox::f32>(Toolbox::f64(BodyB.AngularVelocity) -\n\t\t\t                                                   (ArmBX * PushY - ArmBY * PushX) * InverseInertiaB);\n\t\t}'''
    else:
        old = '''\t\t// 並進への反映。\n\t\tBodyA.Velocity += {static_cast<Toolbox::f32>(Push.X * InverseMassA),\n\t\t                   static_cast<Toolbox::f32>(Push.Y * InverseMassA),\n\t\t                   static_cast<Toolbox::f32>(Push.Z * InverseMassA)};\n\t\tBodyB.Velocity += {static_cast<Toolbox::f32>(-Push.X * InverseMassB),\n\t\t                   static_cast<Toolbox::f32>(-Push.Y * InverseMassB),\n\t\t                   static_cast<Toolbox::f32>(-Push.Z * InverseMassB)};\n\t\t// 腕とImpulseの外積をワールド逆慣性で角速度へ変換する。\n\t\tconst FVector3D MomentA = {ArmA.Y * Push.Z - ArmA.Z * Push.Y, ArmA.Z * Push.X - ArmA.X * Push.Z,\n\t\t                           ArmA.X * Push.Y - ArmA.Y * Push.X};\n\t\tconst FVector3D MomentB = {ArmB.Y * Push.Z - ArmB.Z * Push.Y, ArmB.Z * Push.X - ArmB.X * Push.Z,\n\t\t                           ArmB.X * Push.Y - ArmB.Y * Push.X};\n\t\tconst FQuaternionD QuaternionA = ToDouble_Internal(BodyA.Orientation);\n\t\tconst FQuaternionD QuaternionB = ToDouble_Internal(BodyB.Orientation);\n\t\tconst FVector3D DeltaA = TransformDiagonal_Internal(QuaternionA, InverseDiagonalA, MomentA);\n\t\tconst FVector3D DeltaB = TransformDiagonal_Internal(QuaternionB, InverseDiagonalB, MomentB);\n\t\tBodyA.AngularVelocity += {static_cast<Toolbox::f32>(DeltaA.X), static_cast<Toolbox::f32>(DeltaA.Y),\n\t\t                          static_cast<Toolbox::f32>(DeltaA.Z)};\n\t\tBodyB.AngularVelocity += {static_cast<Toolbox::f32>(-DeltaB.X), static_cast<Toolbox::f32>(-DeltaB.Y),\n\t\t                          static_cast<Toolbox::f32>(-DeltaB.Z)};'''
        new = '''\t\tconst bool bWritableA = InverseMassA > 0 || InverseDiagonalA.X > 0 || InverseDiagonalA.Y > 0 || InverseDiagonalA.Z > 0;\n\t\tconst bool bWritableB = InverseMassB > 0 || InverseDiagonalB.X > 0 || InverseDiagonalB.Y > 0 || InverseDiagonalB.Z > 0;\n\t\tif (bWritableA)\n\t\t{\n\t\t\tBodyA.Velocity += {static_cast<Toolbox::f32>(Push.X * InverseMassA),\n\t\t\t                   static_cast<Toolbox::f32>(Push.Y * InverseMassA),\n\t\t\t                   static_cast<Toolbox::f32>(Push.Z * InverseMassA)};\n\t\t\tconst FVector3D MomentA = {ArmA.Y * Push.Z - ArmA.Z * Push.Y, ArmA.Z * Push.X - ArmA.X * Push.Z,\n\t\t\t                           ArmA.X * Push.Y - ArmA.Y * Push.X};\n\t\t\tconst FQuaternionD QuaternionA = ToDouble_Internal(BodyA.Orientation);\n\t\t\tconst FVector3D DeltaA = TransformDiagonal_Internal(QuaternionA, InverseDiagonalA, MomentA);\n\t\t\tBodyA.AngularVelocity += {static_cast<Toolbox::f32>(DeltaA.X), static_cast<Toolbox::f32>(DeltaA.Y),\n\t\t\t                          static_cast<Toolbox::f32>(DeltaA.Z)};\n\t\t}\n\t\tif (bWritableB)\n\t\t{\n\t\t\tBodyB.Velocity += {static_cast<Toolbox::f32>(-Push.X * InverseMassB),\n\t\t\t                   static_cast<Toolbox::f32>(-Push.Y * InverseMassB),\n\t\t\t                   static_cast<Toolbox::f32>(-Push.Z * InverseMassB)};\n\t\t\tconst FVector3D MomentB = {ArmB.Y * Push.Z - ArmB.Z * Push.Y, ArmB.Z * Push.X - ArmB.X * Push.Z,\n\t\t\t                           ArmB.X * Push.Y - ArmB.Y * Push.X};\n\t\t\tconst FQuaternionD QuaternionB = ToDouble_Internal(BodyB.Orientation);\n\t\t\tconst FVector3D DeltaB = TransformDiagonal_Internal(QuaternionB, InverseDiagonalB, MomentB);\n\t\t\tBodyB.AngularVelocity += {static_cast<Toolbox::f32>(-DeltaB.X), static_cast<Toolbox::f32>(-DeltaB.Y),\n\t\t\t                          static_cast<Toolbox::f32>(-DeltaB.Z)};\n\t\t}'''
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: impulse write anchor count {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def integration_helpers(kind: str) -> str:
    if kind == "2D":
        return r'''\t// Dynamic Bodyの速度積分を独立slot単位で並列化する。
\tvoid IntegrateVelocitiesParallel_Internal(Toolbox::f64 Slice)
\t{
\t\tauto IntegrateOne = [&](Toolbox::size_t Index)
\t\t{
\t\t\tFBodyRecord2D& Record = Slots[Index];
\t\t\tif (Record.bAlive && Record.Type == EBodyType::Dynamic && !Record.bSleeping)
\t\t\t{
\t\t\t\tIntegrateVelocity_Internal(Record, Gravity, Slice);
\t\t\t}
\t\t};
\t\tToolbox::FJobSystem* Jobs = Execution.bParallelIntegration ? Execution.JobSystem : nullptr;
\t\tif (Jobs != nullptr && Jobs->GetExecutionThreadCount() > 1)
\t\t{
\t\t\tif (!Toolbox::ParallelFor(*Jobs, Slots.Size(), IntegrateOne, 32))
\t\t\t{
\t\t\t\tthrow Toolbox::FException("Parallel 2D velocity integration failed");
\t\t\t}
\t\t}
\t\telse
\t\t{
\t\t\tfor (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
\t\t\t{
\t\t\t\tIntegrateOne(Index);
\t\t\t}
\t\t}
\t}
\t// 離散更新の位置・姿勢積分を独立slot単位で並列化する。
\tvoid IntegratePositionsParallel_Internal(Toolbox::f64 Slice)
\t{
\t\tauto IntegrateOne = [&](Toolbox::size_t Index)
\t\t{
\t\t\tFBodyRecord2D& Record = Slots[Index];
\t\t\tif (!Record.bAlive)
\t\t\t{
\t\t\t\treturn;
\t\t\t}
\t\t\tif (Record.Type == EBodyType::Dynamic)
\t\t\t{
\t\t\t\tIntegratePosition_Internal(Record, Slice);
\t\t\t}
\t\t\telse if (Record.Type == EBodyType::Kinematic)
\t\t\t{
\t\t\t\tIntegrateKinematic_Internal(Record, Slice);
\t\t\t}
\t\t};
\t\tToolbox::FJobSystem* Jobs = Execution.bParallelIntegration ? Execution.JobSystem : nullptr;
\t\tif (Jobs != nullptr && Jobs->GetExecutionThreadCount() > 1)
\t\t{
\t\t\tif (!Toolbox::ParallelFor(*Jobs, Slots.Size(), IntegrateOne, 32))
\t\t\t{
\t\t\t\tthrow Toolbox::FException("Parallel 2D position integration failed");
\t\t\t}
\t\t}
\t\telse
\t\t{
\t\t\tfor (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
\t\t\t{
\t\t\t\tIntegrateOne(Index);
\t\t\t}
\t\t}
\t}'''
    return r'''\t// Dynamic Bodyの速度積分を独立slot単位で並列化する。
\tvoid IntegrateVelocitiesParallel_Internal(Toolbox::f64 Slice)
\t{
\t\tauto IntegrateOne = [&](Toolbox::size_t Index)
\t\t{
\t\t\tFBodyRecord3D& Record = Slots[Index];
\t\t\tif (Record.bAlive && Record.Type == EBodyType::Dynamic && !Record.bSleeping)
\t\t\t{
\t\t\t\tIntegrateVelocity_Internal(Record, Gravity, Slice);
\t\t\t}
\t\t};
\t\tToolbox::FJobSystem* Jobs = Execution.bParallelIntegration ? Execution.JobSystem : nullptr;
\t\tif (Jobs != nullptr && Jobs->GetExecutionThreadCount() > 1)
\t\t{
\t\t\tif (!Toolbox::ParallelFor(*Jobs, Slots.Size(), IntegrateOne, 32))
\t\t\t{
\t\t\t\tthrow Toolbox::FException("Parallel 3D velocity integration failed");
\t\t\t}
\t\t}
\t\telse
\t\t{
\t\t\tfor (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
\t\t\t{
\t\t\t\tIntegrateOne(Index);
\t\t\t}
\t\t}
\t}
\t// 離散更新の位置・姿勢積分を独立slot単位で並列化する。
\tvoid IntegratePositionsParallel_Internal(Toolbox::f64 Slice)
\t{
\t\tauto IntegrateOne = [&](Toolbox::size_t Index)
\t\t{
\t\t\tFBodyRecord3D& Record = Slots[Index];
\t\t\tif (!Record.bAlive)
\t\t\t{
\t\t\t\treturn;
\t\t\t}
\t\t\tif (Record.Type == EBodyType::Dynamic || Record.Type == EBodyType::Kinematic)
\t\t\t{
\t\t\t\tIntegratePosition_Internal(Record, Slice);
\t\t\t\tconst FVector3D Angular = {Record.AngularVelocity.X, Record.AngularVelocity.Y, Record.AngularVelocity.Z};
\t\t\t\tIntegrateOrientation_Internal(Record.Orientation, Angular, Slice);
\t\t\t}
\t\t};
\t\tToolbox::FJobSystem* Jobs = Execution.bParallelIntegration ? Execution.JobSystem : nullptr;
\t\tif (Jobs != nullptr && Jobs->GetExecutionThreadCount() > 1)
\t\t{
\t\t\tif (!Toolbox::ParallelFor(*Jobs, Slots.Size(), IntegrateOne, 32))
\t\t\t{
\t\t\t\tthrow Toolbox::FException("Parallel 3D position integration failed");
\t\t\t}
\t\t}
\t\telse
\t\t{
\t\t\tfor (Toolbox::size_t Index = 0; Index < Slots.Size(); ++Index)
\t\t\t{
\t\t\t\tIntegrateOne(Index);
\t\t\t}
\t\t}
\t}'''


def patch_integration(path: Path, kind: str) -> None:
    # Free velocity integrator is defined after FImpl in the base file, so add a forward declaration beside the existing position declaration.
    if kind == "2D":
        replace_once(path,
'''static void IntegratePosition_Internal(FBodyRecord2D& Record, Toolbox::f64 StepSeconds) noexcept;
// 指定速度どおりに運動させる。外力と減衰は適用しない。''',
'''static void IntegratePosition_Internal(FBodyRecord2D& Record, Toolbox::f64 StepSeconds) noexcept;
// Dynamicの速度だけを更新する。位置は呼び出し元が進める。
static void IntegrateVelocity_Internal(FBodyRecord2D& Record, Toolbox::FVector2 Gravity, Toolbox::f64 StepSeconds);
// 指定速度どおりに運動させる。外力と減衰は適用しない。''')
        anchor_sig = "\tvoid AdvanceAll_Internal(Toolbox::f64 Slice) noexcept"
    else:
        replace_once(path,
'''static void IntegratePosition_Internal(FBodyRecord3D& Record, Toolbox::f64 StepSeconds) noexcept;
// 剛体へ取り付けた形状と材質の登録。''',
'''static void IntegratePosition_Internal(FBodyRecord3D& Record, Toolbox::f64 StepSeconds) noexcept;
// Dynamicの速度と角速度を更新する。位置は呼び出し元が進める。
static void IntegrateVelocity_Internal(FBodyRecord3D& Record, Toolbox::FVector3 Gravity, Toolbox::f64 StepSeconds);
// 剛体へ取り付けた形状と材質の登録。''')
        anchor_sig = "\tvoid AdvanceAll_Internal(Toolbox::f64 Slice) noexcept"
    insert_after_function(path, anchor_sig, integration_helpers(kind))
    text = path.read_text(encoding="utf-8-sig")
    if kind == "2D":
        old_vel = '''\t\t// 力と重力を速度へ反映する。\n\t\tfor (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)\n\t\t{\n\t\t\tFBodyRecord2D& Record = m_pImpl->Slots[Index];\n\t\t\tif (!Record.bAlive || Record.Type != EBodyType::Dynamic || Record.bSleeping)\n\t\t\t{\n\t\t\t\tcontinue;\n\t\t\t}\n\t\t\tIntegrateVelocity_Internal(Record, m_pImpl->Gravity, Slice);\n\t\t}'''
        old_pos = '''\t\t// 更新後の速度で位置と姿勢を進める。\n\t\tfor (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)\n\t\t{\n\t\t\tFBodyRecord2D& Record = m_pImpl->Slots[Index];\n\t\t\tif (!Record.bAlive)\n\t\t\t{\n\t\t\t\tcontinue;\n\t\t\t}\n\t\t\tif (Record.Type == EBodyType::Dynamic)\n\t\t\t{\n\t\t\t\tIntegratePosition_Internal(Record, Slice);\n\t\t\t}\n\t\t\telse if (Record.Type == EBodyType::Kinematic)\n\t\t\t{\n\t\t\t\tIntegrateKinematic_Internal(Record, Slice);\n\t\t\t}\n\t\t}'''
    else:
        old_vel = '''\t\t// 力と重力を速度へ反映する。\n\t\tfor (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)\n\t\t{\n\t\t\tFBodyRecord3D& Record = m_pImpl->Slots[Index];\n\t\t\tif (!Record.bAlive || Record.Type != EBodyType::Dynamic || Record.bSleeping)\n\t\t\t{\n\t\t\t\tcontinue;\n\t\t\t}\n\t\t\tIntegrateVelocity_Internal(Record, m_pImpl->Gravity, Slice);\n\t\t}'''
        old_pos = '''\t\t// 更新後の速度で位置を進める。\n\t\tfor (Toolbox::size_t Index = 0; Index < m_pImpl->Slots.Size(); ++Index)\n\t\t{\n\t\t\tFBodyRecord3D& Record = m_pImpl->Slots[Index];\n\t\t\tif (!Record.bAlive)\n\t\t\t{\n\t\t\t\tcontinue;\n\t\t\t}\n\t\t\tif (Record.Type == EBodyType::Dynamic)\n\t\t\t{\n\t\t\t\tIntegratePosition_Internal(Record, Slice);\n\t\t\t\t// 指定角速度で姿勢を進める。\n\t\t\t\tconst FVector3D Angular = {Record.AngularVelocity.X, Record.AngularVelocity.Y,\n\t\t\t\t                           Record.AngularVelocity.Z};\n\t\t\t\tIntegrateOrientation_Internal(Record.Orientation, Angular, Slice);\n\t\t\t}\n\t\t\telse if (Record.Type == EBodyType::Kinematic)\n\t\t\t{\n\t\t\t\tIntegratePosition_Internal(Record, Slice);\n\t\t\t\t// 指定角速度で姿勢を進める。\n\t\t\t\tconst FVector3D Angular = {Record.AngularVelocity.X, Record.AngularVelocity.Y,\n\t\t\t\t                           Record.AngularVelocity.Z};\n\t\t\t\tIntegrateOrientation_Internal(Record.Orientation, Angular, Slice);\n\t\t\t}\n\t\t}'''
    if text.count(old_vel) != 1 or text.count(old_pos) != 1:
        raise RuntimeError(f"{path}: integration loop anchors vel={text.count(old_vel)} pos={text.count(old_pos)}")
    text = text.replace(old_vel, '\t\tm_pImpl->IntegrateVelocitiesParallel_Internal(Slice);', 1)
    text = text.replace(old_pos, '\t\tm_pImpl->IntegratePositionsParallel_Internal(Slice);', 1)
    path.write_text(text, encoding="utf-8")


def patch_execution_api(path: Path, kind: str) -> None:
    idtype = "FColliderId2D" if kind == "2D" else "FColliderId3D"
    marker = f'''bool FPhysicsWorld{kind}::IsColliderAlive({idtype} Id) const noexcept\n{{\n\treturn m_pImpl->FindCollider_Internal(Id) != nullptr;\n}}'''
    addition = marker + f'''\nvoid FPhysicsWorld{kind}::SetExecutionSettings(const FPhysicsExecutionSettings& Settings) noexcept\n{{\n\tm_pImpl->Execution = Settings;\n}}\nFPhysicsExecutionSettings FPhysicsWorld{kind}::GetExecutionSettings() const noexcept\n{{\n\treturn m_pImpl->Execution;\n}}\nFPhysicsExecutionDiagnostics FPhysicsWorld{kind}::GetExecutionDiagnostics() const noexcept\n{{\n\treturn m_pImpl->ExecutionDiagnostics;\n}}'''
    replace_once(path, marker, addition)
    validation = "\tif (SubSteps < 1 || SubSteps > 1024)\n\t{\n\t\tthrow Toolbox::FException(\"Invalid " + kind + " sub step count\");\n\t}"
    replacement = validation + "\n\tm_pImpl->ExecutionDiagnostics = {};\n\tm_pImpl->ExecutionDiagnostics.ExecutionThreadCount =\n\t    m_pImpl->Execution.JobSystem != nullptr ? m_pImpl->Execution.JobSystem->GetExecutionThreadCount() : 1;"
    replace_once(path, validation, replacement)


def patch_cpp(kind: str) -> None:
    path = ROOT / f"Source/Physics/Private/PhysicsWorld{kind}.cpp"
    common_cpp_prefix(path)
    patch_execution_fields(path, f"FSleepSettings{kind}")
    patch_next_world(path, kind)
    patch_generate(path, kind)
    patch_angular_wake(path, kind)
    patch_apply_impulse(path, kind)
    patch_solver(path, kind)
    patch_integration(path, kind)
    patch_execution_api(path, kind)


def main() -> int:
    if not (ROOT / ".git").exists():
        print("Run this script from the dxlib_framework repository root", file=sys.stderr)
        return 2
    head = run("git", "rev-parse", "HEAD")
    merge_base = subprocess.run(["git", "merge-base", "--is-ancestor", BASE, head], cwd=ROOT).returncode
    if merge_base != 0:
        print(f"Current HEAD {head} does not contain required base {BASE}", file=sys.stderr)
        return 2
    if run("git", "status", "--porcelain"):
        print("Working tree is not clean; refusing to mix Stage-2 changes", file=sys.stderr)
        return 2
    ensure_stage1()
    for relative in (
        "Source/Physics/Private/ParallelPhysicsCore.h",
        "Source/Physics/Private/ParallelPhysicsCore.cpp",
        "Source/Physics/Public/Dxf/PhysicsExecution.h",
        "Tests/Physics/ParallelTests.cpp",
        "Docs/Physics/ParallelExecution.md",
        "Docs/Tdd/Physics/Parallel/P1-red.log",
        "Docs/Tdd/Physics/Parallel/P1-green.log",
    ):
        copy_payload(relative)
    patch_cmake()
    patch_test_registration()
    patch_header("RigidBody2D")
    patch_header("RigidBody3D")
    patch_cpp("2D")
    patch_cpp("3D")
    print("Stage-1 + Stage-2 changes applied. Run repository validation before committing.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
