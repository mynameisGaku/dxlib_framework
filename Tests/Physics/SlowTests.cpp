// SPDX-License-Identifier: NOASSERTION
#include "TestCases.h"
#include "Dxf/PhysicsExecution.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/JobSystem.h"
#include "Toolbox/Platform.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 計測対象の固定刻み。
constexpr f64 SlowStepSeconds = 1.0 / 120.0;
// 60秒相当の手数。
constexpr size_t SlowStepCount = 7200;
// 評価する最後の10秒相当の手数。
constexpr size_t SlowTailSteps = 1200;
// 1m箱の半 extent。
constexpr f32 HalfBox = 0.5f;
// 積む箱の個数。
constexpr int32 BoxCount = 10;
// 60秒の内訳表示に使う刻みあたりの上限目安ではなく記録値。
struct FStepTiming
{
	// 刻み時間の記録。単位はナノ秒。
	TVector<uint64> Nanos;
};
// 刻み時間を記録する。
void RecordStep_Internal(FStepTiming& Timing, uint64 Nanos)
{
	Timing.Nanos.PushBack(Nanos);
}
// 中央値と95パーセンタイルと最大値を標準出力へ出す。
void ReportTiming_Internal(const char* Name, const FStepTiming& Timing, uint64 Pairs, uint64 Manifolds,
                           uint64 Islands, uint32 Lanes)
{
	TVector<uint64> Ordered(Timing.Nanos);
	for (size_t I = 1; I < Ordered.Size(); ++I)
	{
		const uint64 Key = Ordered[I];
		size_t Slot = I;
		while (Slot > 0 && Ordered[Slot - 1] > Key)
		{
			Ordered[Slot] = Ordered[Slot - 1];
			--Slot;
		}
		Ordered[Slot] = Key;
	}
	const uint64 Median = Ordered[Ordered.Size() / 2];
	const uint64 High = Ordered[Ordered.Size() * 95 / 100];
	printf("TIMING %s steps=%llu median_ns=%llu p95_ns=%llu max_ns=%llu pairs=%llu manifolds=%llu islands=%llu lanes=%u\n",
	       Name, (unsigned long long)Ordered.Size(), (unsigned long long)Median, (unsigned long long)High,
	       (unsigned long long)Ordered[Ordered.Size() - 1], (unsigned long long)Pairs, (unsigned long long)Manifolds,
	       (unsigned long long)Islands, (unsigned)Lanes);
}
FBodyId2D DropBox2D_Internal(FPhysicsWorld2D& World, f32 X, f32 Y)
{
	FBodyDescription2D Description;
	Description.Position = {X, Y};
	const FBodyId2D Body = World.CreateBody(Description);
	FColliderDescription2D Collider;
	FOrientedBox2D Shape;
	Shape.Center = {0, 0};
	Shape.HalfExtents = {HalfBox, HalfBox};
	Collider.Shape = Shape;
	Collider.Friction = 0.8f;
	World.AttachCollider(Body, Collider);
	return Body;
}
FBodyId3D DropBox3D_Internal(FPhysicsWorld3D& World, f32 X, f32 Y)
{
	FBodyDescription3D Description;
	Description.Position = {X, Y, 0};
	const FBodyId3D Body = World.CreateBody(Description);
	FColliderDescription3D Collider;
	FOBB Shape;
	Shape.Center = {0, 0, 0};
	Shape.HalfExtents = {HalfBox, HalfBox, HalfBox};
	Collider.Shape = Shape;
	Collider.Friction = 0.8f;
	World.AttachCollider(Body, Collider);
	return Body;
}
void StackStability2D_Internal(bool bSleep)
{
	FJobSystem Jobs(4);
	FPhysicsWorld2D World;
	FPhysicsExecutionSettings Execution;
	Execution.JobSystem = &Jobs;
	World.SetExecutionSettings(Execution);
	FSleepSettings2D Sleep = World.GetSleepSettings();
	Sleep.bEnabled = bSleep;
	World.SetSleepSettings(Sleep);
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId2D Floor = World.CreateBody(Ground);
	FColliderDescription2D FloorCollider;
	FOrientedBox2D FloorShape;
	FloorShape.Center = {0, -1};
	FloorShape.HalfExtents = {40, 1};
	FloorCollider.Shape = FloorShape;
	World.AttachCollider(Floor, FloorCollider);
	TVector<FBodyId2D> Bodies;
	for (int32 Index = 0; Index < BoxCount; ++Index)
	{
		Bodies.PushBack(DropBox2D_Internal(World, 0, HalfBox + Index * 1.001f));
	}
	FStepTiming Timing;
	for (size_t Step = 0; Step < SlowStepCount; ++Step)
	{
		const uint64 Begin = MonotonicNanoseconds();
		World.Step(SlowStepSeconds);
		RecordStep_Internal(Timing, MonotonicNanoseconds() - Begin);
	}
	const FPhysicsExecutionDiagnostics Diagnostics = World.GetExecutionDiagnostics();
	ReportTiming_Internal(bSleep ? "stack2d-sleep" : "stack2d-awake", Timing, Diagnostics.CandidatePairCount,
	                      Diagnostics.ManifoldCount, Diagnostics.IslandCount, Diagnostics.ExecutionThreadCount);
	f32 WorstPenetration = 0;
	f32 WorstDrift = 0;
	f32 WorstSpeed = 0;
	f32 WorstTilt = 0;
	for (size_t Step = 0; Step < SlowTailSteps; ++Step)
	{
		World.Step(SlowStepSeconds);
		for (int32 Index = 0; Index < BoxCount; ++Index)
		{
			const f32 ExpectedY = HalfBox + Index * 1.0f;
			const f32 Penetration = ExpectedY - World.GetPosition(Bodies[Index]).Y;
			const f32 Drift = Abs(World.GetPosition(Bodies[Index]).X);
			const f32 Speed = Abs(World.GetVelocity(Bodies[Index]).X) + Abs(World.GetVelocity(Bodies[Index]).Y);
			const f32 Tilt = Abs(World.GetAngle(Bodies[Index]));
			if (Penetration > WorstPenetration)
			{
				WorstPenetration = Penetration;
			}
			if (Drift > WorstDrift)
			{
				WorstDrift = Drift;
			}
			if (Speed > WorstSpeed)
			{
				WorstSpeed = Speed;
			}
			if (Tilt > WorstTilt)
			{
				WorstTilt = Tilt;
			}
		}
	}
	printf("STABILITY stack2d %s penetration=%.6f drift=%.6f speed=%.6f tilt=%.6f\n", bSleep ? "sleep" : "awake",
	       (double)WorstPenetration, (double)WorstDrift, (double)WorstSpeed, (double)WorstTilt);
	PHYSICS_REQUIRE(WorstPenetration <= 0.005f);
	PHYSICS_REQUIRE(WorstDrift <= 0.01f);
	PHYSICS_REQUIRE(WorstSpeed <= 0.02f);
	PHYSICS_REQUIRE(WorstTilt <= 0.05f);
	if (bSleep)
	{
		for (int32 Index = 0; Index < BoxCount; ++Index)
		{
			PHYSICS_REQUIRE(World.IsSleeping(Bodies[Index]));
		}
	}
}
void StackStability3D_Internal(bool bSleep)
{
	FJobSystem Jobs(4);
	FPhysicsWorld3D World;
	FPhysicsExecutionSettings Execution;
	Execution.JobSystem = &Jobs;
	World.SetExecutionSettings(Execution);
	FSleepSettings3D Sleep = World.GetSleepSettings();
	Sleep.bEnabled = bSleep;
	World.SetSleepSettings(Sleep);
	FBodyDescription3D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId3D Floor = World.CreateBody(Ground);
	FColliderDescription3D FloorCollider;
	FOBB FloorShape;
	FloorShape.Center = {0, -1, 0};
	FloorShape.HalfExtents = {40, 1, 40};
	FloorCollider.Shape = FloorShape;
	World.AttachCollider(Floor, FloorCollider);
	TVector<FBodyId3D> Bodies;
	for (int32 Index = 0; Index < BoxCount; ++Index)
	{
		Bodies.PushBack(DropBox3D_Internal(World, 0, HalfBox + Index * 1.001f));
	}
	FStepTiming Timing;
	for (size_t Step = 0; Step < SlowStepCount; ++Step)
	{
		const uint64 Begin = MonotonicNanoseconds();
		World.Step(SlowStepSeconds);
		RecordStep_Internal(Timing, MonotonicNanoseconds() - Begin);
	}
	const FPhysicsExecutionDiagnostics Diagnostics = World.GetExecutionDiagnostics();
	ReportTiming_Internal(bSleep ? "stack3d-sleep" : "stack3d-awake", Timing, Diagnostics.CandidatePairCount,
	                      Diagnostics.ManifoldCount, Diagnostics.IslandCount, Diagnostics.ExecutionThreadCount);
	f32 WorstPenetration = 0;
	f32 WorstDrift = 0;
	f32 WorstSpeed = 0;
	for (size_t Step = 0; Step < SlowTailSteps; ++Step)
	{
		World.Step(SlowStepSeconds);
		for (int32 Index = 0; Index < BoxCount; ++Index)
		{
			const f32 ExpectedY = HalfBox + Index * 1.0f;
			const f32 Penetration = ExpectedY - World.GetPosition(Bodies[Index]).Y;
			const f32 Drift = Abs(World.GetPosition(Bodies[Index]).X) + Abs(World.GetPosition(Bodies[Index]).Z);
			const f32 Speed = Abs(World.GetVelocity(Bodies[Index]).X) + Abs(World.GetVelocity(Bodies[Index]).Y) +
			                  Abs(World.GetVelocity(Bodies[Index]).Z);
			if (Penetration > WorstPenetration)
			{
				WorstPenetration = Penetration;
			}
			if (Drift > WorstDrift)
			{
				WorstDrift = Drift;
			}
			if (Speed > WorstSpeed)
			{
				WorstSpeed = Speed;
			}
		}
	}
	printf("STABILITY stack3d %s penetration=%.6f drift=%.6f speed=%.6f\n", bSleep ? "sleep" : "awake",
	       (double)WorstPenetration, (double)WorstDrift, (double)WorstSpeed);
	PHYSICS_REQUIRE(WorstPenetration <= 0.005f);
	PHYSICS_REQUIRE(WorstDrift <= 0.01f);
	PHYSICS_REQUIRE(WorstSpeed <= 0.02f);
	if (bSleep)
	{
		for (int32 Index = 0; Index < BoxCount; ++Index)
		{
			PHYSICS_REQUIRE(World.IsSleeping(Bodies[Index]));
		}
	}
}
void Stack2DSleep_Internal()
{
	StackStability2D_Internal(true);
}
void Stack2DAwake_Internal()
{
	StackStability2D_Internal(false);
}
void Stack3DSleep_Internal()
{
	StackStability3D_Internal(true);
}
void Stack3DAwake_Internal()
{
	StackStability3D_Internal(false);
}
const PhysicsTest::FCase Cases[] = {
    {"stacked boxes stay stable sleeping in 2D", Stack2DSleep_Internal},
    {"stacked boxes stay stable awake in 2D", Stack2DAwake_Internal},
    {"stacked boxes stay stable sleeping in 3D", Stack3DSleep_Internal},
    {"stacked boxes stay stable awake in 3D", Stack3DAwake_Internal},
};
} // namespace

const PhysicsTest::FCase* PhysicsTest::GetSlowCases(Toolbox::size_t& Count) noexcept
{
	Count = sizeof(Cases) / sizeof(Cases[0]);
	return Cases;
}
