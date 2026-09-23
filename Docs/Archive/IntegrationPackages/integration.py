"""Exact, bounded edits to the four inspected 15f5df4 Physics files."""
from __future__ import annotations

BASE_BLOBS = {
    'Source/Physics/Public/Dxf/RigidBody2D.h': '07745e1b69133b4de75c0f8b74dec81d8e440bcd',
    'Source/Physics/Public/Dxf/RigidBody3D.h': 'c0a3be9da8b76c73baf2774b8b2803029d7a0103',
    'Source/Physics/Private/Dxf/PhysicsWorld2D.cpp': '7210eaeee831fdbcf4e26f067bbff1dda389849d',
    'Source/Physics/Private/Dxf/PhysicsWorld3D.cpp': '1e85634580f0f0dceecc27e57059d1d1e9613dcb',
}

def edits(path: str) -> list[tuple[str, str]]:
    dimension = '2D' if '2D' in path else '3D'
    vector = 'Toolbox::FVector2' if dimension == '2D' else 'Toolbox::FVector3'
    rotation = 'Toolbox::f32' if dimension == '2D' else 'Toolbox::FQuaternion'
    angular = 'Toolbox::f32' if dimension == '2D' else 'Toolbox::FVector3'
    field = 'Angle' if dimension == '2D' else 'Orientation'
    if path.endswith('.h'):
        alias = f'''/**
 * {dimension} Worldの値所有Snapshot。形状の世代はCollider IDで識別する。
 */
using FPhysicsSnapshot{dimension} = TPhysicsSnapshot<FBodyId{dimension}, FColliderId{dimension},
    {vector}, {rotation}, {angular}, decltype(FColliderDescription{dimension}::Shape)>;
'''
        declaration = f'''
	/**
	 * 生存Body・Colliderを全件複製する。戻り値はWorld破棄後も保持できる。
	 * 明示呼出し時だけ採取し、上限超過・確保失敗は例外で通知する。
	 * Stepや他のWorld操作と並行実行しない。中断したStepの後は正常Step完了まで拒否する。
	 * StepIndexは正常完了したStep呼出し数であり、SubStepsや描画フレーム数ではない。
	 * 形状はローカル座標。現在の公開APIでは同一Collider ID中の形状は不変。
	 * @param Limits 生存Body・Colliderの最大保持件数。
	 */
	FPhysicsSnapshot{dimension} CaptureSnapshot(const FPhysicsSnapshotLimits& Limits = {{}}) const;
'''
        return [
            ('#include "Dxf/BodyType.h"', '#include "Dxf/BodyType.h"\n#include "Dxf/PhysicsSnapshot.h"'),
            (f'/**\n * 力・重力・Impulseで動く{"平面" if dimension == "2D" else "立体"}剛体を所有し、接触拘束を解く。\n * 単一スレッドで使用し、DxLibや描画を知らない。\n */\nclass FPhysicsWorld{dimension}\n',
             alias + f'/**\n * 力・重力・Impulseで動く{"平面" if dimension == "2D" else "立体"}剛体を所有し、接触拘束を解く。\n * 単一スレッドで使用し、DxLibや描画を知らない。\n */\nclass FPhysicsWorld{dimension}\n'),
            ('\tvoid Step(Toolbox::f64 DeltaSeconds, Toolbox::uint32 SubSteps = 1);',
             '\tvoid Step(Toolbox::f64 DeltaSeconds, Toolbox::uint32 SubSteps = 1);' + declaration),
        ]
    method = f'''// 登録配列から直接採取する。外部の観察登録一覧は使用しない。
FPhysicsSnapshot{dimension} FPhysicsWorld{dimension}::CaptureSnapshot(const FPhysicsSnapshotLimits& Limits) const
{{
	return PhysicsPrivate::CaptureSnapshot_Internal<FPhysicsSnapshot{dimension}>(
	    m_pImpl->World, m_pImpl->SnapshotState, m_pImpl->Slots, m_pImpl->Colliders, Limits,
	    [](const FBodyRecord{dimension}& Body, FPhysicsSnapshot{dimension}::FBody& Item)
	    {{
		    Item.Rotation = Body.{field};
	    }});
}}
'''
    torque = '0' if dimension == '2D' else '{}'
    tail = f'\t\tRecord.Torque = {torque};\n\t}}\n}}\n}} // namespace Dxf'
    return [
        (f'#include "Dxf/RigidBody{dimension}.h"',
         f'#include "Dxf/RigidBody{dimension}.h"\n#include "PhysicsSnapshotBuilder.h"'),
        ('\tFPhysicsExecutionDiagnostics ExecutionDiagnostics;',
         '\tFPhysicsExecutionDiagnostics ExecutionDiagnostics;\n\t// 明示的なSnapshot採取に渡すStep完了情報。\n\tPhysicsPrivate::FSnapshotStepState SnapshotState;'),
        (f'void FPhysicsWorld{dimension}::Step(Toolbox::f64 DeltaSeconds, Toolbox::uint32 SubSteps)\n',
         method + f'void FPhysicsWorld{dimension}::Step(Toolbox::f64 DeltaSeconds, Toolbox::uint32 SubSteps)\n'),
        ('\t// 一回の更新を等分割し、蓄積力は全分割で保持する。',
         '\t// 引数検証後に観測を開始し、更新途中の例外では採取を禁止する。\n'
         '\tPhysicsPrivate::FSnapshotStepGuard SnapshotStep(m_pImpl->SnapshotState, DeltaSeconds, SubSteps);\n'
         '\t// 一回の更新を等分割し、蓄積力は全分割で保持する。'),
        (tail, f'\t\tRecord.Torque = {torque};\n\t}}\n\tSnapshotStep.Complete();\n}}\n}} // namespace Dxf'),
    ]

def forward(path: str, text: str) -> str:
    for old, new in edits(path):
        if text.count(old) != 1:
            raise ValueError(f'Anchor is missing or ambiguous: {path}: {old[:70]!r}')
        text = text.replace(old, new, 1)
    return text

def reverse(path: str, text: str) -> str:
    for old, new in reversed(edits(path)):
        if text.count(new) != 1:
            raise ValueError(f'Applied anchor is missing or ambiguous: {path}')
        text = text.replace(new, old, 1)
    return text
