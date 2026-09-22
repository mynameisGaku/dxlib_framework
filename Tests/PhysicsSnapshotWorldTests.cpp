// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/JobSystem.h"
using namespace Dxf;
namespace
{
// 本物のWorldに取り付ける円。原点からのずれも観察する。
FColliderDescription2D Circle_Internal()
{
	Toolbox::FCircle2D Shape;
	Shape.Center = {1, 2};
	Shape.Radius = 0.5f;
	FColliderDescription2D Description;
	Description.Shape = Shape;
	return Description;
}
// 本物のWorldに取り付ける球。
FColliderDescription3D Sphere_Internal()
{
	Toolbox::FSphere Shape;
	Shape.Center = {1, 2, 3};
	Shape.Radius = 0.5f;
	FColliderDescription3D Description;
	Description.Shape = Shape;
	return Description;
}
template <typename TFunction> bool ThrowsWorld_Internal(TFunction Function)
{
	try
	{
		Function();
	}
	catch (const Toolbox::FException&)
	{
		return true;
	}
	return false;
}
} // namespace
TEST("snapshot_world_empty_2d_3d")
{
	FPhysicsWorld2D World2D;
	FPhysicsWorld3D World3D;
	const auto Two = World2D.CaptureSnapshot({0, 0});
	const auto Three = World3D.CaptureSnapshot({0, 0});
	REQUIRE(Two.World != 0 && Three.World != 0);
	REQUIRE(Two.Bodies.IsEmpty() && Three.Colliders.IsEmpty());
	REQUIRE(Two.StepIndex == 0 && Three.StepIndex == 0);
}
TEST("snapshot_world_all_bodies_and_compound_colliders_2d")
{
	FPhysicsWorld2D World;
	FBodyDescription2D Description;
	Description.Type = EBodyType::Kinematic;
	const auto Body = World.CreateBody(Description);
	const auto Bare = World.CreateBody(Description);
	const auto First = World.AttachCollider(Body, Circle_Internal());
	const auto Second = World.AttachCollider(Body, Circle_Internal());
	const auto Snapshot = World.CaptureSnapshot();
	REQUIRE(Snapshot.Bodies.Size() == 2 && Snapshot.Colliders.Size() == 2);
	REQUIRE(Snapshot.Bodies[0].Id == Body && Snapshot.Bodies[1].Id == Bare);
	REQUIRE(Snapshot.Bodies[0].Type == EBodyType::Kinematic);
	REQUIRE(Snapshot.Colliders[0].Id == First && Snapshot.Colliders[1].Id == Second);
	Snapshot.Colliders[0].LocalShape.Visit([](const auto& Shape)
	{
		REQUIRE(Shape.Center.X == 1 && Shape.Center.Y == 2);
	});
}
TEST("snapshot_world_all_bodies_and_compound_colliders_3d")
{
	FPhysicsWorld3D World;
	const auto Body = World.CreateBody({});
	const auto Bare = World.CreateBody({});
	const auto First = World.AttachCollider(Body, Sphere_Internal());
	const auto Second = World.AttachCollider(Body, Sphere_Internal());
	const auto Snapshot = World.CaptureSnapshot();
	REQUIRE(Snapshot.Bodies.Size() == 2 && Snapshot.Colliders.Size() == 2);
	REQUIRE(Snapshot.Bodies[1].Id == Bare);
	REQUIRE(Snapshot.Colliders[0].Id == First && Snapshot.Colliders[1].Id == Second);
	Snapshot.Colliders[0].LocalShape.Visit([](const auto& Shape)
	{
		REQUIRE(Shape.Center.X == 1 && Shape.Center.Y == 2 && Shape.Center.Z == 3);
	});
}
TEST("snapshot_world_destroy_and_reuse_2d")
{
	FPhysicsWorld2D World;
	const auto OldBody = World.CreateBody({});
	const auto OldCollider = World.AttachCollider(OldBody, Circle_Internal());
	const auto Before = World.CaptureSnapshot();
	REQUIRE(World.DestroyBody(OldBody));
	const auto Body = World.CreateBody({});
	const auto Collider = World.AttachCollider(Body, Circle_Internal());
	const auto After = World.CaptureSnapshot();
	REQUIRE(After.Bodies.Size() == 1 && After.Colliders.Size() == 1);
	REQUIRE(!(Body == OldBody) && !(Collider == OldCollider));
	REQUIRE(Before.Colliders[0].Id == OldCollider);
	REQUIRE(After.Colliders[0].Id == Collider);
}
TEST("snapshot_world_detach_and_reuse_3d")
{
	FPhysicsWorld3D World;
	const auto Body = World.CreateBody({});
	const auto First = World.AttachCollider(Body, Sphere_Internal());
	REQUIRE(World.DetachCollider(First));
	const auto Second = World.AttachCollider(Body, Sphere_Internal());
	const auto Snapshot = World.CaptureSnapshot();
	REQUIRE(Snapshot.Colliders.Size() == 1);
	REQUIRE(Snapshot.Colliders[0].Id == Second && !(First == Second));
}
TEST("snapshot_world_step_transform_and_no_mutation_2d")
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FBodyDescription2D Description;
	Description.Velocity = {1, 0};
	const auto Body = World.CreateBody(Description);
	World.Step(0.25, 4);
	const auto First = World.CaptureSnapshot();
	const auto Again = World.CaptureSnapshot();
	REQUIRE(First.StepIndex == 1 && Again.StepIndex == 1);
	REQUIRE(First.LastSubSteps == 4 && First.LastDeltaSeconds == 0.25);
	REQUIRE(First.Bodies[0].Position.X == World.GetPosition(Body).X);
	World.SetBodyTransform(Body, {10, 20}, 0.5f);
	const auto Moved = World.CaptureSnapshot();
	REQUIRE(Moved.StepIndex == 1 && Moved.Bodies[0].Position.X == 10);
	REQUIRE(Moved.Bodies[0].Rotation == 0.5f);
	REQUIRE(First.Bodies[0].Position.X != 10);
}
TEST("snapshot_world_step_and_orientation_3d")
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D Description;
	Description.Velocity = {1, 0, 0};
	Description.AngularVelocity = {0, 0, 1};
	const auto Body = World.CreateBody(Description);
	World.Step(0.125, 8);
	const auto Snapshot = World.CaptureSnapshot();
	REQUIRE(Snapshot.StepIndex == 1 && Snapshot.LastSubSteps == 8);
	REQUIRE(Snapshot.Bodies[0].Rotation.Z == World.GetOrientation(Body).Z);
	REQUIRE(Snapshot.Bodies[0].Position.X == World.GetPosition(Body).X);
}
TEST("snapshot_world_snapshot_survives_both_worlds")
{
	FPhysicsSnapshot2D Two;
	FPhysicsSnapshot3D Three;
	{
		FPhysicsWorld2D World2D;
		FPhysicsWorld3D World3D;
		World2D.AttachCollider(World2D.CreateBody({}), Circle_Internal());
		World3D.AttachCollider(World3D.CreateBody({}), Sphere_Internal());
		Two = World2D.CaptureSnapshot();
		Three = World3D.CaptureSnapshot();
	}
	REQUIRE(Two.Bodies.Size() == 1 && Three.Bodies.Size() == 1);
	Two.Colliders[0].LocalShape.Visit([](const auto& Shape)
	{
		REQUIRE(Shape.Center.X == 1);
	});
	Three.Colliders[0].LocalShape.Visit([](const auto& Shape)
	{
		REQUIRE(Shape.Center.Z == 3);
	});
}
TEST("snapshot_world_limits_and_invalid_step_preserve_state")
{
	FPhysicsWorld2D World2D;
	FPhysicsWorld3D World3D;
	const auto TwoId = World2D.CreateBody({});
	World3D.AttachCollider(World3D.CreateBody({}), Sphere_Internal());
	REQUIRE(ThrowsWorld_Internal([&]()
	{
		World2D.CaptureSnapshot({0, 0});
	}));
	REQUIRE(ThrowsWorld_Internal([&]()
	{
		World3D.CaptureSnapshot({1, 0});
	}));
	REQUIRE(ThrowsWorld_Internal([&]()
	{
		World2D.Step(0);
	}));
	REQUIRE(World2D.CaptureSnapshot().StepIndex == 0);
	REQUIRE(World2D.IsAlive(TwoId));
}
// 以下は実Worldへの統合後に追加した回帰。共通処理の20件とは別に、実Stepと実登録を使う。
TEST("snapshot_world_failed_step_blocks_capture_until_success_2d")
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	const auto Body = World.CreateBody({});
	World.Step(0.5, 2);
	// 停止済みJob Systemは投入を拒否し、Step開始後の並列積分で本物の例外を起こす。
	Toolbox::FJobSystem Jobs(2);
	Jobs.Shutdown();
	FPhysicsExecutionSettings Broken;
	Broken.JobSystem = &Jobs;
	World.SetExecutionSettings(Broken);
	REQUIRE(ThrowsWorld_Internal([&]()
	{
		World.Step(0.25, 1);
	}));
	// 中断後は正常Stepが完了するまで採取を拒否する。
	REQUIRE(ThrowsWorld_Internal([&]()
	{
		World.CaptureSnapshot();
	}));
	World.SetExecutionSettings({});
	World.Step(0.125, 3);
	const auto Snapshot = World.CaptureSnapshot();
	// 中断したStepは通算数と最後の引数に含めない。
	REQUIRE(Snapshot.StepIndex == 2);
	REQUIRE(Snapshot.LastDeltaSeconds == 0.125 && Snapshot.LastSubSteps == 3);
	REQUIRE(Snapshot.Bodies.Size() == 1 && Snapshot.Bodies[0].Id == Body);
}
TEST("snapshot_world_failed_step_blocks_capture_until_success_3d")
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	World.CreateBody({});
	Toolbox::FJobSystem Jobs(2);
	Jobs.Shutdown();
	FPhysicsExecutionSettings Broken;
	Broken.JobSystem = &Jobs;
	World.SetExecutionSettings(Broken);
	REQUIRE(ThrowsWorld_Internal([&]()
	{
		World.Step(0.25, 1);
	}));
	REQUIRE(ThrowsWorld_Internal([&]()
	{
		World.CaptureSnapshot();
	}));
	World.SetExecutionSettings({});
	World.Step(0.5, 5);
	const auto Snapshot = World.CaptureSnapshot();
	REQUIRE(Snapshot.StepIndex == 1);
	REQUIRE(Snapshot.LastDeltaSeconds == 0.5 && Snapshot.LastSubSteps == 5);
}
TEST("snapshot_world_ids_are_world_scoped")
{
	FPhysicsWorld2D First;
	FPhysicsWorld2D Second;
	const auto FirstBody = First.CreateBody({});
	const auto FirstCollider = First.AttachCollider(FirstBody, Circle_Internal());
	const auto SecondBody = Second.CreateBody({});
	const auto SecondCollider = Second.AttachCollider(SecondBody, Circle_Internal());
	// 同じスロットと世代でも、World識別子で区別される。
	REQUIRE(FirstBody.Index == SecondBody.Index && FirstBody.Generation == SecondBody.Generation);
	REQUIRE(!(FirstBody == SecondBody) && !(FirstCollider == SecondCollider));
	const auto SnapshotA = First.CaptureSnapshot();
	const auto SnapshotB = Second.CaptureSnapshot();
	REQUIRE(SnapshotA.World != SnapshotB.World);
	REQUIRE(SnapshotA.Bodies[0].Id.World == SnapshotA.World);
	REQUIRE(SnapshotA.Colliders[0].Id.Body.World == SnapshotA.World);
	REQUIRE(SnapshotB.Bodies[0].Id == SecondBody && !(SnapshotB.Bodies[0].Id == FirstBody));
	REQUIRE(SnapshotB.Colliders[0].Id == SecondCollider);
	FPhysicsWorld3D Three;
	const auto ThreeBody = Three.CreateBody({});
	const auto ThreeSnapshot = Three.CaptureSnapshot();
	REQUIRE(ThreeSnapshot.Bodies[0].Id == ThreeBody && ThreeSnapshot.World == ThreeBody.World);
}
TEST("snapshot_world_local_box_and_flags_2d")
{
	FPhysicsWorld2D World;
	FBodyDescription2D Description;
	Description.Type = EBodyType::Static;
	Description.Position = {4, 5};
	Description.Angle = 1.0f;
	Description.bUseContinuous = true;
	const auto Body = World.CreateBody(Description);
	Toolbox::FOrientedBox2D Box;
	Box.Center = {0.5f, 0};
	Box.HalfExtents = {1, 0.25f};
	Box.Angle = 0.3f;
	FColliderDescription2D Collider;
	Collider.Shape = Box;
	Collider.Friction = 0.75f;
	Collider.Restitution = 0.125f;
	World.AttachCollider(Body, Collider);
	const auto Snapshot = World.CaptureSnapshot();
	REQUIRE(Snapshot.Bodies[0].Type == EBodyType::Static && Snapshot.Bodies[0].bUseContinuous);
	REQUIRE(Snapshot.Bodies[0].Rotation == 1.0f);
	REQUIRE(Snapshot.Colliders[0].Friction == 0.75f && Snapshot.Colliders[0].Restitution == 0.125f);
	// 形状は重心相対のまま保持し、Bodyの角度を合成しない。
	REQUIRE(Snapshot.Colliders[0].LocalShape.Index() == 1);
	const Toolbox::FOrientedBox2D& Shape = Snapshot.Colliders[0].LocalShape.Get<1>();
	REQUIRE(Shape.Center.X == 0.5f && Shape.Center.Y == 0);
	REQUIRE(Shape.Angle == 0.3f && Shape.HalfExtents.X == 1);
}
TEST("snapshot_world_local_obb_and_orientation_3d")
{
	FPhysicsWorld3D World;
	FBodyDescription3D Description;
	Description.Type = EBodyType::Kinematic;
	Description.Position = {1, 2, 3};
	Description.Orientation = Toolbox::FQuaternion::FromAxisAngle({0, 1, 0}, 0.5f);
	const auto Body = World.CreateBody(Description);
	Toolbox::FOBB Box;
	Box.Center = {0, 0, 2};
	Box.HalfExtents = {0.5f, 1, 1.5f};
	Box.Axes = {Toolbox::FVector3{0, 1, 0}, Toolbox::FVector3{-1, 0, 0}, Toolbox::FVector3{0, 0, 1}};
	FColliderDescription3D Collider;
	Collider.Shape = Box;
	World.AttachCollider(Body, Collider);
	const auto Snapshot = World.CaptureSnapshot();
	const auto Orientation = World.GetOrientation(Body);
	REQUIRE(Snapshot.Bodies[0].Type == EBodyType::Kinematic);
	REQUIRE(Snapshot.Bodies[0].Rotation.Y == Orientation.Y && Snapshot.Bodies[0].Rotation.W == Orientation.W);
	REQUIRE(Snapshot.Colliders[0].LocalShape.Index() == 1);
	// 箱の軸はBody姿勢を合成しない重心相対の値。
	const Toolbox::FOBB& Shape = Snapshot.Colliders[0].LocalShape.Get<1>();
	REQUIRE(Shape.Center.Z == 2 && Shape.HalfExtents.Z == 1.5f);
	REQUIRE(Shape.Axes[0].Y == 1 && Shape.Axes[1].X == -1 && Shape.Axes[2].Z == 1);
}
TEST("snapshot_world_sleeping_matches_world_2d")
{
	FPhysicsWorld2D World;
	FBodyDescription2D Deck;
	Deck.Type = EBodyType::Kinematic;
	const auto Platform = World.CreateBody(Deck);
	Toolbox::FOrientedBox2D Slab;
	Slab.HalfExtents = {2, 0.5f};
	FColliderDescription2D SlabCollider;
	SlabCollider.Shape = Slab;
	SlabCollider.Friction = 0.6f;
	World.AttachCollider(Platform, SlabCollider);
	FBodyDescription2D Fall;
	Fall.Position = {0, 1.0f};
	const auto Crate = World.CreateBody(Fall);
	FColliderDescription2D CrateCollider;
	CrateCollider.Shape = Toolbox::FOrientedBox2D{};
	CrateCollider.Friction = 0.6f;
	World.AttachCollider(Crate, CrateCollider);
	for (Toolbox::int32 Step = 0; Step < 600; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	REQUIRE(World.IsSleeping(Crate));
	const auto Snapshot = World.CaptureSnapshot();
	REQUIRE(Snapshot.StepIndex == 600);
	REQUIRE(Snapshot.Bodies[1].Id == Crate && Snapshot.Bodies[1].bSleeping);
	REQUIRE(!Snapshot.Bodies[0].bSleeping);
}
TEST("snapshot_world_capture_does_not_change_simulation")
{
	// 同じ条件の2 Worldを進め、片方だけ毎Step採取しても数値が一致することを確認する。
	FPhysicsWorld3D Observed;
	FPhysicsWorld3D Plain;
	FBodyDescription3D Ground;
	Ground.Type = EBodyType::Static;
	Toolbox::FOBB Floor;
	Floor.HalfExtents = {5, 0.5f, 5};
	FColliderDescription3D FloorCollider;
	FloorCollider.Shape = Floor;
	FBodyDescription3D Falling;
	Falling.Position = {0, 2, 0};
	Falling.AngularVelocity = {0.5f, 0, 0.25f};
	FColliderDescription3D FallingCollider;
	FallingCollider.Shape = Toolbox::FOBB{};
	Observed.AttachCollider(Observed.CreateBody(Ground), FloorCollider);
	Plain.AttachCollider(Plain.CreateBody(Ground), FloorCollider);
	const auto ObservedBody = Observed.CreateBody(Falling);
	const auto PlainBody = Plain.CreateBody(Falling);
	Observed.AttachCollider(ObservedBody, FallingCollider);
	Plain.AttachCollider(PlainBody, FallingCollider);
	for (Toolbox::int32 Step = 0; Step < 240; ++Step)
	{
		Observed.Step(1.0 / 60.0, 2);
		Plain.Step(1.0 / 60.0, 2);
		const auto Snapshot = Observed.CaptureSnapshot();
		REQUIRE(Snapshot.StepIndex == static_cast<Toolbox::uint64>(Step + 1));
	}
	const auto A = Observed.GetPosition(ObservedBody);
	const auto B = Plain.GetPosition(PlainBody);
	const auto QA = Observed.GetOrientation(ObservedBody);
	const auto QB = Plain.GetOrientation(PlainBody);
	REQUIRE(A.X == B.X && A.Y == B.Y && A.Z == B.Z);
	REQUIRE(QA.X == QB.X && QA.Y == QB.Y && QA.Z == QB.Z && QA.W == QB.W);
}
