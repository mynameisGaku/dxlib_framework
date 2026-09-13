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
// 横長床の上に箱を置く。箱の初期中心位置を指定する。
FBodyId2D PlaceBoxOnWideFloor_Internal(FPhysicsWorld2D& World, f32 X, f32 Y)
{
	FBodyDescription2D Fall;
	Fall.Position = {X, Y};
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
	return Id;
}
// 箱が床上面の高さで静止しているか調べる。
void RequireBoxRestsOnFloor_Internal(FPhysicsWorld2D& World, FBodyId2D Id, f32 X)
{
	// 床上面はY=0なので箱中心の理想高さは0.5。
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.5, 0.08, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).X, X, 0.12, 0));
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).X) < 0.3);
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).Y) < 0.3);
}
void WideFloorSupportsCenteredBox_Internal()
{
	FPhysicsWorld2D World;
	StaticFloor_Internal(World);
	const FBodyId2D Id = PlaceBoxOnWideFloor_Internal(World, 0, 0.5f);
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 中央では接線座標が床半幅より十分小さい。
	RequireBoxRestsOnFloor_Internal(World, Id, 0);
}
void WideFloorSupportsOffsetBox_Internal()
{
	FPhysicsWorld2D World;
	StaticFloor_Internal(World);
	const FBodyId2D Id = PlaceBoxOnWideFloor_Internal(World, 3, 0.5f);
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 床上面の接線範囲は半幅5なので端が3.5でも接触が残る。
	RequireBoxRestsOnFloor_Internal(World, Id, 3);
}
void WideFloorSupportsOffsetBoxReversedOrder_Internal()
{
	FPhysicsWorld2D World;
	const FBodyId2D Id = PlaceBoxOnWideFloor_Internal(World, 3, 0.5f);
	StaticFloor_Internal(World);
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 登録順を反転しても正準順序の接触が床支持する。
	RequireBoxRestsOnFloor_Internal(World, Id, 3);
}
void WideFloorSupportsBoxNearEnd_Internal()
{
	FPhysicsWorld2D World;
	StaticFloor_Internal(World);
	const FBodyId2D Id = PlaceBoxOnWideFloor_Internal(World, 4, 0.5f);
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 箱の右端4.5は床の右端5の内側にある。
	RequireBoxRestsOnFloor_Internal(World, Id, 4);
}
// 縦長壁の側面で箱の初期貫通を解く。壁面付近のY座標を指定する。
FBodyId2D PlacePenetratingBoxNearWall_Internal(FPhysicsWorld2D& World, f32 Y)
{
	FBodyDescription2D WallGround;
	WallGround.Type = EBodyType::Static;
	WallGround.Position = {1.5f, 2};
	const FBodyId2D WallId = World.CreateBody(WallGround);
	FColliderDescription2D Wall;
	FOrientedBox2D WallShape;
	WallShape.Center = {0, 0};
	WallShape.HalfExtents = {0.5f, 3};
	WallShape.Angle = 0;
	Wall.Shape = WallShape;
	Wall.Friction = 0.6f;
	Wall.Restitution = 0;
	World.AttachCollider(WallId, Wall);
	// 箱の右端1.05は壁の左面1.0へ0.05だけ貫通する。
	FBodyDescription2D Push;
	Push.Position = {0.55f, Y};
	const FBodyId2D Id = World.CreateBody(Push);
	FColliderDescription2D Crate;
	FOrientedBox2D BoxShape;
	BoxShape.Center = {0, 0};
	BoxShape.HalfExtents = {0.5f, 0.5f};
	BoxShape.Angle = 0;
	Crate.Shape = BoxShape;
	Crate.Friction = 0.6f;
	Crate.Restitution = 0;
	World.AttachCollider(Id, Crate);
	return Id;
}
void TallWallPushesOutBoxNearTop_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	const FBodyId2D Id = PlacePenetratingBoxNearWall_Internal(World, 4.5f);
	for (int32 Step = 0; Step < 120; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 壁側面の接線範囲は高さ半分3なので上端付近の接触が残り押し戻す。
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).X, 0.5, 0.04, 0));
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).X) < 0.1);
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).Y) < 0.1);
}
void TallWallPushesOutBoxNearBottom_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	const FBodyId2D Id = PlacePenetratingBoxNearWall_Internal(World, -0.5f);
	for (int32 Step = 0; Step < 120; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 下端付近でも同じく押し戻す。
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).X, 0.5, 0.04, 0));
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).X) < 0.1);
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).Y) < 0.1);
}
void NarrowColumnSupportsBox_Internal()
{
	FPhysicsWorld2D World;
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	Ground.Position = {0, -2};
	const FBodyId2D ColumnId = World.CreateBody(Ground);
	FColliderDescription2D Column;
	FOrientedBox2D ColumnShape;
	ColumnShape.Center = {0, 0};
	ColumnShape.HalfExtents = {1, 5};
	ColumnShape.Angle = 0;
	Column.Shape = ColumnShape;
	Column.Friction = 0.6f;
	Column.Restitution = 0;
	World.AttachCollider(ColumnId, Column);
	// 柱上面はY=3なので箱中心の理想高さは3.5。
	const FBodyId2D Id = PlaceBoxOnWideFloor_Internal(World, 0, 3.5f);
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 3.5, 0.08, 0));
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).X) < 0.3);
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).Y) < 0.3);
}
void RotatedFloorSupportsOffsetBox_Internal()
{
	FPhysicsWorld2D World;
	const f64 Angle = 0.5;
	const f64 Sine = Sin(Angle);
	const f64 Cosine = Cos(Angle);
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	Ground.Position = {0, -1};
	Ground.Angle = static_cast<f32>(Angle);
	const FBodyId2D FloorId = World.CreateBody(Ground);
	FColliderDescription2D Slope;
	FOrientedBox2D SlopeShape;
	SlopeShape.Center = {0, 0};
	SlopeShape.HalfExtents = {5, 1};
	SlopeShape.Angle = 0;
	Slope.Shape = SlopeShape;
	Slope.Friction = 0.6f;
	Slope.Restitution = 0;
	World.AttachCollider(FloorId, Slope);
	// 床上面の中心と法線・接線を回転から求める。
	const f64 NormalX = -Sine;
	const f64 NormalY = Cosine;
	const f64 TangentX = Cosine;
	const f64 TangentY = Sine;
	const f64 FaceX = NormalX * 1.0;
	const f64 FaceY = -1.0 + NormalY * 1.0;
	// 接線方向へ3だけずらし、法線方向へ箱半分と隙間を空ける。
	FBodyDescription2D Fall;
	Fall.Position = {static_cast<f32>(FaceX + NormalX * 0.52 + TangentX * 3.0),
	                 static_cast<f32>(FaceY + NormalY * 0.52 + TangentY * 3.0)};
	Fall.Angle = static_cast<f32>(Angle);
	const FBodyId2D Id = World.CreateBody(Fall);
	FColliderDescription2D Crate;
	FOrientedBox2D BoxShape;
	BoxShape.Center = {0, 0};
	BoxShape.HalfExtents = {0.5f, 0.5f};
	BoxShape.Angle = 0;
	Crate.Shape = BoxShape;
	Crate.Friction = 0.6f;
	Crate.Restitution = 0;
	World.AttachCollider(Id, Crate);
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 法線距離は箱半分、接線位置はほぼ動かない。
	const f64 GapX = f64(World.GetPosition(Id).X) - FaceX;
	const f64 GapY = f64(World.GetPosition(Id).Y) - FaceY;
	const f64 NormalGap = GapX * NormalX + GapY * NormalY;
	const f64 TangentShift = GapX * TangentX + GapY * TangentY;
	PHYSICS_REQUIRE(Near_Internal(NormalGap, 0.5, 0.10, 0));
	PHYSICS_REQUIRE(Near_Internal(TangentShift, 3.0, 0.30, 0));
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).X) < 0.4);
	PHYSICS_REQUIRE(Abs(World.GetVelocity(Id).Y) < 0.4);
}
void SeparatedBoxesStayPut_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	StaticFloor_Internal(World);
	const FBodyId2D Id = PlaceBoxOnWideFloor_Internal(World, 0, 5);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 離れた箱に偽の接触は生まれない。
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).X, 0, 1e-6, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 5, 1e-6, 0));
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
    {"wide floor supports centered box", &WideFloorSupportsCenteredBox_Internal},
    {"wide floor supports offset box", &WideFloorSupportsOffsetBox_Internal},
    {"wide floor supports offset box with reversed creation order", &WideFloorSupportsOffsetBoxReversedOrder_Internal},
    {"wide floor supports box near its end", &WideFloorSupportsBoxNearEnd_Internal},
    {"tall wall pushes out box near its top", &TallWallPushesOutBoxNearTop_Internal},
    {"tall wall pushes out box near its bottom", &TallWallPushesOutBoxNearBottom_Internal},
    {"narrow column supports box", &NarrowColumnSupportsBox_Internal},
    {"rotated floor supports offset box", &RotatedFloorSupportsOffsetBox_Internal},
    {"separated boxes stay put", &SeparatedBoxesStayPut_Internal},
};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetSolverCases(Toolbox::size_t& Count) noexcept
{
	Count = sizeof(SolverCases_Internal) / sizeof(SolverCases_Internal[0]);
	return SolverCases_Internal;
}
