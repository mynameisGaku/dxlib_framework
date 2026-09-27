// SPDX-License-Identifier: NOASSERTION
// 接触・Triggerのイベント（R2）。成功したStep単位の確定集合の差（Begin／Stay／End）を、実FPhysicsWorld2D／3Dで確かめる。
// 期待値は配置から決まる重なりの有無（解析値）と、イベントを使わない対照のWorldから求める。
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/JobSystem.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
constexpr f64 StepSeconds = 1.0 / 60.0;

template <typename F> bool Throws_Internal(F&& Run)
{
	try
	{
		Run();
	}
	catch (const FException&)
	{
		return true;
	}
	return false;
}

// 2D Worldの型と登録操作（Yが上）。
struct F2D
{
	using FWorld = FPhysicsWorld2D;
	using FBodyId = FBodyId2D;
	using FColliderId = FColliderId2D;
	using FVector = FVector2;
	using FBatch = FWorldEventBatch2D;
	static FVector At(f32 X, f32 Y)
	{
		return {X, Y};
	}
	static f32 Height(FVector Value)
	{
		return Value.Y;
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type)
	{
		FBodyDescription2D Description;
		Description.Type = Type;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static FColliderDescription2D Ball(f32 Radius, EColliderResponse Response = EColliderResponse::Solid)
	{
		FColliderDescription2D Description;
		Description.Shape = FCircle2D{{0, 0}, Radius};
		Description.Response = Response;
		return Description;
	}
	static FColliderDescription2D Box(f32 HalfX, f32 HalfY, EColliderResponse Response = EColliderResponse::Solid)
	{
		FColliderDescription2D Description;
		Description.Shape = FOrientedBox2D{{0, 0}, {HalfX, HalfY}, 0};
		Description.Response = Response;
		return Description;
	}
	static void Place(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, 0);
	}
};

// 3D Worldの型と登録操作（Yが上）。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FColliderId = FColliderId3D;
	using FVector = FVector3;
	using FBatch = FWorldEventBatch3D;
	static FVector At(f32 X, f32 Y)
	{
		return {X, Y, 0};
	}
	static f32 Height(FVector Value)
	{
		return Value.Y;
	}
	static FBodyId Body(FWorld& World, FVector Position, EBodyType Type)
	{
		FBodyDescription3D Description;
		Description.Type = Type;
		Description.Position = Position;
		return World.CreateBody(Description);
	}
	static FColliderDescription3D Ball(f32 Radius, EColliderResponse Response = EColliderResponse::Solid)
	{
		FColliderDescription3D Description;
		Description.Shape = FSphere{{0, 0, 0}, Radius};
		Description.Response = Response;
		return Description;
	}
	static FColliderDescription3D Box(f32 HalfX, f32 HalfY, EColliderResponse Response = EColliderResponse::Solid)
	{
		FColliderDescription3D Description;
		Description.Shape = FOBB{{0, 0, 0}, {HalfX, HalfY, HalfX}};
		Description.Response = Response;
		return Description;
	}
	static void Place(FWorld& World, FBodyId Body, FVector Position)
	{
		World.SetBodyTransform(Body, Position, FQuaternion{});
	}
};

// 有効化した設定。
FWorldEventSettings Enabled_Internal(uint32 MaxPairs = 64)
{
	FWorldEventSettings Settings;
	Settings.bEnabled = true;
	Settings.MaxPairs = MaxPairs;
	return Settings;
}

// バッチの中の、指定の種類・遷移のイベントの数。
template <typename TBatch> int32 Count_Internal(const TBatch& Batch, EWorldEventKind Kind, EWorldEventPhase Phase)
{
	int32 Count = 0;
	for (const auto& Event : Batch.Events)
	{
		Count += Event.Kind == Kind && Event.Phase == Phase ? 1 : 0;
	}
	return Count;
}

// 静止したSensorの箱へ、Kinematicの球を固定更新ごとに置き直して出入りさせる。Begin→Stay→Endは各1回。
template <typename T> void TriggerTransitions_Internal()
{
	typename T::FWorld World;
	World.SetEventSettings(Enabled_Internal());
	const auto Zone = T::Body(World, T::At(0, 0), EBodyType::Static);
	const auto ZoneCollider = World.AttachCollider(Zone, T::Box(1, 1, EColliderResponse::Sensor));
	const auto Mover = T::Body(World, T::At(-5, 0), EBodyType::Kinematic);
	const auto MoverCollider = World.AttachCollider(Mover, T::Ball(0.5f));
	World.Step(StepSeconds);
	const auto& Batch = World.GetEventBatch();
	PHYSICS_REQUIRE(Batch.bPublished && Batch.bReset && Batch.Events.IsEmpty() && Batch.StepIndex == 1);
	const uint64 FirstBatch = Batch.BatchId;
	// 境界に接する位置（距離0）は重なりに含む。
	T::Place(World, Mover, T::At(-1.5f, 0));
	World.Step(StepSeconds);
	PHYSICS_REQUIRE(World.GetEventBatch().Events.Size() == 1);
	const auto Begin = World.GetEventBatch().Events[0];
	PHYSICS_REQUIRE(Begin.Kind == EWorldEventKind::Trigger && Begin.Phase == EWorldEventPhase::Begin);
	PHYSICS_REQUIRE(Begin.ColliderA == ZoneCollider && Begin.ColliderB == MoverCollider && !Begin.Normal);
	PHYSICS_REQUIRE(!World.GetEventBatch().bReset && World.GetEventBatch().BatchId == FirstBatch + 1);
	// 読んでも消費しない。
	PHYSICS_REQUIRE(World.GetEventBatch().Events.Size() == 1 && World.GetEventBatch().BatchId == FirstBatch + 1);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		T::Place(World, Mover, T::At(0.2f * static_cast<f32>(Index), 0));
		World.Step(StepSeconds);
		PHYSICS_REQUIRE(World.GetEventBatch().Events.Size() == 1);
		PHYSICS_REQUIRE(World.GetEventBatch().Events[0].Phase == EWorldEventPhase::Stay);
		PHYSICS_REQUIRE(World.GetEventBatch().PairCount == 1);
	}
	T::Place(World, Mover, T::At(5, 0));
	World.Step(StepSeconds);
	PHYSICS_REQUIRE(World.GetEventBatch().Events.Size() == 1);
	const auto End = World.GetEventBatch().Events[0];
	PHYSICS_REQUIRE(End.Phase == EWorldEventPhase::End && End.EndReason == EWorldEventEndReason::Separated);
	World.Step(StepSeconds);
	PHYSICS_REQUIRE(World.GetEventBatch().Events.IsEmpty() && World.GetEventBatch().StepIndex == 7);
	// Sensorは押し返さないので、Kinematicを置いた位置のまま。
	PHYSICS_REQUIRE(World.GetPosition(Mover) == T::At(5, 0));
}

// 床に落ちて静止する球と箱。接触点が複数でもSolver反復があっても、Contactの組のBeginは1回で、静止中はStayが続く。
template <typename T> void ContactOnce_Internal()
{
	typename T::FWorld World;
	World.SetEventSettings(Enabled_Internal());
	const auto Floor = T::Body(World, T::At(0, 0), EBodyType::Static);
	const auto FloorCollider = World.AttachCollider(Floor, T::Box(10, 0.5f));
	const auto Ball = T::Body(World, T::At(-3, 2), EBodyType::Dynamic);
	(void)World.AttachCollider(Ball, T::Ball(0.5f));
	const auto Crate = T::Body(World, T::At(3, 2), EBodyType::Dynamic);
	(void)World.AttachCollider(Crate, T::Box(0.4f, 0.4f));
	int32 Begins = 0;
	int32 Ends = 0;
	int32 Stays = 0;
	for (int32 Index = 0; Index < 240; ++Index)
	{
		World.Step(StepSeconds);
		const auto& Batch = World.GetEventBatch();
		Begins += Count_Internal(Batch, EWorldEventKind::Contact, EWorldEventPhase::Begin);
		Ends += Count_Internal(Batch, EWorldEventKind::Contact, EWorldEventPhase::End);
		Stays += Count_Internal(Batch, EWorldEventKind::Contact, EWorldEventPhase::Stay);
		for (const auto& Event : Batch.Events)
		{
			PHYSICS_REQUIRE(Event.ColliderA == FloorCollider);
			// 法線は二つ目（落ちた物体）から一つ目（床）へ向く：下向き。
			PHYSICS_REQUIRE(Event.Normal && T::Height(*Event.Normal) < -0.9f);
		}
	}
	PHYSICS_REQUIRE(Begins == 2 && Ends == 0 && Stays > 300);
	PHYSICS_REQUIRE(World.IsSleeping(Ball) && World.IsSleeping(Crate));
	// 休止中も接触は続く（Stay）。
	PHYSICS_REQUIRE(Count_Internal(World.GetEventBatch(), EWorldEventKind::Contact, EWorldEventPhase::Stay) == 2);
}

// イベントの有効化は物理の数値経過を変えない（無効のWorldと位置・速度・休止がビット単位で一致）。
template <typename T> void NoNumericChange_Internal()
{
	typename T::FWorld On;
	typename T::FWorld Off;
	On.SetEventSettings(Enabled_Internal());
	typename T::FBodyId Bodies[2];
	typename T::FWorld* Worlds[2] = {&On, &Off};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto& World = *Worlds[Index];
		const auto Floor = T::Body(World, T::At(0, 0), EBodyType::Static);
		(void)World.AttachCollider(Floor, T::Box(10, 0.5f));
		(void)World.AttachCollider(Floor, T::Box(1, 1, EColliderResponse::Sensor));
		Bodies[Index] = T::Body(World, T::At(0.3f, 3), EBodyType::Dynamic);
		(void)World.AttachCollider(Bodies[Index], T::Box(0.4f, 0.3f));
	}
	for (int32 Step = 0; Step < 200; ++Step)
	{
		On.Step(StepSeconds);
		Off.Step(StepSeconds);
		PHYSICS_REQUIRE(On.GetPosition(Bodies[0]) == Off.GetPosition(Bodies[1]));
		PHYSICS_REQUIRE(On.GetVelocity(Bodies[0]) == Off.GetVelocity(Bodies[1]));
		PHYSICS_REQUIRE(On.IsSleeping(Bodies[0]) == Off.IsSleeping(Bodies[1]));
	}
	PHYSICS_REQUIRE(!Off.GetEventBatch().bPublished && Off.GetEventBatch().Events.IsEmpty());
	PHYSICS_REQUIRE(On.GetEventBatch().bPublished);
}

// 削除・取り外し・スロットの再使用。削除後の最初の成功したStepでEnd(Removed)、同じスロットの新しい組は別のBegin。
template <typename T> void Removal_Internal()
{
	typename T::FWorld World;
	World.SetEventSettings(Enabled_Internal());
	const auto Zone = T::Body(World, T::At(0, 0), EBodyType::Static);
	const auto ZoneCollider = World.AttachCollider(Zone, T::Box(2, 2, EColliderResponse::Sensor));
	const auto Mover = T::Body(World, T::At(0, 0), EBodyType::Kinematic);
	const auto First = World.AttachCollider(Mover, T::Ball(0.5f));
	World.Step(StepSeconds);
	PHYSICS_REQUIRE(Count_Internal(World.GetEventBatch(), EWorldEventKind::Trigger, EWorldEventPhase::Begin) == 1);
	// 取り外した直後に同じスロットへ新しいColliderを付ける（世代が違う別の組）。
	PHYSICS_REQUIRE(World.DetachCollider(First));
	const auto Second = World.AttachCollider(Mover, T::Ball(0.5f));
	PHYSICS_REQUIRE(Second.Index == First.Index && Second.Generation != First.Generation);
	World.Step(StepSeconds);
	const auto& Batch = World.GetEventBatch();
	PHYSICS_REQUIRE(Batch.Events.Size() == 2);
	PHYSICS_REQUIRE(Batch.Events[0].Phase == EWorldEventPhase::End && Batch.Events[0].ColliderB == First);
	PHYSICS_REQUIRE(Batch.Events[0].EndReason == EWorldEventEndReason::Removed);
	PHYSICS_REQUIRE(Batch.Events[1].Phase == EWorldEventPhase::Begin && Batch.Events[1].ColliderB == Second);
	// Bodyの削除。相手の値のIDは残り、現在のWorldでは解決できない。
	PHYSICS_REQUIRE(World.DestroyBody(Mover));
	World.Step(StepSeconds);
	PHYSICS_REQUIRE(World.GetEventBatch().Events.Size() == 1);
	const auto End = World.GetEventBatch().Events[0];
	PHYSICS_REQUIRE(End.Phase == EWorldEventPhase::End && End.EndReason == EWorldEventEndReason::Removed);
	PHYSICS_REQUIRE(End.ColliderA == ZoneCollider && End.ColliderB == Second && !World.IsColliderAlive(End.ColliderB));
	World.Step(StepSeconds);
	PHYSICS_REQUIRE(World.GetEventBatch().Events.IsEmpty());
}

// 区分・衝突フィルターの変更。ContactからTriggerはEnd(FilterChanged)→Begin、許さなくなった組はEnd(FilterChanged)。
template <typename T> void FilterChanges_Internal()
{
	typename T::FWorld World;
	World.SetEventSettings(Enabled_Internal());
	const auto Floor = T::Body(World, T::At(0, 0), EBodyType::Static);
	const auto FloorCollider = World.AttachCollider(Floor, T::Box(10, 0.5f));
	const auto Ball = T::Body(World, T::At(0, 1.0f), EBodyType::Dynamic);
	(void)World.AttachCollider(Ball, T::Ball(0.5f));
	for (int32 Index = 0; Index < 10; ++Index)
	{
		World.Step(StepSeconds);
	}
	PHYSICS_REQUIRE(Count_Internal(World.GetEventBatch(), EWorldEventKind::Contact, EWorldEventPhase::Stay) == 1);
	World.SetColliderResponse(FloorCollider, EColliderResponse::Sensor);
	World.Step(StepSeconds);
	const auto& Batch = World.GetEventBatch();
	PHYSICS_REQUIRE(Batch.Events.Size() == 2);
	PHYSICS_REQUIRE(Batch.Events[0].Kind == EWorldEventKind::Contact && Batch.Events[0].Phase == EWorldEventPhase::End);
	PHYSICS_REQUIRE(Batch.Events[0].EndReason == EWorldEventEndReason::FilterChanged);
	PHYSICS_REQUIRE(Batch.Events[1].Kind == EWorldEventKind::Trigger &&
	                Batch.Events[1].Phase == EWorldEventPhase::Begin);
	World.SetColliderCollisionFilter(FloorCollider, {2u, 2u});
	World.Step(StepSeconds);
	PHYSICS_REQUIRE(World.GetEventBatch().Events.Size() == 1);
	PHYSICS_REQUIRE(World.GetEventBatch().Events[0].EndReason == EWorldEventEndReason::FilterChanged);
}

// 上限を超えたStepは一部を落とさずイベントなしのbOverflowedにし、上限内へ戻ったStepで保持していた組との差を発行する。
template <typename T> void Overflow_Internal()
{
	typename T::FWorld World;
	World.SetEventSettings(Enabled_Internal(2));
	const auto Zone = T::Body(World, T::At(0, 0), EBodyType::Static);
	(void)World.AttachCollider(Zone, T::Box(5, 5, EColliderResponse::Sensor));
	typename T::FBodyId Movers[3];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Movers[Index] = T::Body(World, T::At(static_cast<f32>(Index) - 1, 0), EBodyType::Kinematic);
		(void)World.AttachCollider(Movers[Index], T::Ball(0.3f));
	}
	T::Place(World, Movers[2], T::At(20, 0));
	World.Step(StepSeconds);
	PHYSICS_REQUIRE(World.GetEventBatch().Events.Size() == 2 && !World.GetEventBatch().bOverflowed);
	T::Place(World, Movers[2], T::At(1, 0));
	World.Step(StepSeconds);
	const auto& Batch = World.GetEventBatch();
	PHYSICS_REQUIRE(Batch.bPublished && Batch.bOverflowed && Batch.RequiredPairs == 3 && Batch.Events.IsEmpty());
	PHYSICS_REQUIRE(Batch.PairCount == 2);
	// 上限内へ戻す：0番を外へ出す。保持していた集合（0,1）との差でEnd(0)とBegin(2)、Stay(1)。
	T::Place(World, Movers[0], T::At(-20, 0));
	World.Step(StepSeconds);
	PHYSICS_REQUIRE(!World.GetEventBatch().bOverflowed && World.GetEventBatch().Events.Size() == 3);
	PHYSICS_REQUIRE(Count_Internal(World.GetEventBatch(), EWorldEventKind::Trigger, EWorldEventPhase::End) == 1);
	PHYSICS_REQUIRE(Count_Internal(World.GetEventBatch(), EWorldEventKind::Trigger, EWorldEventPhase::Begin) == 1);
	PHYSICS_REQUIRE(Count_Internal(World.GetEventBatch(), EWorldEventKind::Trigger, EWorldEventPhase::Stay) == 1);
}

// 途中で失敗したStepは新しいバッチを発行せず、前回のバッチも発行済みに見せない。次の成功したStepは最後の成功と比べる。
template <typename T> void FailedStep_Internal()
{
	typename T::FWorld World;
	World.SetEventSettings(Enabled_Internal());
	const auto Zone = T::Body(World, T::At(0, 0), EBodyType::Static);
	(void)World.AttachCollider(Zone, T::Box(1, 1, EColliderResponse::Sensor));
	const auto Mover = T::Body(World, T::At(0, 0), EBodyType::Dynamic);
	(void)World.AttachCollider(Mover, T::Ball(0.3f));
	World.SetGravity(T::At(0, 0));
	World.Step(StepSeconds);
	const uint64 Published = World.GetEventBatch().BatchId;
	PHYSICS_REQUIRE(World.GetEventBatch().Events.Size() == 1);
	FJobSystem Jobs(2);
	Jobs.Shutdown();
	FPhysicsExecutionSettings Broken;
	Broken.JobSystem = &Jobs;
	World.SetExecutionSettings(Broken);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.Step(StepSeconds);
	    }));
	PHYSICS_REQUIRE(!World.GetEventBatch().bPublished && World.GetEventBatch().Events.IsEmpty());
	PHYSICS_REQUIRE(World.GetEventBatch().BatchId == Published);
	World.SetExecutionSettings({});
	World.Step(StepSeconds);
	// 最後に成功したStepと同じ集合なのでStay。
	PHYSICS_REQUIRE(World.GetEventBatch().bPublished && World.GetEventBatch().BatchId == Published + 1);
	PHYSICS_REQUIRE(World.GetEventBatch().Events.Size() == 1);
	PHYSICS_REQUIRE(World.GetEventBatch().Events[0].Phase == EWorldEventPhase::Stay);
	PHYSICS_REQUIRE(World.GetEventBatch().StepIndex == 2);
}

// 設定の検証・変更・無効化。有効化のやり直しではbResetで現在の組をBeginにする。Static同士・同じBodyは組にならない。
template <typename T> void SettingsAndScope_Internal()
{
	typename T::FWorld World;
	FWorldEventSettings Bad = Enabled_Internal(0);
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.SetEventSettings(Bad);
	    }));
	Bad = Enabled_Internal();
	Bad.ContactMargin = -1;
	PHYSICS_REQUIRE(Throws_Internal(
	    [&]
	    {
		    World.SetEventSettings(Bad);
	    }));
	PHYSICS_REQUIRE(!World.GetEventSettings().bEnabled);
	// Static同士（Sensorを含む）と、同じBodyのColliderは組にならない。
	const auto Zone = T::Body(World, T::At(0, 0), EBodyType::Static);
	(void)World.AttachCollider(Zone, T::Box(1, 1, EColliderResponse::Sensor));
	(void)World.AttachCollider(Zone, T::Box(1, 1));
	const auto Wall = T::Body(World, T::At(0.5f, 0), EBodyType::Static);
	(void)World.AttachCollider(Wall, T::Box(1, 1));
	const auto Mover = T::Body(World, T::At(0, 0), EBodyType::Kinematic);
	(void)World.AttachCollider(Mover, T::Ball(0.3f));
	(void)World.AttachCollider(Mover, T::Ball(0.3f, EColliderResponse::Sensor));
	World.SetEventSettings(Enabled_Internal());
	PHYSICS_REQUIRE(World.GetEventSettings().bEnabled && World.GetEventSettings().MaxPairs == 64);
	World.Step(StepSeconds);
	// Kinematicの2つのColliderと、Staticの3つの組のうち、Sensorを含む組だけ（Kinematic同士・Kinematic対StaticのSolidは対象外）。
	// ZoneのSensor×Kinematic2、ZoneのSolid×KinematicのSensor、WallのSolid×KinematicのSensor。
	PHYSICS_REQUIRE(World.GetEventBatch().bReset);
	PHYSICS_REQUIRE(Count_Internal(World.GetEventBatch(), EWorldEventKind::Trigger, EWorldEventPhase::Begin) == 4);
	PHYSICS_REQUIRE(World.GetEventBatch().Events.Size() == 4);
	World.SetEventSettings(Enabled_Internal(32));
	PHYSICS_REQUIRE(!World.GetEventBatch().bPublished);
	World.Step(StepSeconds);
	PHYSICS_REQUIRE(World.GetEventBatch().bReset && World.GetEventBatch().Events.Size() == 4);
	World.SetEventSettings({});
	World.Step(StepSeconds);
	PHYSICS_REQUIRE(!World.GetEventBatch().bPublished && World.GetEventBatch().Events.IsEmpty());
}

// 散らばったSensorの円・球の組を、中心間距離と半径の和の解析値で総当たりに求めた組と比べる（BroadPhaseの取りこぼし・重複なし）。
template <typename T> void MatchesBruteForce_Internal()
{
	typename T::FWorld World;
	World.SetEventSettings(Enabled_Internal(4096));
	constexpr int32 Count = 60;
	typename T::FBodyId Bodies[Count];
	typename T::FColliderId Colliders[Count];
	f32 Radius[Count];
	uint32 Seed = 12345u;
	auto Next = [&Seed]()
	{
		Seed = Seed * 1664525u + 1013904223u;
		return static_cast<f32>((Seed >> 8) & 0xffffu) / 65535.0f;
	};
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Radius[Index] = 0.2f + Next() * 0.6f;
		Bodies[Index] = T::Body(World, T::At(Next() * 10, Next() * 10), EBodyType::Kinematic);
		Colliders[Index] = World.AttachCollider(Bodies[Index], T::Ball(Radius[Index], EColliderResponse::Sensor));
	}
	for (int32 Round = 0; Round < 3; ++Round)
	{
		World.Step(StepSeconds);
		int32 Expected = 0;
		for (int32 First = 0; First < Count; ++First)
		{
			for (int32 Second = First + 1; Second < Count; ++Second)
			{
				const auto Delta = World.GetPosition(Bodies[First]) - World.GetPosition(Bodies[Second]);
				const f64 Distance = Sqrt(static_cast<f64>(Dot(Delta, Delta)));
				const f64 Gap = Distance - (static_cast<f64>(Radius[First]) + Radius[Second]);
				PHYSICS_REQUIRE(Abs(Gap) > 1e-4);
				Expected += Gap < 0 ? 1 : 0;
			}
		}
		PHYSICS_REQUIRE(World.GetEventBatch().PairCount == static_cast<uint32>(Expected));
		int32 Seen = 0;
		for (const auto& Event : World.GetEventBatch().Events)
		{
			PHYSICS_REQUIRE(Event.ColliderA.Index < Event.ColliderB.Index);
			Seen += Event.Phase != EWorldEventPhase::End ? 1 : 0;
		}
		PHYSICS_REQUIRE(Seen == Expected);
		// 半数を動かす。
		for (int32 Index = 0; Index < Count; Index += 2)
		{
			T::Place(World, Bodies[Index], T::At(Next() * 10, Next() * 10));
		}
	}
	(void)Colliders;
}

const PhysicsTest::FCase Cases_Internal[] = {
    {"2D trigger begin stay end once per step", &TriggerTransitions_Internal<F2D>},
    {"3D trigger begin stay end once per step", &TriggerTransitions_Internal<F3D>},
    {"2D contact begins once for many points", &ContactOnce_Internal<F2D>},
    {"3D contact begins once for many points", &ContactOnce_Internal<F3D>},
    {"2D events keep numeric results", &NoNumericChange_Internal<F2D>},
    {"3D events keep numeric results", &NoNumericChange_Internal<F3D>},
    {"2D events end removed and reused slots", &Removal_Internal<F2D>},
    {"3D events end removed and reused slots", &Removal_Internal<F3D>},
    {"2D events follow response and filter changes", &FilterChanges_Internal<F2D>},
    {"3D events follow response and filter changes", &FilterChanges_Internal<F3D>},
    {"2D events report overflow without dropping", &Overflow_Internal<F2D>},
    {"3D events report overflow without dropping", &Overflow_Internal<F3D>},
    {"2D events skip failed steps", &FailedStep_Internal<F2D>},
    {"3D events skip failed steps", &FailedStep_Internal<F3D>},
    {"2D event settings and pair scope", &SettingsAndScope_Internal<F2D>},
    {"3D event settings and pair scope", &SettingsAndScope_Internal<F3D>},
    {"2D event pairs match brute force", &MatchesBruteForce_Internal<F2D>},
    {"3D event pairs match brute force", &MatchesBruteForce_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetWorldEventCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
