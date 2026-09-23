// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/PhysicsDebugPicking3D.h"
using namespace Dxf;
using namespace Toolbox;
namespace
{
FPhysicsDebugSnapshot3D Snapshot_Internal()
{
	FPhysicsDebugSnapshot3D Snapshot;
	Snapshot.World = 7;
	Snapshot.Step = 12;
	Snapshot.BodyCount = 1;
	FPhysicsDebugItem3D Item;
	Item.Collider = {{7, 4, 2}, 9, 3};
	Item.Shape = FSphere{{0, 0, 5}, 1};
	Snapshot.Items.PushBack(Item);
	Item.Collider.Index = 10;
	Item.Shape = FOBB{{0, 0, 8}, {1, 1, 1}};
	Snapshot.Items.PushBack(Item);
	return Snapshot;
}
} // namespace
TEST("snapshot picking is finite nearest stable and does not mutate inputs")
{
	const FLine3D Line{{0, 0, 0}, {0, 0, 10}};
	REQUIRE(!PickPhysicsDebugSnapshot3D({}, Line).Value());
	auto Snapshot = Snapshot_Internal();
	const auto Hit = PickPhysicsDebugSnapshot3D(Snapshot, Line);
	REQUIRE(Hit && Hit.Value());
	REQUIRE(Hit.Value()->Collider == Snapshot.Items[0].Collider);
	REQUIRE(Hit.Value()->Step == 12);
	REQUIRE(Abs(Hit.Value()->Fraction - 0.4) < 1e-6);
	REQUIRE(Hit.Value()->Position == FVector3(0, 0, 4));
	REQUIRE(Snapshot.Step == 12 && Snapshot.Items.Size() == 2 && Snapshot.Items[0].Shape.Get<0>().Center == FVector3(0, 0, 5));
	Snapshot.Items[1].Shape = FOBB{{0, 0, 5}, {1, 1, 1}};
	REQUIRE(PickPhysicsDebugSnapshot3D(Snapshot, Line).Value()->Collider == Snapshot.Items[0].Collider);
	Snapshot.Items[1].Shape = FOBB{{0, 0, 3}, {1, 1, 1}};
	REQUIRE(PickPhysicsDebugSnapshot3D(Snapshot, Line).Value()->Collider == Snapshot.Items[1].Collider);
	REQUIRE(!PickPhysicsDebugSnapshot3D(Snapshot, {{4, 0, 0}, {4, 0, 10}}).Value());
	REQUIRE(PickPhysicsDebugSnapshot3D(Snapshot, {{0, 0, 3}, {0, 0, 10}}).Value()->Fraction == 0);
	REQUIRE(PickPhysicsDebugSnapshot3D(Snapshot, {{0, 0, 0}, {0, 0, 2}}).Value()->Fraction == 1);
	REQUIRE(!PickPhysicsDebugSnapshot3D(Snapshot, {{0, 0, 0}, {0, 0, 1.9f}}).Value());
	REQUIRE(!PickPhysicsDebugSnapshot3D(Snapshot, {{0, 0, 9.1f}, {0, 0, 10}}).Value());
	REQUIRE(PickPhysicsDebugSnapshot3D(Snapshot, {{1, 0, 4}, {1, 0, 6}}).Value());
}
TEST("snapshot picking rejects invalid later entries and ambiguous identities")
{
	for (int32 Case = 0; Case < 8; ++Case)
	{
		auto Snapshot = Snapshot_Internal();
		auto& Item = Snapshot.Items[1];
		switch (Case)
		{
		case 0:
			Snapshot.World = 0;
			break;
		case 1:
			Item.Collider.Body.World = 8;
			break;
		case 2:
			Item.Collider.Generation = 0;
			break;
		case 3:
			Item.Collider.Body.Generation = 3;
			break;
		case 4:
			Item.Collider.Index = Snapshot.Items[0].Collider.Index;
			break;
		case 5:
			Item.Shape = FSphere{{0, 0, 8}, -1};
			break;
		case 6:
			Snapshot.BodyCount = 0;
			break;
		case 7:
			Item.Velocity.X = TNumericLimits<f32>::QuietNaN();
			break;
		}
		REQUIRE(!PickPhysicsDebugSnapshot3D(Snapshot, {{0, 0, 5}, {0, 0, 10}}));
	}
	const auto Snapshot = Snapshot_Internal();
	REQUIRE(!PickPhysicsDebugSnapshot3D(Snapshot, {{TNumericLimits<f32>::QuietNaN(), 0, 0}, {0, 0, 10}}));
	auto Huge = Snapshot_Internal();
	Huge.Items.Resize(MaxPhysicsDebugColliders3D + 1);
	REQUIRE(!PickPhysicsDebugSnapshot3D(Huge, {{0, 0, 0}, {0, 0, 10}}));
}
TEST("real World picking uses transformed offsets and survives source destruction")
{
	FPhysicsDebugSnapshot3D Saved;
	FColliderId3D Old;
	{
		FPhysicsWorld3D World;
		FBodyDescription3D Body;
		Body.Type = EBodyType::Kinematic;
		Body.Position = {10, 0, 0};
		Body.Orientation = FQuaternion::FromAxisAngle({0, 1, 0}, 1.57079632679f);
		const auto Id = World.CreateBody(Body);
		FColliderDescription3D Sphere;
		Sphere.Shape = FSphere{{0, 0, 2}, 0.5f};
		Old = World.AttachCollider(Id, Sphere);
		FColliderDescription3D Box;
		Box.Shape = FOBB{{0, 0, -2}, {0.5f, 0.5f, 1}};
		const auto BoxId = World.AttachCollider(Id, Box);
		Saved = CapturePhysicsDebugSnapshot3D(World, 0).Value();
		REQUIRE(PickPhysicsDebugSnapshot3D(Saved, {{12, 0, -3}, {12, 0, 3}}).Value()->Collider == Old);
		REQUIRE(PickPhysicsDebugSnapshot3D(Saved, {{8, 0, -3}, {8, 0, 3}}).Value()->Collider == BoxId);
		// 同じBody内でColliderスロットだけを再利用しても、保存IDへ引き継がない。
		REQUIRE(World.DetachCollider(Old));
		const auto Reattached = World.AttachCollider(Id, Sphere);
		REQUIRE(Reattached.Index == Old.Index && Reattached.Generation != Old.Generation && Reattached.Body == Old.Body);
		const auto ReattachedSnapshot = CapturePhysicsDebugSnapshot3D(World, 0).Value();
		REQUIRE(ReattachedSnapshot.Step == Saved.Step);
		REQUIRE(PickPhysicsDebugSnapshot3D(ReattachedSnapshot, {{12, 0, -3}, {12, 0, 3}}).Value()->Collider == Reattached);
		REQUIRE(PickPhysicsDebugSnapshot3D(Saved, {{12, 0, -3}, {12, 0, 3}}).Value()->Collider == Old);
		REQUIRE(World.DestroyBody(Id));
		const auto Reused = World.CreateBody(Body);
		const auto New = World.AttachCollider(Reused, Sphere);
		REQUIRE(Reused.Index == Id.Index && Reused.Generation != Id.Generation);
		const auto Current = CapturePhysicsDebugSnapshot3D(World, 0).Value();
		REQUIRE(Current.Step == Saved.Step);
		REQUIRE(PickPhysicsDebugSnapshot3D(Current, {{12, 0, -3}, {12, 0, 3}}).Value()->Collider == New);
		REQUIRE(New != Old);
		REQUIRE(PickPhysicsDebugSnapshot3D(Saved, {{12, 0, -3}, {12, 0, 3}}).Value()->Collider == Old);
		FPhysicsWorld3D Other;
		const auto OtherId = Other.CreateBody(Body);
		Other.AttachCollider(OtherId, Sphere);
		REQUIRE(CapturePhysicsDebugSnapshot3D(Other, 0).Value().World != Saved.World);
	}
	REQUIRE(PickPhysicsDebugSnapshot3D(Saved, {{12, 0, -3}, {12, 0, 3}}).Value()->Collider == Old);
}
TEST("real World motion is unchanged by stored snapshot queries")
{
	FPhysicsWorld3D A;
	FPhysicsWorld3D B;
	FBodyDescription3D Body;
	Body.Position = {0, 4, 0};
	Body.Velocity = {1, 0, 0};
	const auto IdA = A.CreateBody(Body);
	const auto IdB = B.CreateBody(Body);
	FColliderDescription3D Sphere;
	Sphere.Shape = FSphere{{0, 0, 0}, 0.5f};
	A.AttachCollider(IdA, Sphere);
	B.AttachCollider(IdB, Sphere);
	for (int32 Frame = 0; Frame < 12; ++Frame)
	{
		A.Step(1.0 / 60);
		B.Step(1.0 / 60);
		const auto Snapshot = CapturePhysicsDebugSnapshot3D(A, Frame / 60.0).Value();
		REQUIRE(PickPhysicsDebugSnapshot3D(Snapshot, {{0, 4, -10}, {0, 4, 10}}));
		REQUIRE(A.GetPosition(IdA) == B.GetPosition(IdB));
		REQUIRE(A.CaptureSnapshot().StepIndex == B.CaptureSnapshot().StepIndex);
	}
}
