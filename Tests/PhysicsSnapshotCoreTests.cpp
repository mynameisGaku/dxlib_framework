// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/PhysicsSnapshot.h"
#include "PhysicsSnapshotBuilder.h"
using namespace Dxf;
using namespace Dxf::PhysicsPrivate;
namespace
{
// 共通採取処理へ渡すテスト用登録領域。本物のWorldやSolverではない。
struct FBodyId
{
	Toolbox::uint64 World = 0;
	Toolbox::size_t Index = 0;
	Toolbox::uint64 Generation = 0;
	bool operator==(const FBodyId&) const = default;
};
struct FColliderId
{
	FBodyId Body;
	Toolbox::size_t Index = 0;
	Toolbox::uint64 Generation = 0;
	bool operator==(const FColliderId&) const = default;
};
// 入れ子の所有配列を使い、単なるポインタ借用ではないことも検証する。
using FShape = Toolbox::TVector<Toolbox::int32>;
using FSnapshot = TPhysicsSnapshot<FBodyId, FColliderId, Toolbox::f32,
                                  Toolbox::f32, Toolbox::f32, FShape>;
struct FBody
{
	Toolbox::uint64 Generation = 1;
	bool bAlive = true;
	EBodyType Type = EBodyType::Dynamic;
	Toolbox::f32 Position = 3;
	Toolbox::f32 Angle = 2;
	Toolbox::f32 Velocity = 5;
	Toolbox::f32 AngularVelocity = 7;
	bool bSleeping = false;
	bool bUseContinuous = false;
};
struct FCollider
{
	Toolbox::uint64 Generation = 1;
	bool bAlive = true;
	FBodyId Body{1, 0, 1};
	FShape Shape{11, 12};
	Toolbox::f32 Friction = 0.5f;
	Toolbox::f32 Restitution = 0.25f;
};
struct FFixture
{
	FSnapshotStepState State;
	Toolbox::TVector<FBody> Bodies;
	Toolbox::TVector<FCollider> Colliders;
};
FSnapshot Capture_Internal(const FFixture& Fixture, const FPhysicsSnapshotLimits& Limits = {})
{
	return CaptureSnapshot_Internal<FSnapshot>(1, Fixture.State, Fixture.Bodies, Fixture.Colliders, Limits,
	    [](const FBody& Body, FSnapshot::FBody& Item)
	    {
		    Item.Rotation = Body.Angle;
	    });
}
template <typename TFunction> bool Throws_Internal(TFunction Function)
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
TEST("snapshot_core_empty_zero_limits")
{
	FFixture Fixture;
	const auto Snapshot = Capture_Internal(Fixture, {0, 0});
	REQUIRE(Snapshot.World == 1);
	REQUIRE(Snapshot.StepIndex == 0);
	REQUIRE(Snapshot.LastDeltaSeconds == 0);
	REQUIRE(Snapshot.LastSubSteps == 0);
	REQUIRE(Snapshot.Bodies.IsEmpty() && Snapshot.Colliders.IsEmpty());
}
TEST("snapshot_core_includes_bodies_without_colliders")
{
	FFixture Fixture;
	Fixture.Bodies.PushBack(FBody{});
	REQUIRE(Capture_Internal(Fixture).Bodies.Size() == 1);
}
TEST("snapshot_core_sparse_slots_keep_original_ids_and_order")
{
	FFixture Fixture;
	Fixture.Bodies.Resize(5);
	Fixture.Bodies[1].bAlive = false;
	Fixture.Bodies[3].bAlive = false;
	Fixture.Bodies[4].Generation = 8;
	const auto Snapshot = Capture_Internal(Fixture, {3, 0});
	REQUIRE(Snapshot.Bodies.Size() == 3);
	REQUIRE(Snapshot.Bodies[1].Id.Index == 2);
	REQUIRE(Snapshot.Bodies[2].Id == FBodyId{1, 4, 8});
}
TEST("snapshot_core_multiple_colliders_and_deleted_slots")
{
	FFixture Fixture;
	Fixture.Bodies.Resize(1);
	Fixture.Colliders.Resize(3);
	Fixture.Colliders[1].bAlive = false;
	Fixture.Colliders[2].Generation = 9;
	const auto Snapshot = Capture_Internal(Fixture);
	REQUIRE(Snapshot.Colliders.Size() == 2);
	REQUIRE(Snapshot.Colliders[1].Id.Index == 2);
	REQUIRE(Snapshot.Colliders[1].Id.Generation == 9);
	REQUIRE(Snapshot.Colliders[1].Id.Body == Snapshot.Bodies[0].Id);
}
TEST("snapshot_core_copies_motion_and_material_values")
{
	FFixture Fixture;
	Fixture.Bodies.Resize(1);
	Fixture.Colliders.Resize(1);
	Fixture.Bodies[0].Type = EBodyType::Kinematic;
	Fixture.Bodies[0].bSleeping = true;
	Fixture.Bodies[0].bUseContinuous = true;
	const auto Snapshot = Capture_Internal(Fixture);
	const auto& Body = Snapshot.Bodies[0];
	REQUIRE(Body.Type == EBodyType::Kinematic);
	REQUIRE(Body.Position == 3 && Body.Rotation == 2);
	REQUIRE(Body.Velocity == 5 && Body.AngularVelocity == 7);
	REQUIRE(Body.bSleeping && Body.bUseContinuous);
	REQUIRE(Snapshot.Colliders[0].Friction == 0.5f);
	REQUIRE(Snapshot.Colliders[0].Restitution == 0.25f);
}
TEST("snapshot_core_survives_source_destruction")
{
	FSnapshot Snapshot;
	{
		FFixture Fixture;
		Fixture.Bodies.Resize(1);
		Fixture.Colliders.Resize(1);
		Snapshot = Capture_Internal(Fixture);
	}
	REQUIRE(Snapshot.Bodies[0].Position == 3);
	REQUIRE(Snapshot.Colliders[0].LocalShape[1] == 12);
}
TEST("snapshot_core_copies_do_not_alias_source_or_each_other")
{
	FFixture Fixture;
	Fixture.Bodies.Resize(1);
	Fixture.Colliders.Resize(1);
	const auto Original = Capture_Internal(Fixture);
	auto Copy = Original;
	Fixture.Colliders[0].Shape[0] = 80;
	Copy.Colliders[0].LocalShape[0] = 90;
	REQUIRE(Original.Colliders[0].LocalShape[0] == 11);
}
TEST("snapshot_core_body_limit_does_not_truncate")
{
	FFixture Fixture;
	Fixture.Bodies.Resize(2);
	REQUIRE(Throws_Internal([&]()
	{
		Capture_Internal(Fixture, {1, 10});
	}));
	REQUIRE(Fixture.Bodies.Size() == 2);
}
TEST("snapshot_core_collider_limit_does_not_truncate")
{
	FFixture Fixture;
	Fixture.Bodies.Resize(1);
	Fixture.Colliders.Resize(2);
	REQUIRE(Throws_Internal([&]()
	{
		Capture_Internal(Fixture, {1, 1});
	}));
	REQUIRE(Fixture.Colliders.Size() == 2);
}
TEST("snapshot_core_wrong_world_parent_is_rejected")
{
	FFixture Fixture;
	Fixture.Bodies.Resize(1);
	Fixture.Colliders.Resize(1);
	Fixture.Colliders[0].Body.World = 2;
	REQUIRE(Throws_Internal([&]()
	{
		Capture_Internal(Fixture);
	}));
}
TEST("snapshot_core_out_of_range_parent_is_rejected")
{
	FFixture Fixture;
	Fixture.Bodies.Resize(1);
	Fixture.Colliders.Resize(1);
	Fixture.Colliders[0].Body.Index = 1;
	REQUIRE(Throws_Internal([&]()
	{
		Capture_Internal(Fixture);
	}));
}
TEST("snapshot_core_dead_parent_is_rejected")
{
	FFixture Fixture;
	Fixture.Bodies.Resize(1);
	Fixture.Colliders.Resize(1);
	Fixture.Bodies[0].bAlive = false;
	REQUIRE(Throws_Internal([&]()
	{
		Capture_Internal(Fixture);
	}));
}
TEST("snapshot_core_reused_parent_generation_is_rejected")
{
	FFixture Fixture;
	Fixture.Bodies.Resize(1);
	Fixture.Colliders.Resize(1);
	Fixture.Bodies[0].Generation = 2;
	REQUIRE(Throws_Internal([&]()
	{
		Capture_Internal(Fixture);
	}));
}
TEST("snapshot_core_step_counts_once_not_per_substep")
{
	FFixture Fixture;
	{
		FSnapshotStepGuard Guard(Fixture.State, 0.125, 8);
		Guard.Complete();
		Guard.Complete();
	}
	const auto Snapshot = Capture_Internal(Fixture);
	REQUIRE(Snapshot.StepIndex == 1);
	REQUIRE(Snapshot.LastDeltaSeconds == 0.125);
	REQUIRE(Snapshot.LastSubSteps == 8);
}
TEST("snapshot_core_in_progress_capture_is_rejected")
{
	FFixture Fixture;
	FSnapshotStepGuard Guard(Fixture.State, 1, 1);
	Guard.Complete();
	REQUIRE(Throws_Internal([&]()
	{
		Capture_Internal(Fixture);
	}));
}
TEST("snapshot_core_failed_step_does_not_publish_completed_metadata")
{
	FFixture Fixture;
	REQUIRE(Throws_Internal([&]()
	{
		FSnapshotStepGuard Guard(Fixture.State, 1, 1);
		throw Toolbox::FException("simulated Step failure");
	}));
	REQUIRE(Fixture.State.StepIndex == 0);
	REQUIRE(!Fixture.State.bInStep);
	REQUIRE(Throws_Internal([&]()
	{
		Capture_Internal(Fixture);
	}));
}
TEST("snapshot_core_success_after_failure_allows_capture")
{
	FFixture Fixture;
	{
		FSnapshotStepGuard Failed(Fixture.State, 1, 1);
	}
	{
		FSnapshotStepGuard Success(Fixture.State, 0.25, 4);
		Success.Complete();
	}
	REQUIRE(Capture_Internal(Fixture).StepIndex == 1);
}
TEST("snapshot_core_step_overflow_is_rejected_before_changes")
{
	FFixture Fixture;
	Fixture.State.StepIndex = Toolbox::TNumericLimits<Toolbox::uint64>::Max();
	REQUIRE(Throws_Internal([&]()
	{
		FSnapshotStepGuard Guard(Fixture.State, 1, 1);
	}));
	REQUIRE(Fixture.State.bCaptureAllowed);
	REQUIRE(!Fixture.State.bInStep);
}
TEST("snapshot_core_reentrant_step_is_rejected")
{
	FFixture Fixture;
	FSnapshotStepGuard Outer(Fixture.State, 1, 1);
	REQUIRE(Throws_Internal([&]()
	{
		FSnapshotStepGuard Inner(Fixture.State, 1, 1);
	}));
	REQUIRE(Fixture.State.bInStep);
	Outer.Complete();
}
TEST("snapshot_core_copy_failure_preserves_previous_result")
{
	FFixture Fixture;
	Fixture.Bodies.Resize(2);
	auto Previous = Capture_Internal(Fixture);
	// 1件目の複製後、2件目で失敗させる。途中まで作った結果を公開しないことを確認する。
	Toolbox::size_t CopyCount = 0;
	REQUIRE(Throws_Internal([&]()
	{
		Previous = CaptureSnapshot_Internal<FSnapshot>(1, Fixture.State, Fixture.Bodies, Fixture.Colliders, {},
		    [&CopyCount](const FBody&, FSnapshot::FBody&)
		    {
			    ++CopyCount;
			    if (CopyCount == 2)
			    {
				    throw Toolbox::FException("copy failure");
			    }
		    });
	}));
	REQUIRE(CopyCount == 2);
	REQUIRE(Previous.Bodies.Size() == 2);
	REQUIRE(Previous.Bodies[0].Position == 3);
	REQUIRE(Fixture.Bodies[0].Position == 3);
}
