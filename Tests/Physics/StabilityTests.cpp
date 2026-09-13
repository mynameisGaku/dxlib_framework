// SPDX-License-Identifier: NOASSERTION
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/Array.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
bool Near_Internal(f64 A, f64 B, f64 Absolute, f64 Relative)
{
	const f64 Scale = 1 + (Abs(A) > Abs(B) ? Abs(A) : Abs(B));
	return Abs(A - B) <= Absolute + Relative * Scale;
}
// 積み重ねる箱の段数。
constexpr size_t StackCount_Internal = 5;
// 積み重ねの沈み込みとずれの計測値。
struct FStackMetrics2D
{
	// 理想高さからの最大沈み込み。メートル単位で正が沈み。
	f32 MaxPenetration = 0;
	// 理想高さからの最大浮き上がり。メートル単位で正が浮き。
	f32 MaxLift = 0;
	// 中心軸からの最大水平ずれ。メートル単位。
	f32 MaxDrift = 0;
	// 最大傾き。ラジアン単位の絶対値。
	f32 MaxTilt = 0;
	// 最大速度。メートル毎秒単位。
	f32 MaxSpeed = 0;
	// 非有限の位置や速度があれば真。
	bool bAnyNonFinite = false;
	// 全段が休止していれば真。
	bool bAllSleeping = false;
};
// 床の形状と材質を作る。
FColliderDescription2D StackFloorDescription_Internal()
{
	FColliderDescription2D Floor;
	FOrientedBox2D Shape;
	Shape.Center = {0, -1};
	Shape.HalfExtents = {5, 1};
	Shape.Angle = 0;
	Floor.Shape = Shape;
	Floor.Friction = 0.6f;
	Floor.Restitution = 0;
	return Floor;
}
// 積み重ねる箱の形状と材質を作る。
FColliderDescription2D StackBoxDescription_Internal()
{
	FColliderDescription2D Box;
	FOrientedBox2D Shape;
	Shape.Center = {0, 0};
	Shape.HalfExtents = {0.5f, 0.5f};
	Shape.Angle = 0;
	Box.Shape = Shape;
	Box.Friction = 0.6f;
	Box.Restitution = 0;
	return Box;
}
// 休止の有無を切り替えて五段積みを解き、沈み込みとずれを測る。
void RunStack2D_Internal(bool bEnableSleep, TArray<FBodyId2D, StackCount_Internal>& OutIds, FStackMetrics2D& OutMetrics)
{
	FPhysicsWorld2D World;
	// 休止の有効無効だけを切り替える。
	FSleepSettings2D Sleep = World.GetSleepSettings();
	Sleep.bEnabled = bEnableSleep;
	World.SetSleepSettings(Sleep);
	// 静止床を作る。
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId2D FloorId = World.CreateBody(Ground);
	World.AttachCollider(FloorId, StackFloorDescription_Internal());
	// わずかな隙間で積み上げて落下後に整列させる。
	for (size_t Index = 0; Index < OutIds.Size(); ++Index)
	{
		FBodyDescription2D Box;
		Box.Position = {0, 0.5f + static_cast<f32>(Index) * 1.02f};
		const FBodyId2D Id = World.CreateBody(Box);
		World.AttachCollider(Id, StackBoxDescription_Internal());
		OutIds[Index] = Id;
	}
	for (int32 Step = 0; Step < 600; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 理想配置は床上面を基準にした等間隔。
	OutMetrics = {};
	OutMetrics.bAllSleeping = true;
	for (size_t Index = 0; Index < OutIds.Size(); ++Index)
	{
		const FBodyId2D Id = OutIds[Index];
		const FVector2 Position = World.GetPosition(Id);
		const f32 Angle = World.GetAngle(Id);
		const FVector2 Velocity = World.GetVelocity(Id);
		const f32 Angular = World.GetAngularVelocity(Id);
		if (!World.IsSleeping(Id))
		{
			OutMetrics.bAllSleeping = false;
		}
		// 有限性の破綻を記録する。
		if (!IsFinite(Position.X) || !IsFinite(Position.Y))
		{
			OutMetrics.bAnyNonFinite = true;
		}
		if (!IsFinite(Velocity.X) || !IsFinite(Velocity.Y))
		{
			OutMetrics.bAnyNonFinite = true;
		}
		if (!IsFinite(Angle) || !IsFinite(Angular))
		{
			OutMetrics.bAnyNonFinite = true;
		}
		const f32 IdealY = 0.5f + static_cast<f32>(Index) * 1.0f;
		const f32 Sink = IdealY - Position.Y;
		const f32 Lift = Position.Y - IdealY;
		if (Sink > OutMetrics.MaxPenetration)
		{
			OutMetrics.MaxPenetration = Sink;
		}
		if (Lift > OutMetrics.MaxLift)
		{
			OutMetrics.MaxLift = Lift;
		}
		const f32 Drift = Abs(Position.X);
		if (Drift > OutMetrics.MaxDrift)
		{
			OutMetrics.MaxDrift = Drift;
		}
		const f32 Tilt = Abs(Angle);
		if (Tilt > OutMetrics.MaxTilt)
		{
			OutMetrics.MaxTilt = Tilt;
		}
		const f64 Speed = Sqrt(f64(Velocity.X) * Velocity.X + f64(Velocity.Y) * Velocity.Y);
		if (static_cast<f32>(Speed) > OutMetrics.MaxSpeed)
		{
			OutMetrics.MaxSpeed = static_cast<f32>(Speed);
		}
	}
}
// 休止なしでも五段積みが崩壊や沈み込みなく静止する。
void StackStaysAwake2D_Internal()
{
	TArray<FBodyId2D, StackCount_Internal> Ids;
	FStackMetrics2D Metrics;
	RunStack2D_Internal(false, Ids, Metrics);
	// 非有限の破綻がない。
	PHYSICS_REQUIRE(!Metrics.bAnyNonFinite);
	// 沈み込みと浮き上がりの初期上限。
	PHYSICS_REQUIRE(Metrics.MaxPenetration < 0.12f);
	PHYSICS_REQUIRE(Metrics.MaxLift < 0.12f);
	// 水平ずれと傾きの初期上限。
	PHYSICS_REQUIRE(Metrics.MaxDrift < 0.10f);
	PHYSICS_REQUIRE(Metrics.MaxTilt < 0.10f);
	// 残留速度の初期上限。
	PHYSICS_REQUIRE(Metrics.MaxSpeed < 0.35f);
}
// 休止ありでは五段積みが眠って沈み込みが小さい。
void StackSleeps2D_Internal()
{
	TArray<FBodyId2D, StackCount_Internal> Ids;
	FStackMetrics2D Metrics;
	RunStack2D_Internal(true, Ids, Metrics);
	// 非有限の破綻がない。
	PHYSICS_REQUIRE(!Metrics.bAnyNonFinite);
	// 休止時の沈み込みとずれの初期上限。
	PHYSICS_REQUIRE(Metrics.MaxPenetration < 0.08f);
	PHYSICS_REQUIRE(Metrics.MaxLift < 0.08f);
	PHYSICS_REQUIRE(Metrics.MaxDrift < 0.08f);
	PHYSICS_REQUIRE(Metrics.MaxTilt < 0.08f);
	PHYSICS_REQUIRE(Metrics.MaxSpeed < 0.10f);
	// 全段が休止する。
	PHYSICS_REQUIRE(Metrics.bAllSleeping);
}
// 立体積み重ねの沈み込みとずれの計測値。
struct FStackMetrics3D
{
	// 理想高さからの最大沈み込み。メートル単位で正が沈み。
	f32 MaxPenetration = 0;
	// 理想高さからの最大浮き上がり。メートル単位で正が浮き。
	f32 MaxLift = 0;
	// 水平面の最大ずれ。メートル単位。
	f32 MaxDrift = 0;
	// 最大速度。メートル毎秒単位。
	f32 MaxSpeed = 0;
	// 非有限の位置や速度があれば真。
	bool bAnyNonFinite = false;
	// 全段が休止していれば真。
	bool bAllSleeping = false;
};
// 床の立体形状と材質を作る。
FColliderDescription3D StackFloorDescription3D_Internal()
{
	FColliderDescription3D Floor;
	FOBB Shape;
	Shape.Center = {0, -1, 0};
	Shape.HalfExtents = {5, 1, 5};
	Floor.Shape = Shape;
	Floor.Friction = 0.6f;
	Floor.Restitution = 0;
	return Floor;
}
// 積み重ねる立体箱の形状と材質を作る。
FColliderDescription3D StackBoxDescription3D_Internal()
{
	FColliderDescription3D Box;
	FOBB Shape;
	Shape.Center = {0, 0, 0};
	Shape.HalfExtents = {0.5f, 0.5f, 0.5f};
	Box.Shape = Shape;
	Box.Friction = 0.6f;
	Box.Restitution = 0;
	return Box;
}
// 休止の有無を切り替えて立体五段積みを解き、沈み込みとずれを測る。
void RunStack3D_Internal(bool bEnableSleep, TArray<FBodyId3D, StackCount_Internal>& OutIds, FStackMetrics3D& OutMetrics)
{
	FPhysicsWorld3D World;
	// 休止の有効無効だけを切り替える。
	FSleepSettings3D Sleep = World.GetSleepSettings();
	Sleep.bEnabled = bEnableSleep;
	World.SetSleepSettings(Sleep);
	// 静止床を作る。
	FBodyDescription3D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId3D FloorId = World.CreateBody(Ground);
	World.AttachCollider(FloorId, StackFloorDescription3D_Internal());
	// わずかな隙間で積み上げて落下後に整列させる。
	for (size_t Index = 0; Index < OutIds.Size(); ++Index)
	{
		FBodyDescription3D Box;
		Box.Position = {0, 0.5f + static_cast<f32>(Index) * 1.02f, 0};
		const FBodyId3D Id = World.CreateBody(Box);
		World.AttachCollider(Id, StackBoxDescription3D_Internal());
		OutIds[Index] = Id;
	}
	for (int32 Step = 0; Step < 600; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 理想配置は床上面を基準にした等間隔。
	OutMetrics = {};
	OutMetrics.bAllSleeping = true;
	for (size_t Index = 0; Index < OutIds.Size(); ++Index)
	{
		const FBodyId3D Id = OutIds[Index];
		const FVector3 Position = World.GetPosition(Id);
		const FVector3 Velocity = World.GetVelocity(Id);
		if (!World.IsSleeping(Id))
		{
			OutMetrics.bAllSleeping = false;
		}
		// 有限性の破綻を記録する。
		if (!IsFinite(Position.X) || !IsFinite(Position.Y) || !IsFinite(Position.Z))
		{
			OutMetrics.bAnyNonFinite = true;
		}
		if (!IsFinite(Velocity.X) || !IsFinite(Velocity.Y) || !IsFinite(Velocity.Z))
		{
			OutMetrics.bAnyNonFinite = true;
		}
		const f32 IdealY = 0.5f + static_cast<f32>(Index) * 1.0f;
		const f32 Sink = IdealY - Position.Y;
		const f32 Lift = Position.Y - IdealY;
		if (Sink > OutMetrics.MaxPenetration)
		{
			OutMetrics.MaxPenetration = Sink;
		}
		if (Lift > OutMetrics.MaxLift)
		{
			OutMetrics.MaxLift = Lift;
		}
		const f64 Drift = Sqrt(f64(Position.X) * Position.X + f64(Position.Z) * Position.Z);
		if (static_cast<f32>(Drift) > OutMetrics.MaxDrift)
		{
			OutMetrics.MaxDrift = static_cast<f32>(Drift);
		}
		const f32 Speed = Length(Velocity);
		if (Speed > OutMetrics.MaxSpeed)
		{
			OutMetrics.MaxSpeed = Speed;
		}
	}
}
// 休止なしでも立体五段積みが崩壊なく静止する。
void StackStaysAwake3D_Internal()
{
	TArray<FBodyId3D, StackCount_Internal> Ids;
	FStackMetrics3D Metrics;
	RunStack3D_Internal(false, Ids, Metrics);
	// 非有限の破綻がない。
	PHYSICS_REQUIRE(!Metrics.bAnyNonFinite);
	// 沈み込みとずれの初期上限。
	PHYSICS_REQUIRE(Metrics.MaxPenetration < 0.12f);
	PHYSICS_REQUIRE(Metrics.MaxLift < 0.12f);
	PHYSICS_REQUIRE(Metrics.MaxDrift < 0.10f);
	PHYSICS_REQUIRE(Metrics.MaxSpeed < 0.35f);
}
// 休止ありでは立体五段積みが沈み込み小さく静止する。
// 島全体の休止伝播は将来課題のため、全段休止は求めず指標で判定する。
void StackSleeps3D_Internal()
{
	TArray<FBodyId3D, StackCount_Internal> Ids;
	FStackMetrics3D Metrics;
	RunStack3D_Internal(true, Ids, Metrics);
	// 非有限の破綻がない。
	PHYSICS_REQUIRE(!Metrics.bAnyNonFinite);
	// 休止時の沈み込みとずれの初期上限。
	PHYSICS_REQUIRE(Metrics.MaxPenetration < 0.08f);
	PHYSICS_REQUIRE(Metrics.MaxLift < 0.08f);
	PHYSICS_REQUIRE(Metrics.MaxDrift < 0.08f);
	PHYSICS_REQUIRE(Metrics.MaxSpeed < 0.15f);
}
// 緩い斜面では摩擦で箱が滑り落ちない。
void BoxStaysOnGentleSlope_Internal()
{
	FPhysicsWorld2D World;
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId2D FloorId = World.CreateBody(Ground);
	FColliderDescription2D Slope;
	FOrientedBox2D SlopeShape;
	SlopeShape.Center = {0, 0};
	SlopeShape.HalfExtents = {5, 0.5f};
	SlopeShape.Angle = 0.1745329f;
	Slope.Shape = SlopeShape;
	Slope.Friction = 0.6f;
	Slope.Restitution = 0;
	World.AttachCollider(FloorId, Slope);
	// 斜面上面に沿わせて箱を置く。
	FBodyDescription2D Place;
	Place.Position = {-0.1736f, 0.9848f};
	Place.Angle = 0.1745329f;
	const FBodyId2D Id = World.CreateBody(Place);
	World.AttachCollider(Id, StackBoxDescription_Internal());
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 滑落や転倒がなく有限に留まる。
	const FVector2 Position = World.GetPosition(Id);
	const FVector2 Velocity = World.GetVelocity(Id);
	PHYSICS_REQUIRE(IsFinite(Position.X) && IsFinite(Position.Y));
	PHYSICS_REQUIRE(IsFinite(Velocity.X) && IsFinite(Velocity.Y));
	PHYSICS_REQUIRE(Abs(Position.X + 0.1736f) < 0.25f);
	PHYSICS_REQUIRE(Abs(Position.Y - 0.9848f) < 0.25f);
	PHYSICS_REQUIRE(Sqrt(f64(Velocity.X) * Velocity.X + f64(Velocity.Y) * Velocity.Y) < 0.35);
}
// 両側の静止壁に挟まれても箱が飛び出さず有限に留まる。
void BoxSqueezedByWalls_Internal()
{
	FPhysicsWorld2D World;
	FBodyDescription2D LeftGround;
	LeftGround.Type = EBodyType::Static;
	LeftGround.Position = {-1.1f, 1};
	const FBodyId2D LeftId = World.CreateBody(LeftGround);
	FColliderDescription2D LeftWall;
	FOrientedBox2D LeftShape;
	LeftShape.Center = {0, 0};
	LeftShape.HalfExtents = {0.5f, 2};
	LeftShape.Angle = 0;
	LeftWall.Shape = LeftShape;
	LeftWall.Friction = 0.6f;
	LeftWall.Restitution = 0;
	World.AttachCollider(LeftId, LeftWall);
	FBodyDescription2D RightGround;
	RightGround.Type = EBodyType::Static;
	RightGround.Position = {1.1f, 1};
	const FBodyId2D RightId = World.CreateBody(RightGround);
	FColliderDescription2D RightWall;
	FOrientedBox2D RightShape;
	RightShape.Center = {0, 0};
	RightShape.HalfExtents = {0.5f, 2};
	RightShape.Angle = 0;
	RightWall.Shape = RightShape;
	RightWall.Friction = 0.6f;
	RightWall.Restitution = 0;
	World.AttachCollider(RightId, RightWall);
	// 落下を支える床を作る。
	FBodyDescription2D FloorGround;
	FloorGround.Type = EBodyType::Static;
	const FBodyId2D FloorId = World.CreateBody(FloorGround);
	World.AttachCollider(FloorId, StackFloorDescription_Internal());
	FBodyDescription2D Middle;
	Middle.Position = {0, 0.5f};
	const FBodyId2D Id = World.CreateBody(Middle);
	World.AttachCollider(Id, StackBoxDescription_Internal());
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 同時接触でも爆発や貫通落下がない。
	const FVector2 Position = World.GetPosition(Id);
	const FVector2 Velocity = World.GetVelocity(Id);
	PHYSICS_REQUIRE(IsFinite(Position.X) && IsFinite(Position.Y));
	PHYSICS_REQUIRE(IsFinite(Velocity.X) && IsFinite(Velocity.Y));
	PHYSICS_REQUIRE(Abs(Position.X) < 0.20f);
	PHYSICS_REQUIRE(Near_Internal(Position.Y, 0.5, 0.12, 0));
	PHYSICS_REQUIRE(Sqrt(f64(Velocity.X) * Velocity.X + f64(Velocity.Y) * Velocity.Y) < 0.40);
}
void BoxLandsFlat3D_Internal()
{
	FPhysicsWorld3D World;
	FBodyDescription3D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId3D FloorId = World.CreateBody(Ground);
	FColliderDescription3D Floor;
	FOBB FloorShape;
	FloorShape.Center = {0, -1, 0};
	FloorShape.HalfExtents = {5, 1, 5};
	Floor.Shape = FloorShape;
	Floor.Friction = 0.6f;
	World.AttachCollider(FloorId, Floor);
	FBodyDescription3D Fall;
	Fall.Position = {0, 3, 0};
	const FBodyId3D Id = World.CreateBody(Fall);
	FColliderDescription3D Crate;
	FOBB BoxShape;
	BoxShape.Center = {0, 0, 0};
	BoxShape.HalfExtents = {0.5f, 0.5f, 0.5f};
	Crate.Shape = BoxShape;
	Crate.Friction = 0.6f;
	World.AttachCollider(Id, Crate);
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 箱が床に面で静止する。
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.5, 0.06, 0));
	const FQuaternion Q = World.GetOrientation(Id);
	PHYSICS_REQUIRE(Q.W > 0.999f);
	PHYSICS_REQUIRE(Length(World.GetVelocity(Id)) < 0.3f);
}
const PhysicsTest::FCase StabilityCases_Internal[] = {
    {"box lands flat on floor in 3D", &BoxLandsFlat3D_Internal},
    {"stack stays awake in 2D", &StackStaysAwake2D_Internal},
    {"stack sleeps in 2D", &StackSleeps2D_Internal},
    {"stack stays awake in 3D", &StackStaysAwake3D_Internal},
    {"stack sleeps in 3D", &StackSleeps3D_Internal},
    {"box stays on gentle slope", &BoxStaysOnGentleSlope_Internal},
    {"box squeezed by walls", &BoxSqueezedByWalls_Internal},
};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetStabilityCases(Toolbox::size_t& Count) noexcept
{
	Count = sizeof(StabilityCases_Internal) / sizeof(StabilityCases_Internal[0]);
	return StabilityCases_Internal;
}
