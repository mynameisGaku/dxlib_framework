// SPDX-License-Identifier: NOASSERTION
#include "WorldEventAllocationFaultTests.h"
#include "QueryIndexTestSupport.h"
#include "../../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"

namespace PhysicsTest
{
namespace
{
// 一つのStepへ次の確保失敗を注入し、確保そのものが起きなかったかを返す。
template <typename TWorld> bool StepWithoutAllocation(TWorld& World)
{
	Toolbox::Testing::SetAllocationFailureCountdown(0);
	bool bSucceeded = true;
	try
	{
		World.Step(1.0 / 60.0);
	}
	catch (const Toolbox::FException&)
	{
		bSucceeded = false;
	}
	const bool bInjected = Toolbox::Testing::WasAllocationFailureInjected();
	Toolbox::Testing::SetAllocationFailureCountdown(-1);
	if (!bSucceeded)
	{
		// Redでも次の独立な確認へ進めるよう、失敗したWorldを正常Stepで復帰する。
		World.Step(1.0 / 60.0);
	}
	return bSucceeded && !bInjected;
}

// 成功・失敗を既存の隔離exeと同じ形式で出力する。
Toolbox::int32 Check(bool bCondition, const char* Dimension, const char* Name)
{
	printf("%s %s event allocation %s\n", bCondition ? "PASS" : "FAIL", Dimension, Name);
	fflush(stdout);
	return bCondition ? 0 : 1;
}

// 候補の走査順に依存せず、イベントがColliderスロットの正準順かを確認する。
template <typename TBatch> bool Canonical(const TBatch& Batch)
{
	for (Toolbox::size_t Index = 0; Index < Batch.Events.Size(); ++Index)
	{
		const auto& Event = Batch.Events[Index];
		if (Event.ColliderA.Index >= Event.ColliderB.Index)
		{
			return false;
		}
		if (Index > 0)
		{
			const auto& Previous = Batch.Events[Index - 1];
			if (Previous.ColliderA.Index > Event.ColliderA.Index ||
			    (Previous.ColliderA.Index == Event.ColliderA.Index && Previous.ColliderB.Index > Event.ColliderB.Index))
			{
				return false;
			}
		}
	}
	return true;
}

// Solverを使わないKinematic Sensorで、イベント生成由来の確保を直接検出する。
template <typename T> Toolbox::int32 RunDimension()
{
	Toolbox::int32 Failures = 0;
	typename T::FWorld World;
	Dxf::FWorldEventSettings Settings;
	Settings.bEnabled = true;
	Settings.MaxPairs = 256;
	World.SetEventSettings(Settings);
	// 有効化後に登録する経路。20Colliderの密集時は190組になる。
	Toolbox::TVector<typename T::FBody> Bodies;
	Toolbox::TVector<typename T::FCollider> Colliders;
	typename T::FBodyDescription Body;
	Body.Type = Dxf::EBodyType::Kinematic;
	auto Shape = T::Ball(T::At(0, 0, 0), 0.5f);
	Shape.Response = Dxf::EColliderResponse::Sensor;
	for (Toolbox::int32 Index = 0; Index < 4; ++Index)
	{
		const auto Id = World.CreateBody(Body);
		Bodies.PushBack(Id);
		Colliders.PushBack(World.AttachCollider(Id, Shape));
	}
	bool bNoAllocation = StepWithoutAllocation(World);
	Failures += Check(bNoAllocation && World.GetEventBatch().Events.Size() == 6 && Canonical(World.GetEventBatch()),
	                  T::Name, "first enabled step");
	bNoAllocation = StepWithoutAllocation(World);
	Failures += Check(bNoAllocation && World.GetEventBatch().Events.Size() == 6 &&
	                      World.GetEventBatch().Events[0].Phase == Dxf::EWorldEventPhase::Stay,
	                  T::Name, "warm stay step");
	// 同じスロットを再登録しても古い世代のEndと新しいBeginを混ぜない。
	const auto OldCollider = Colliders.Back();
	World.DetachCollider(OldCollider);
	Colliders.Back() = World.AttachCollider(Bodies.Back(), Shape);
	bNoAllocation = StepWithoutAllocation(World);
	Toolbox::int32 Removed = 0;
	Toolbox::int32 Began = 0;
	for (const auto& Event : World.GetEventBatch().Events)
	{
		Removed += Event.Phase == Dxf::EWorldEventPhase::End && Event.EndReason == Dxf::EWorldEventEndReason::Removed &&
		                   Event.ColliderB == OldCollider
		               ? 1
		               : 0;
		Began += Event.Phase == Dxf::EWorldEventPhase::Begin && Event.ColliderB == Colliders.Back() ? 1 : 0;
	}
	Failures += Check(bNoAllocation && Removed == 3 && Began == 3 && Canonical(World.GetEventBatch()), T::Name,
	                  "reused collider generation");
	// 追加登録直後。Xの順はCollider番号の順とは逆にする。
	for (Toolbox::int32 Index = 4; Index < 20; ++Index)
	{
		Body.Position = T::At(static_cast<Toolbox::f32>(100 - Index * 3), 0, 0);
		const auto Id = World.CreateBody(Body);
		Bodies.PushBack(Id);
		Colliders.PushBack(World.AttachCollider(Id, Shape));
	}
	bNoAllocation = StepWithoutAllocation(World);
	Failures += Check(bNoAllocation && World.GetEventBatch().PairCount == 6 && Canonical(World.GetEventBatch()),
	                  T::Name, "registration growth");
	// 保持容量内で真の接触組だけが急増しても、候補配列の追加確保をしない。
	for (const auto Id : Bodies)
	{
		T::Place(World, Id, T::At(0, 0, 0));
	}
	bNoAllocation = StepWithoutAllocation(World);
	Failures += Check(bNoAllocation && World.GetEventBatch().PairCount == 190 && Canonical(World.GetEventBatch()),
	                  T::Name, "dense pair growth");
	// 上限超過でも全ての必要組を数え、部分イベントを発行しない。
	Settings.MaxPairs = 3;
	World.SetEventSettings(Settings);
	bNoAllocation = StepWithoutAllocation(World);
	Failures += Check(bNoAllocation && World.GetEventBatch().bPublished && World.GetEventBatch().bOverflowed &&
	                      World.GetEventBatch().RequiredPairs == 190 && World.GetEventBatch().Events.IsEmpty(),
	                  T::Name, "overflow counts all pairs");
	for (Toolbox::size_t Index = 2; Index < Bodies.Size(); ++Index)
	{
		T::Place(World, Bodies[Index], T::At(static_cast<Toolbox::f32>(Index * 3), 0, 0));
	}
	bNoAllocation = StepWithoutAllocation(World);
	Failures += Check(bNoAllocation && !World.GetEventBatch().bOverflowed && World.GetEventBatch().PairCount == 1 &&
	                      World.GetEventBatch().Events.Size() == 1 &&
	                      World.GetEventBatch().Events[0].Phase == Dxf::EWorldEventPhase::Begin,
	                  T::Name, "recovery after overflow");
	// 設定中の確保失敗では、前の設定と発行済みバッチを維持する。
	const Toolbox::uint64 PreviousBatch = World.GetEventBatch().BatchId;
	Settings.MaxPairs = 512;
	bool bThrown = false;
	Toolbox::Testing::SetAllocationFailureCountdown(0);
	try
	{
		World.SetEventSettings(Settings);
	}
	catch (const Toolbox::FException&)
	{
		bThrown = true;
	}
	const bool bInjected = Toolbox::Testing::WasAllocationFailureInjected();
	Toolbox::Testing::SetAllocationFailureCountdown(-1);
	Failures += Check(bThrown && bInjected && World.GetEventSettings().MaxPairs == 3 &&
	                      World.GetEventBatch().BatchId == PreviousBatch && World.GetEventBatch().bPublished,
	                  T::Name, "configuration failure keeps published state");
	return Failures;
}
} // namespace

Toolbox::int32 RunWorldEventAllocationChecks()
{
	return RunDimension<QueryIndexTest::F2D>() + RunDimension<QueryIndexTest::F3D>();
}
} // namespace PhysicsTest
