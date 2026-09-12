// SPDX-License-Identifier: NOASSERTION
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include <stdio.h>
using namespace Toolbox;
using namespace Dxf;
namespace
{
bool Near_Internal(f64 A, f64 B, f64 Absolute, f64 Relative)
{
	const f64 Scale = 1 + (Abs(A) > Abs(B) ? Abs(A) : Abs(B));
	return Abs(A - B) <= Absolute + Relative * Scale;
}
FColliderDescription2D FloorDescription_Internal()
{
	FColliderDescription2D Floor;
	FOrientedBox2D Shape;
	Shape.Center = {0, -1};
	Shape.HalfExtents = {5, 1};
	Shape.Angle = 0;
	Floor.Shape = Shape;
	Floor.Friction = 0.4f;
	Floor.Restitution = 0;
	return Floor;
}
FBodyId2D StaticFloor_Internal(FPhysicsWorld2D& World)
{
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId2D Id = World.CreateBody(Ground);
	World.AttachCollider(Id, FloorDescription_Internal());
	return Id;
}
FColliderDescription2D BallDescription_Internal(f32 Friction, f32 Restitution)
{
	FColliderDescription2D Ball;
	FCircle2D Shape;
	Shape.Center = {0, 0};
	Shape.Radius = 0.5f;
	Ball.Shape = Shape;
	Ball.Friction = Friction;
	Ball.Restitution = Restitution;
	return Ball;
}
void BallRestsOnFloor_Internal()
{
	FPhysicsWorld2D World;
	StaticFloor_Internal(World);
	FBodyDescription2D Fall;
	Fall.Position = {0, 5};
	const FBodyId2D Id = World.CreateBody(Fall);
	World.AttachCollider(Id, BallDescription_Internal(0.4f, 0));
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 半径0.5の円が床面に静止し、すり抜けない。
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.5, 0.1, 0));
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).Y) < 0.3);
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).X) < 0.3);
}
void FrictionlessSlideKeepsSpeed_Internal()
{
	FPhysicsWorld2D World;
	StaticFloor_Internal(World);
	FBodyDescription2D Slide;
	Slide.Position = {0, 0.5f};
	Slide.Velocity = {3, 0};
	const FBodyId2D Id = World.CreateBody(Slide);
	World.AttachCollider(Id, BallDescription_Internal(0, 0));
	for (int32 Step = 0; Step < 120; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).X, 3, 0.15, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.5, 0.1, 0));
}
void FrictionDeceleratesSlide_Internal()
{
	FPhysicsWorld2D World;
	StaticFloor_Internal(World);
	FBodyDescription2D Slide;
	Slide.Position = {0, 0.5f};
	Slide.Velocity = {3, 0};
	const FBodyId2D Id = World.CreateBody(Slide);
	FColliderDescription2D Grippy = BallDescription_Internal(1, 0);
	World.AttachCollider(Id, Grippy);
	for (int32 Step = 0; Step < 240; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 摩擦は滑りを転がりへ変える。慣性1・質量1・半径0.5では転がり速度0.6に収束する。
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).X, 0.6, 0.15, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).X + World.GetAngularVelocity(Id) * 0.5, 0, 0.1, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.5, 0.1, 0));
}
void BallSlidesDownSlope_Internal()
{
	FPhysicsWorld2D World;
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId2D FloorId = World.CreateBody(Ground);
	FColliderDescription2D Slope;
	FOrientedBox2D Shape;
	Shape.Center = {0, 0};
	Shape.HalfExtents = {5, 0.5f};
	Shape.Angle = 0.5235988f;
	Slope.Shape = Shape;
	Slope.Friction = 0;
	Slope.Restitution = 0;
	World.AttachCollider(FloorId, Slope);
	// 斜面の上面中央に接する位置へ円を置く。
	FBodyDescription2D Fall;
	Fall.Position = {-0.5f, 0.8660254f};
	const FBodyId2D Id = World.CreateBody(Fall);
	World.AttachCollider(Id, BallDescription_Internal(0, 0));
	for (int32 Step = 0; Step < 120; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 斜面に沿って左下へ滑る。
	const FVector2 Rest = World.GetPosition(Id);
	PHYSICS_REQUIRE(Rest.X < -1.5 && Rest.X > -4.5);
	PHYSICS_REQUIRE(Rest.Y < 0.5 && Rest.Y > -1.5);
}
void BallBouncesWithRestitution_Internal()
{
	FPhysicsWorld2D World;
	StaticFloor_Internal(World);
	FBodyDescription2D Fall;
	Fall.Position = {0, 2};
	const FBodyId2D Id = World.CreateBody(Fall);
	World.AttachCollider(Id, BallDescription_Internal(0, 0.8f));
	f64 Apex = 0;
	for (int32 Step = 0; Step < 600; ++Step)
	{
		World.Step(1.0 / 240.0);
		if (Step > 120)
		{
			const f64 Height = World.GetPosition(Id).Y;
			if (Height > Apex)
			{
				Apex = Height;
			}
		}
	}
	// 落下1.5mの衝突速度5.42へ反発0.8が掛かり、頂点は約1.46になる。
	PHYSICS_REQUIRE(Apex > 1.1 && Apex < 1.7);
}
void MassRatioCollisionSwapsMotion_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FBodyDescription2D HeavyDescription;
	HeavyDescription.Position = {-3, 0};
	HeavyDescription.Velocity = {2, 0};
	HeavyDescription.Mass = 10;
	const FBodyId2D Heavy = World.CreateBody(HeavyDescription);
	FBodyDescription2D LightDescription;
	LightDescription.Position = {3, 0};
	LightDescription.Mass = 1;
	const FBodyId2D Light = World.CreateBody(LightDescription);
	FColliderDescription2D Elastic = BallDescription_Internal(0, 1);
	World.AttachCollider(Heavy, Elastic);
	World.AttachCollider(Light, Elastic);
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 一次元弾性衝突の解析解に一致する。
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Heavy).X, 1.6363636, 0, 0.07));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Light).X, 3.6363636, 0, 0.07));
	// 運動量が保存される。
	const f64 Momentum = 10 * f64(World.GetVelocity(Heavy).X) + f64(World.GetVelocity(Light).X);
	PHYSICS_REQUIRE(Near_Internal(Momentum, 20, 0, 0.05));
}
void InitialPenetrationResolves_Internal()
{
	FPhysicsWorld2D World;
	StaticFloor_Internal(World);
	FBodyDescription2D Fall;
	Fall.Position = {0, 0.2f};
	const FBodyId2D Id = World.CreateBody(Fall);
	World.AttachCollider(Id, BallDescription_Internal(0.4f, 0));
	for (int32 Step = 0; Step < 240; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(World.GetPosition(Id).IsValid());
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.5, 0.12, 0));
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).Y) < 0.5);
}
void KinematicPlatformCarriesBall_Internal()
{
	FPhysicsWorld2D World;
	FBodyDescription2D PlatformDescription;
	PlatformDescription.Type = EBodyType::Kinematic;
	PlatformDescription.Velocity = {0, 1};
	const FBodyId2D Platform = World.CreateBody(PlatformDescription);
	FColliderDescription2D Deck;
	FOrientedBox2D Shape;
	Shape.Center = {0, 0};
	Shape.HalfExtents = {2, 0.5f};
	Shape.Angle = 0;
	Deck.Shape = Shape;
	Deck.Friction = 0.8f;
	Deck.Restitution = 0;
	World.AttachCollider(Platform, Deck);
	FBodyDescription2D Fall;
	Fall.Position = {0, 1};
	const FBodyId2D Id = World.CreateBody(Fall);
	World.AttachCollider(Id, BallDescription_Internal(0.8f, 0));
	for (int32 Step = 0; Step < 120; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 上昇する床に運ばれて円も上昇する。
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 2, 0.2, 0));
}
void RestitutionMixingIsSymmetric_Internal()
{
	const auto BounceApex_Internal = [](f32 BallRestitution, f32 FloorRestitution) {
		FPhysicsWorld2D World;
		FBodyDescription2D Ground;
		Ground.Type = EBodyType::Static;
		const FBodyId2D FloorId = World.CreateBody(Ground);
		FColliderDescription2D Floor = FloorDescription_Internal();
		Floor.Restitution = FloorRestitution;
		World.AttachCollider(FloorId, Floor);
		FBodyDescription2D Fall;
		Fall.Position = {0, 2};
		const FBodyId2D Id = World.CreateBody(Fall);
		World.AttachCollider(Id, BallDescription_Internal(0, BallRestitution));
		f64 Apex = 0;
		for (int32 Step = 0; Step < 600; ++Step)
		{
			World.Step(1.0 / 240.0);
			if (Step > 120)
			{
				const f64 Height = World.GetPosition(Id).Y;
				if (Height > Apex)
				{
					Apex = Height;
				}
			}
		}
		return Apex;
	};
	// 混合則が最大値なら入れ替え前後で頂点が一致する。
	const f64 Forward = BounceApex_Internal(0.8f, 0.2f);
	const f64 Swapped = BounceApex_Internal(0.2f, 0.8f);
	PHYSICS_REQUIRE(Near_Internal(Forward, Swapped, 0, 0.05));
	PHYSICS_REQUIRE(Forward > 1.1 && Forward < 1.7);
}
void ThinEdgeSupportsBall_Internal()
{
	FPhysicsWorld2D World;
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId2D FloorId = World.CreateBody(Ground);
	FColliderDescription2D Edge;
	FOrientedBox2D Shape;
	Shape.Center = {0, -0.1f};
	Shape.HalfExtents = {2, 0.1f};
	Shape.Angle = 0;
	Edge.Shape = Shape;
	Edge.Friction = 0.4f;
	Edge.Restitution = 0;
	World.AttachCollider(FloorId, Edge);
	FBodyDescription2D Fall;
	Fall.Position = {0, 3};
	const FBodyId2D Id = World.CreateBody(Fall);
	World.AttachCollider(Id, BallDescription_Internal(0.4f, 0));
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.5, 0.1, 0));
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).Y) < 0.3);
}
void DynamicBoxLandsFlat_Internal()
{
	FPhysicsWorld2D World;
	StaticFloor_Internal(World);
	FBodyDescription2D Fall;
	Fall.Position = {0, 3};
	Fall.Angle = 0.05f;
	const FBodyId2D Id = World.CreateBody(Fall);
	FColliderDescription2D Crate;
	FOrientedBox2D Shape;
	Shape.Center = {0, 0};
	Shape.HalfExtents = {0.5f, 0.5f};
	Shape.Angle = 0;
	Crate.Shape = Shape;
	Crate.Friction = 0.6f;
	Crate.Restitution = 0;
	World.AttachCollider(Id, Crate);
	for (int32 Step = 0; Step < 720; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 箱が床に面で静止する。
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.5, 0.06, 0));
	PHYSICS_REQUIRE(Abs(World.GetAngle(Id)) < 0.08);
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).Y) < 0.3);
}
FColliderDescription3D FloorDescription3D_Internal()
{
	FColliderDescription3D Floor;
	FOBB Shape;
	Shape.Center = {0, -1, 0};
	Shape.HalfExtents = {5, 1, 5};
	Floor.Shape = Shape;
	Floor.Friction = 0.4f;
	Floor.Restitution = 0;
	return Floor;
}
void SphereRestsOnFloor_Internal()
{
	FPhysicsWorld3D World;
	FBodyDescription3D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId3D FloorId = World.CreateBody(Ground);
	World.AttachCollider(FloorId, FloorDescription3D_Internal());
	FBodyDescription3D Fall;
	Fall.Position = {0, 5, 0};
	const FBodyId3D Id = World.CreateBody(Fall);
	FColliderDescription3D Ball;
	FSphere Shape;
	Shape.Center = {0, 0, 0};
	Shape.Radius = 0.5f;
	Ball.Shape = Shape;
	Ball.Friction = 0.4f;
	Ball.Restitution = 0;
	World.AttachCollider(Id, Ball);
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.5, 0.1, 0));
	PHYSICS_REQUIRE(Length(World.GetVelocity(Id)) < 0.3f);
}
void SpherePairElasticCollision_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D HeavyDescription;
	HeavyDescription.Position = {-3, 0, 0};
	HeavyDescription.Velocity = {2, 0, 0};
	HeavyDescription.Mass = 10;
	const FBodyId3D Heavy = World.CreateBody(HeavyDescription);
	FBodyDescription3D LightDescription;
	LightDescription.Position = {3, 0, 0};
	LightDescription.Mass = 1;
	const FBodyId3D Light = World.CreateBody(LightDescription);
	FColliderDescription3D Elastic;
	FSphere Shape;
	Shape.Center = {0, 0, 0};
	Shape.Radius = 1;
	Elastic.Shape = Shape;
	Elastic.Friction = 0;
	Elastic.Restitution = 1;
	World.AttachCollider(Heavy, Elastic);
	World.AttachCollider(Light, Elastic);
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Heavy).X, 1.6363636, 0, 0.07));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Light).X, 3.6363636, 0, 0.07));
}
void SphereFrictionDecelerates_Internal()
{
	FPhysicsWorld3D World;
	FBodyDescription3D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId3D FloorId = World.CreateBody(Ground);
	FColliderDescription3D Floor = FloorDescription3D_Internal();
	Floor.Friction = 1;
	World.AttachCollider(FloorId, Floor);
	FBodyDescription3D Slide;
	Slide.Position = {0, 0.5f, 0};
	Slide.Velocity = {0, 0, 3};
	const FBodyId3D Id = World.CreateBody(Slide);
	FColliderDescription3D Grippy;
	FSphere Shape;
	Shape.Center = {0, 0, 0};
	Shape.Radius = 0.5f;
	Grippy.Shape = Shape;
	Grippy.Friction = 1;
	Grippy.Restitution = 0;
	World.AttachCollider(Id, Grippy);
	for (int32 Step = 0; Step < 240; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(Length(World.GetVelocity(Id)) < 0.6f);
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.5, 0.12, 0));
}
void SphereBouncesWithRestitution_Internal()
{
	FPhysicsWorld3D World;
	FBodyDescription3D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId3D FloorId = World.CreateBody(Ground);
	FColliderDescription3D Floor = FloorDescription3D_Internal();
	World.AttachCollider(FloorId, Floor);
	FBodyDescription3D Fall;
	Fall.Position = {0, 2, 0};
	const FBodyId3D Id = World.CreateBody(Fall);
	FColliderDescription3D Ball;
	FSphere Shape;
	Shape.Center = {0, 0, 0};
	Shape.Radius = 0.5f;
	Ball.Shape = Shape;
	Ball.Friction = 0;
	Ball.Restitution = 0.8f;
	World.AttachCollider(Id, Ball);
	f64 Apex = 0;
	for (int32 Step = 0; Step < 600; ++Step)
	{
		World.Step(1.0 / 240.0);
		if (Step > 120)
		{
			const f64 Height = World.GetPosition(Id).Y;
			if (Height > Apex)
			{
				Apex = Height;
			}
		}
	}
	PHYSICS_REQUIRE(Apex > 1.1 && Apex < 1.7);
}
void KinematicRotatingBoxPushesBall_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FBodyDescription2D SpinnerDescription;
	SpinnerDescription.Type = EBodyType::Kinematic;
	SpinnerDescription.AngularVelocity = 2;
	const FBodyId2D Spinner = World.CreateBody(SpinnerDescription);
	FColliderDescription2D Paddle;
	FOrientedBox2D Shape;
	Shape.Center = {0, 0};
	Shape.HalfExtents = {1, 1};
	Shape.Angle = 0;
	Paddle.Shape = Shape;
	Paddle.Friction = 0;
	Paddle.Restitution = 0;
	World.AttachCollider(Spinner, Paddle);
	FBodyDescription2D BallDescription;
	BallDescription.Position = {1.7f, 0.3f};
	const FBodyId2D Id = World.CreateBody(BallDescription);
	World.AttachCollider(Id, BallDescription_Internal(0, 0));
	for (int32 Step = 0; Step < 240; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 回転する角に叩かれて円が外へ押し出される。
	const FVector2 Rest = World.GetPosition(Id);
	PHYSICS_REQUIRE(Rest.X * Rest.X + Rest.Y * Rest.Y > 2.2 * 2.2);
	PHYSICS_REQUIRE(Near_Internal(World.GetAngle(Spinner), 4, 1e-4, 0));
}
void ColliderLifetimeIsTracked_Internal()
{
	FPhysicsWorld2D World;
	const FBodyId2D Id = World.CreateBody({});
	const FColliderId2D Collider = World.AttachCollider(Id, BallDescription_Internal(0.5f, 0));
	PHYSICS_REQUIRE(World.DetachCollider(Collider));
	PHYSICS_REQUIRE(!World.DetachCollider(Collider));
	FColliderDescription2D Broken = BallDescription_Internal(-1, 0);
	bool bThrown = false;
	try
	{
		World.AttachCollider(Id, Broken);
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	PHYSICS_REQUIRE(bThrown);
	Broken = BallDescription_Internal(0.5f, 2);
	bThrown = false;
	try
	{
		World.AttachCollider(Id, Broken);
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	PHYSICS_REQUIRE(bThrown);
	// 剛体の破棄は取り付け済みのコライダーも失効させる。
	const FColliderId2D Second = World.AttachCollider(Id, BallDescription_Internal(0.5f, 0));
	PHYSICS_REQUIRE(World.DestroyBody(Id));
	PHYSICS_REQUIRE(!World.DetachCollider(Second));
	bThrown = false;
	try
	{
		World.AttachCollider(Id, BallDescription_Internal(0.5f, 0));
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	PHYSICS_REQUIRE(bThrown);
}
void ColliderLifetimeIsTracked3D_Internal()
{
	FPhysicsWorld3D World;
	const FBodyId3D Id = World.CreateBody({});
	FColliderDescription3D Ball;
	FSphere Shape;
	Shape.Center = {0, 0, 0};
	Shape.Radius = 0.5f;
	Ball.Shape = Shape;
	const FColliderId3D Collider = World.AttachCollider(Id, Ball);
	PHYSICS_REQUIRE(World.DetachCollider(Collider));
	PHYSICS_REQUIRE(!World.DetachCollider(Collider));
	Ball.Restitution = -0.5f;
	bool bThrown = false;
	try
	{
		World.AttachCollider(Id, Ball);
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	PHYSICS_REQUIRE(bThrown);
}
const PhysicsTest::FCase SolverCases_Internal[] = {
    {"ball rests on static floor", &BallRestsOnFloor_Internal},
    {"frictionless slide keeps tangential speed", &FrictionlessSlideKeepsSpeed_Internal},
    {"friction decelerates sliding ball", &FrictionDeceleratesSlide_Internal},
    {"ball slides down a slope", &BallSlidesDownSlope_Internal},
    {"ball bounces with restitution", &BallBouncesWithRestitution_Internal},
    {"mass ratio collision matches analysis", &MassRatioCollisionSwapsMotion_Internal},
    {"initial penetration resolves safely", &InitialPenetrationResolves_Internal},
    {"kinematic platform carries the ball", &KinematicPlatformCarriesBall_Internal},
    {"restitution mixing is symmetric", &RestitutionMixingIsSymmetric_Internal},
    {"thin edge supports the ball", &ThinEdgeSupportsBall_Internal},
    {"dynamic box lands flat on floor", &DynamicBoxLandsFlat_Internal},
    {"sphere rests on static floor", &SphereRestsOnFloor_Internal},
    {"sphere pair collides elastically", &SpherePairElasticCollision_Internal},
    {"sphere friction decelerates sliding", &SphereFrictionDecelerates_Internal},
    {"sphere bounces with restitution", &SphereBouncesWithRestitution_Internal},
    {"kinematic rotating box pushes the ball", &KinematicRotatingBoxPushesBall_Internal},
    {"collider lifetime is tracked", &ColliderLifetimeIsTracked_Internal},
    {"collider lifetime is tracked in 3D", &ColliderLifetimeIsTracked3D_Internal},
};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetSolverCases(Toolbox::size_t& Count) noexcept
{
	Count = sizeof(SolverCases_Internal) / sizeof(SolverCases_Internal[0]);
	return SolverCases_Internal;
}
