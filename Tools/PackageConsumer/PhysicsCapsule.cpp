// SPDX-License-Identifier: NOASSERTION
#include "PhysicsCapsule.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"

namespace
{
// 物理更新1回の秒数。
constexpr Toolbox::f64 StepSeconds = 1.0 / 60.0;

// 上面y=0の床に横倒しのカプセルを落として止め、問い合わせ・Sensor・BroadPhaseの経路を同じ手順で確かめる。
template <typename TWorld, typename TBody, typename TCollider, typename TVector, typename TCapsule, typename TBox>
Toolbox::int32 CheckCapsule(TVector (*At)(Toolbox::f32, Toolbox::f32), TCapsule Lying, TBox FloorShape,
                            Toolbox::int32 Code)
{
	TWorld World;
	TBody FloorDescription;
	FloorDescription.Type = Dxf::EBodyType::Static;
	FloorDescription.Position = At(0, -0.5f);
	const auto Floor = World.CreateBody(FloorDescription);
	TCollider FloorCollider;
	FloorCollider.Shape = FloorShape;
	World.AttachCollider(Floor, FloorCollider);
	TBody BodyDescription;
	BodyDescription.Position = At(0, 2);
	const auto Body = World.CreateBody(BodyDescription);
	TCollider CapsuleCollider;
	CapsuleCollider.Shape = Lying;
	const auto Capsule = World.AttachCollider(Body, CapsuleCollider);
	for (Toolbox::int32 Index = 0; Index < 180; ++Index)
	{
		World.Step(StepSeconds);
	}
	// 半径0.3のカプセルは中心がy=0.3で止まる。
	if (Toolbox::Abs(World.GetPosition(Body).Y - 0.3f) > 0.02f)
	{
		return Code;
	}
	// 胴体を上から: y=0.6付近で当たる。
	const auto Ray = World.RaycastClosest(At(0.2f, 3), At(0.2f, -1));
	if (!Ray || Ray->Collider != Capsule || Toolbox::Abs(Ray->Fraction - (3 - 0.6) / 4) > 0.01)
	{
		return Code + 1;
	}
	// カプセルの接触の問い合わせは床とカプセルを見つける。
	TCapsule Probe = Lying;
	Probe.Start = At(Lying.Start.X, 0.3f);
	Probe.End = At(Lying.End.X, 0.3f);
	if (World.QueryCapsuleContacts(Probe, 0.05).Count != 2)
	{
		return Code + 2;
	}
	// Sensorのカプセルと、重なるKinematicの剛体。Triggerの開始を出す。
	Dxf::FWorldEventSettings Events;
	Events.bEnabled = true;
	World.SetEventSettings(Events);
	TBody SensorDescription;
	SensorDescription.Type = Dxf::EBodyType::Static;
	SensorDescription.Position = At(5, 1);
	const auto SensorBody = World.CreateBody(SensorDescription);
	TCollider SensorCollider = CapsuleCollider;
	SensorCollider.Response = Dxf::EColliderResponse::Sensor;
	const auto Sensor = World.AttachCollider(SensorBody, SensorCollider);
	TBody VisitorDescription;
	VisitorDescription.Type = Dxf::EBodyType::Kinematic;
	VisitorDescription.Position = At(5, 1);
	const auto Visitor = World.CreateBody(VisitorDescription);
	World.AttachCollider(Visitor, CapsuleCollider);
	World.Step(StepSeconds);
	const auto& Batch = World.GetEventBatch();
	bool bTrigger = false;
	for (const auto& Event : Batch.Events)
	{
		bTrigger =
		    bTrigger || (Event.Kind == Dxf::EWorldEventKind::Trigger && Event.Phase == Dxf::EWorldEventPhase::Begin &&
		                 (Event.ColliderA == Sensor || Event.ColliderB == Sensor));
	}
	if (!bTrigger)
	{
		return Code + 3;
	}
	// BroadPhaseの経路: 離れた64個のカプセルを、索引あり（既定）と総当たりで1回進める。姿勢は同じで、調べる組は減る。
	TWorld Indexed;
	TWorld Reference;
	Reference.SetSolverBroadPhaseEnabled_Internal(false);
	TBody Sparse;
	for (Toolbox::int32 Index = 0; Index < 64; ++Index)
	{
		Sparse.Position = At(static_cast<Toolbox::f32>(Index % 8) * 4, static_cast<Toolbox::f32>(Index / 8) * 4);
		Indexed.AttachCollider(Indexed.CreateBody(Sparse), CapsuleCollider);
		Reference.AttachCollider(Reference.CreateBody(Sparse), CapsuleCollider);
	}
	Indexed.Step(StepSeconds);
	Reference.Step(StepSeconds);
	if (Indexed.GetExecutionDiagnostics().CandidatePairCount * 10 >=
	    Reference.GetExecutionDiagnostics().CandidatePairCount)
	{
		return Code + 4;
	}
	const auto IndexedSnapshot = Indexed.CaptureSnapshot();
	const auto ReferenceSnapshot = Reference.CaptureSnapshot();
	for (Toolbox::size_t Index = 0; Index < IndexedSnapshot.Bodies.Size(); ++Index)
	{
		if (IndexedSnapshot.Bodies[Index].Position != ReferenceSnapshot.Bodies[Index].Position)
		{
			return Code + 5;
		}
	}
	return 0;
}
Toolbox::FVector2 At2D(Toolbox::f32 X, Toolbox::f32 Y)
{
	return {X, Y};
}
Toolbox::FVector3 At3D(Toolbox::f32 X, Toolbox::f32 Y)
{
	return {X, Y, 0};
}
} // namespace

Toolbox::int32 RunPhysicsCapsule()
{
	const Toolbox::int32 Result2D =
	    CheckCapsule<Dxf::FPhysicsWorld2D, Dxf::FBodyDescription2D, Dxf::FColliderDescription2D>(
	        &At2D, Toolbox::FCapsule2D{{-0.6f, 0}, {0.6f, 0}, 0.3f}, Toolbox::FOrientedBox2D{{}, {20, 0.5f}, 0}, 341);
	if (Result2D != 0)
	{
		return Result2D;
	}
	return CheckCapsule<Dxf::FPhysicsWorld3D, Dxf::FBodyDescription3D, Dxf::FColliderDescription3D>(
	    &At3D, Toolbox::FCapsule{{-0.6f, 0, 0}, {0.6f, 0, 0}, 0.3f}, Toolbox::FOBB{{}, {20, 0.5f, 20}}, 351);
}
