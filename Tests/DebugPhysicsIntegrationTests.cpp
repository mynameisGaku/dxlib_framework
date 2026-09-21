// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/RigidBody3D.h"
#include "Dxf/PhysicsDebugSnapshot3D.h"
#include "Dxf/DebugSnapshotHistory.h"
using namespace Dxf;
using namespace Toolbox;
TEST("debug capture reads an actual stepped physics body and survives removal")
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D Body;
	Body.Position = {10, 20, 30};
	Body.Velocity = {3, 0, 0};
	Body.Orientation = FQuaternion::FromAxisAngle({0, 0, 1}, 1.57079632679f);
	const auto BodyId = World.CreateBody(Body);
	FColliderDescription3D Description;
	Description.Shape = FSphere{{2, 0, 0}, 0.5f};
	const auto Collider = World.AttachCollider(BodyId, Description);
	TVector<TPhysicsDebugWatch3D<FColliderId3D>> Watches;
	Watches.PushBack({Collider, Description.Shape, EDebugBodyMotion::Dynamic});
	World.Step(1.0 / 60.0);
	auto Captured = CapturePhysicsDebugSnapshot3D(World, Watches, 1, 1.0 / 60.0);
	REQUIRE(Captured);
	const auto Center = Get<FSphere>(Captured.Value().Items[0].Shape).Center;
	REQUIRE(Abs(Center.X - 10.05f) < 0.0001f);
	REQUIRE(Abs(Center.Y - 22.0f) < 0.0001f);
	REQUIRE(Captured.Value().Items[0].Velocity == FVector3(3, 0, 0));
	REQUIRE(World.DestroyBody(BodyId));
	REQUIRE(Get<FSphere>(Captured.Value().Items[0].Shape).Center == Center);
	const auto Removed = CapturePhysicsDebugSnapshot3D(World, Watches, 2, 2.0 / 60.0);
	REQUIRE(Removed);
	REQUIRE(Removed.Value().SkippedCount == 1);
	REQUIRE(Removed.Value().Items.IsEmpty());
}
TEST("actual worlds preserve snapshot geometry in one and four execution lanes")
{
	TVector<FPhysicsDebugSnapshot3D> Results;
	const uint32 LaneCounts[] = {1, 4};
	for (uint32 Lanes : LaneCounts)
	{
		FJobSystem Jobs(Lanes);
		FPhysicsWorld3D World;
		FPhysicsExecutionSettings Execution;
		Execution.JobSystem = &Jobs;
		World.SetExecutionSettings(Execution);
		World.SetGravity({0, 0, 0});
		FBodyDescription3D Description;
		Description.Velocity = {0.5f, 0, 0};
		const auto Body = World.CreateBody(Description);
		FColliderDescription3D Collider;
		Collider.Shape = FOBB{};
		const auto Id = World.AttachCollider(Body, Collider);
		TVector<TPhysicsDebugWatch3D<FColliderId3D>> Watches;
		Watches.PushBack({Id, Collider.Shape, EDebugBodyMotion::Dynamic});
		for (uint32 Tick = 0; Tick < 60; ++Tick)
		{
			World.Step(1.0 / 60.0);
		}
		Results.PushBack(CapturePhysicsDebugSnapshot3D(World, Watches, 60, 1).Value());
	}
	REQUIRE(Results[0].Items[0].CenterOfMass == Results[1].Items[0].CenterOfMass);
	REQUIRE(Results[0].Items[0].Velocity == Results[1].Items[0].Velocity);
}
