// SPDX-License-Identifier: NOASSERTION
#include "TestCases.h"
#include "Dxf/PhysicsExecution.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Dxf/ParallelPhysicsCore.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
using namespace Dxf::PhysicsPrivate;

FColliderDescription2D Circle2D_Internal()
{
	FColliderDescription2D Collider;
	FCircle2D Shape;
	Shape.Radius = 0.5f;
	Collider.Shape = Shape;
	Collider.Friction = 0.5f;
	return Collider;
}

FColliderDescription2D Floor2D_Internal()
{
	FColliderDescription2D Collider;
	FOrientedBox2D Shape;
	Shape.Center = {0, -1};
	Shape.HalfExtents = {40, 1};
	Collider.Shape = Shape;
	Collider.Friction = 0.5f;
	return Collider;
}

FColliderDescription3D Sphere3D_Internal()
{
	FColliderDescription3D Collider;
	FSphere Shape;
	Shape.Radius = 0.5f;
	Collider.Shape = Shape;
	Collider.Friction = 0.5f;
	return Collider;
}

FColliderDescription3D Floor3D_Internal()
{
	FColliderDescription3D Collider;
	FOBB Shape;
	Shape.Center = {0, -1, 0};
	Shape.HalfExtents = {40, 1, 40};
	Collider.Shape = Shape;
	Collider.Friction = 0.5f;
	return Collider;
}

struct FState2D
{
	FVector2 Position;
	FVector2 Velocity;
	f32 Angle = 0;
	f32 AngularVelocity = 0;
};

struct FState3D
{
	FVector3 Position;
	FVector3 Velocity;
	FQuaternion Orientation;
	FVector3 AngularVelocity;
};

TVector<FState2D> Run2D_Internal(FJobSystem* Jobs, FPhysicsExecutionDiagnostics& Diagnostics)
{
	FPhysicsWorld2D World;
	FPhysicsExecutionSettings Execution;
	Execution.JobSystem = Jobs;
	World.SetExecutionSettings(Execution);
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId2D Floor = World.CreateBody(Ground);
	World.AttachCollider(Floor, Floor2D_Internal());
	TVector<FBodyId2D> Bodies;
	for (int32 Index = 0; Index < 24; ++Index)
	{
		FBodyDescription2D Description;
		Description.Position = {static_cast<f32>(Index * 2 - 23), 0.5f + static_cast<f32>(Index % 3) * 0.02f};
		const FBodyId2D Body = World.CreateBody(Description);
		World.AttachCollider(Body, Circle2D_Internal());
		Bodies.PushBack(Body);
	}
	for (int32 Step = 0; Step < 300; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	Diagnostics = World.GetExecutionDiagnostics();
	TVector<FState2D> Result;
	Result.Reserve(Bodies.Size());
	for (size_t Index = 0; Index < Bodies.Size(); ++Index)
	{
		FState2D State;
		State.Position = World.GetPosition(Bodies[Index]);
		State.Velocity = World.GetVelocity(Bodies[Index]);
		State.Angle = World.GetAngle(Bodies[Index]);
		State.AngularVelocity = World.GetAngularVelocity(Bodies[Index]);
		Result.PushBack(State);
	}
	return Result;
}

TVector<FState3D> Run3D_Internal(FJobSystem* Jobs, FPhysicsExecutionDiagnostics& Diagnostics)
{
	FPhysicsWorld3D World;
	FPhysicsExecutionSettings Execution;
	Execution.JobSystem = Jobs;
	World.SetExecutionSettings(Execution);
	FBodyDescription3D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId3D Floor = World.CreateBody(Ground);
	World.AttachCollider(Floor, Floor3D_Internal());
	TVector<FBodyId3D> Bodies;
	for (int32 Index = 0; Index < 18; ++Index)
	{
		FBodyDescription3D Description;
		Description.Position = {static_cast<f32>(Index * 2 - 17), 0.5f + static_cast<f32>(Index % 2) * 0.02f, 0};
		const FBodyId3D Body = World.CreateBody(Description);
		World.AttachCollider(Body, Sphere3D_Internal());
		Bodies.PushBack(Body);
	}
	for (int32 Step = 0; Step < 300; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	Diagnostics = World.GetExecutionDiagnostics();
	TVector<FState3D> Result;
	Result.Reserve(Bodies.Size());
	for (size_t Index = 0; Index < Bodies.Size(); ++Index)
	{
		FState3D State;
		State.Position = World.GetPosition(Bodies[Index]);
		State.Velocity = World.GetVelocity(Bodies[Index]);
		State.Orientation = World.GetOrientation(Bodies[Index]);
		State.AngularVelocity = World.GetAngularVelocity(Bodies[Index]);
		Result.PushBack(State);
	}
	return Result;
}
void NullExecutionMatchesSingleLane_Internal()
{
	FJobSystem One(1);
	FPhysicsExecutionDiagnostics BorrowedDiagnostics;
	const TVector<FState2D> Borrowed = Run2D_Internal(&One, BorrowedDiagnostics);
	FPhysicsExecutionDiagnostics LegacyDiagnostics;
	const TVector<FState2D> Legacy = Run2D_Internal(nullptr, LegacyDiagnostics);
	PHYSICS_REQUIRE(Borrowed.Size() == Legacy.Size());
	for (size_t Index = 0; Index < Borrowed.Size(); ++Index)
	{
		PHYSICS_REQUIRE(Borrowed[Index].Position == Legacy[Index].Position);
		PHYSICS_REQUIRE(Borrowed[Index].Velocity == Legacy[Index].Velocity);
		PHYSICS_REQUIRE(Borrowed[Index].Angle == Legacy[Index].Angle);
		PHYSICS_REQUIRE(Borrowed[Index].AngularVelocity == Legacy[Index].AngularVelocity);
	}
}

void WorkerCountDoesNotChange2DResult_Internal()
{

	FJobSystem One(1);
	FJobSystem Four(4);
	FPhysicsExecutionDiagnostics SingleDiagnostics;
	FPhysicsExecutionDiagnostics ParallelDiagnostics;
	const TVector<FState2D> Single = Run2D_Internal(&One, SingleDiagnostics);
	const TVector<FState2D> Parallel = Run2D_Internal(&Four, ParallelDiagnostics);
	PHYSICS_REQUIRE(Single.Size() == Parallel.Size());
	for (size_t Index = 0; Index < Single.Size(); ++Index)
	{
		PHYSICS_REQUIRE(Single[Index].Position == Parallel[Index].Position);
		PHYSICS_REQUIRE(Single[Index].Velocity == Parallel[Index].Velocity);
		PHYSICS_REQUIRE(Single[Index].Angle == Parallel[Index].Angle);
		PHYSICS_REQUIRE(Single[Index].AngularVelocity == Parallel[Index].AngularVelocity);
	}
	PHYSICS_REQUIRE(SingleDiagnostics.ExecutionThreadCount == 1);
	PHYSICS_REQUIRE(ParallelDiagnostics.ExecutionThreadCount == 4);
	PHYSICS_REQUIRE(ParallelDiagnostics.CandidatePairCount > 0);
	PHYSICS_REQUIRE(ParallelDiagnostics.IslandCount > 1);
	PHYSICS_REQUIRE(ParallelDiagnostics.SolverIslandCount == ParallelDiagnostics.IslandCount);
}

void WorkerCountDoesNotChange3DResult_Internal()
{
	FJobSystem One(1);
	FJobSystem Four(4);
	FPhysicsExecutionDiagnostics SingleDiagnostics;
	FPhysicsExecutionDiagnostics ParallelDiagnostics;
	const TVector<FState3D> Single = Run3D_Internal(&One, SingleDiagnostics);
	const TVector<FState3D> Parallel = Run3D_Internal(&Four, ParallelDiagnostics);
	PHYSICS_REQUIRE(Single.Size() == Parallel.Size());
	for (size_t Index = 0; Index < Single.Size(); ++Index)
	{
		PHYSICS_REQUIRE(Single[Index].Position == Parallel[Index].Position);
		PHYSICS_REQUIRE(Single[Index].Velocity == Parallel[Index].Velocity);
		PHYSICS_REQUIRE(Single[Index].Orientation.X == Parallel[Index].Orientation.X);
		PHYSICS_REQUIRE(Single[Index].Orientation.Y == Parallel[Index].Orientation.Y);
		PHYSICS_REQUIRE(Single[Index].Orientation.Z == Parallel[Index].Orientation.Z);
		PHYSICS_REQUIRE(Single[Index].Orientation.W == Parallel[Index].Orientation.W);
		PHYSICS_REQUIRE(Single[Index].AngularVelocity == Parallel[Index].AngularVelocity);
	}
	PHYSICS_REQUIRE(SingleDiagnostics.ExecutionThreadCount == 1);
	PHYSICS_REQUIRE(ParallelDiagnostics.ExecutionThreadCount == 4);
	PHYSICS_REQUIRE(ParallelDiagnostics.CandidatePairCount > 0);
	PHYSICS_REQUIRE(ParallelDiagnostics.IslandCount > 1);
	PHYSICS_REQUIRE(ParallelDiagnostics.SolverIslandCount == ParallelDiagnostics.IslandCount);
}

void BroadPhaseSlopKeepsNearPair_Internal()
{
	TVector<FBroadPhaseEntry> Entries;
	Entries.PushBack({0, 0, 1, true, {0, 0, 0, 1, 1, 0, false}});
	Entries.PushBack({1, 1, 1, false, {1.009f, 0, 0, 2, 1, 0, false}});
	TVector<FBroadPhasePair> Pairs;
	FBroadPhase::Generate(Entries, 0.005f, nullptr, Pairs);
	PHYSICS_REQUIRE(Pairs.Size() == 1);
}

void BroadPhasePreservesSmallExtentAtLargeCoordinate_Internal()
{
	TVector<FBroadPhaseEntry> Entries;
	FBroadPhaseEntry A;
	A.ColliderIndex = 0;
	A.BodyIndex = 0;
	A.BodyGeneration = 1;
	A.bDynamic = true;
	A.Bounds.MinX = 100000000.0 - 1.0;
	A.Bounds.MaxX = 100000000.0 + 1.0;
	A.Bounds.MinY = -1.0;
	A.Bounds.MaxY = 1.0;
	Entries.PushBack(A);
	FBroadPhaseEntry B;
	B.ColliderIndex = 1;
	B.BodyIndex = 1;
	B.BodyGeneration = 1;
	B.bDynamic = false;
	B.Bounds.MinX = 100000000.5;
	B.Bounds.MaxX = 100000001.5;
	B.Bounds.MinY = -1.0;
	B.Bounds.MaxY = 1.0;
	Entries.PushBack(B);
	TVector<FBroadPhasePair> Pairs;
	FBroadPhase::Generate(Entries, 0.0f, nullptr, Pairs);
	PHYSICS_REQUIRE(Pairs.Size() == 1);
}

void IslandStaticSupportDoesNotMergeDynamics_Internal()
{
	TVector<FIslandEdge> Edges;
	Edges.PushBack({0, 3, 0, true, false});
	Edges.PushBack({1, 3, 1, true, false});
	Edges.PushBack({2, 3, 2, true, false});
	TVector<FPhysicsIsland> Islands;
	FIslandManager::Build(4, Edges, Islands);
	PHYSICS_REQUIRE(Islands.Size() == 3);
	PHYSICS_REQUIRE(Islands[0].BodyIndices.Size() == 1);
	PHYSICS_REQUIRE(Islands[1].BodyIndices.Size() == 1);
	PHYSICS_REQUIRE(Islands[2].BodyIndices.Size() == 1);
}

const PhysicsTest::FCase Cases[] = {
    {"null execution matches borrowed single lane", NullExecutionMatchesSingleLane_Internal},
    {"parallel 2D physics matches one execution lane", WorkerCountDoesNotChange2DResult_Internal},
    {"parallel 3D physics matches one execution lane", WorkerCountDoesNotChange3DResult_Internal},
    {"broad phase keeps ContactSlop near pair", BroadPhaseSlopKeepsNearPair_Internal},
    {"broad phase preserves small extent at large coordinate", BroadPhasePreservesSmallExtentAtLargeCoordinate_Internal},
    {"shared static support does not merge dynamic islands", IslandStaticSupportDoesNotMergeDynamics_Internal},
};
} // namespace

const PhysicsTest::FCase* PhysicsTest::GetParallelCases(Toolbox::size_t& Count) noexcept
{
	Count = sizeof(Cases) / sizeof(Cases[0]);
	return Cases;
}
