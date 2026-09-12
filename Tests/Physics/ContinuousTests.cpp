// SPDX-License-Identifier: NOASSERTION
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
bool Near_Internal(f64 A, f64 B, f64 Absolute, f64 Relative)
{
	const f64 Scale = 1 + (Abs(A) > Abs(B) ? Abs(A) : Abs(B));
	return Abs(A - B) <= Absolute + Relative * Scale;
}
// 薄い静止壁を作る。内側の面はx=0。
FColliderId2D ThinWall_Internal(FPhysicsWorld2D& World)
{
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId2D Id = World.CreateBody(Ground);
	FColliderDescription2D Wall;
	FOrientedBox2D Shape;
	Shape.Center = {0.05f, 0};
	Shape.HalfExtents = {0.05f, 2};
	Wall.Shape = Shape;
	Wall.Restitution = 1;
	return World.AttachCollider(Id, Wall);
}
// 高速円を作る。CCDを選択する。
FBodyId2D FastBall_Internal(FPhysicsWorld2D& World, Toolbox::f32 StartX, Toolbox::f32 Speed)
{
	FBodyDescription2D Ball;
	Ball.Position = {StartX, 0};
	Ball.Velocity = {Speed, 0};
	Ball.Mass = 1;
	Ball.bUseContinuous = true;
	const FBodyId2D Id = World.CreateBody(Ball);
	FColliderDescription2D Collider;
	FCircle2D Shape;
	Shape.Center = {0, 0};
	Shape.Radius = 0.25f;
	Collider.Shape = Shape;
	Collider.Restitution = 1;
	World.AttachCollider(Id, Collider);
	return Id;
}
void FastBallHitsThinWall_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FContinuousSettings2D Settings;
	Settings.bEnabled = true;
	World.SetContinuousSettings(Settings);
	ThinWall_Internal(World);
	const FBodyId2D Id = FastBall_Internal(World, -2, 20);
	for (int32 Step = 0; Step < 20; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 壁の内側面x=0に半径0.25で跳ね返り、すり抜けない。
	const FVector2 Rest = World.GetPosition(Id);
	PHYSICS_REQUIRE(Rest.X < 0.5 && Rest.X > -2.5);
	PHYSICS_REQUIRE(World.GetVelocity(Id).X < 0);
}
void DiscreteBallTunnelThroughWall_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	ThinWall_Internal(World);
	FBodyDescription2D Ball;
	Ball.Position = {-2, 0};
	Ball.Velocity = {100, 0};
	Ball.Mass = 1;
	const FBodyId2D Id = World.CreateBody(Ball);
	FColliderDescription2D Collider;
	FCircle2D Shape;
	Shape.Center = {0, 0};
	Shape.Radius = 0.25f;
	Collider.Shape = Shape;
	World.AttachCollider(Id, Collider);
	// 1/60刻みで1.667m進むと接触帯域を飛び越える。
	for (int32 Step = 0; Step < 5; ++Step)
	{
		World.Step(1.0 / 60.0);
	}
	// CCDなしでは薄壁をすり抜ける。対照実験。
	PHYSICS_REQUIRE(World.GetPosition(Id).X > 0.5);
}
void BothBodiesMovingCollide_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FContinuousSettings2D Settings;
	Settings.bEnabled = true;
	World.SetContinuousSettings(Settings);
	FBodyDescription2D Left;
	Left.Position = {-3, 0};
	Left.Velocity = {10, 0};
	Left.Mass = 1;
	Left.bUseContinuous = true;
	const FBodyId2D LeftId = World.CreateBody(Left);
	FBodyDescription2D Right;
	Right.Position = {3, 0};
	Right.Velocity = {-10, 0};
	Right.Mass = 1;
	Right.bUseContinuous = true;
	const FBodyId2D RightId = World.CreateBody(Right);
	FColliderDescription2D Elastic;
	FCircle2D Shape;
	Shape.Center = {0, 0};
	Shape.Radius = 0.5f;
	Elastic.Shape = Shape;
	Elastic.Restitution = 1;
	World.AttachCollider(LeftId, Elastic);
	World.AttachCollider(RightId, Elastic);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 等質量の弾性衝突で速度が交換される。
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(LeftId).X, -10, 0, 0.1));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(RightId).X, 10, 0, 0.1));
	PHYSICS_REQUIRE(World.GetPosition(LeftId).X < World.GetPosition(RightId).X);
}
void MultipleImpactsInOneStep_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FContinuousSettings2D Settings;
	Settings.bEnabled = true;
	World.SetContinuousSettings(Settings);
	// 左右に薄壁を置く。内側の面はx=±0.75。
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FBodyDescription2D Ground;
		Ground.Type = EBodyType::Static;
		const FBodyId2D Id = World.CreateBody(Ground);
		FColliderDescription2D Wall;
		FOrientedBox2D Shape;
		Shape.Center = {Side == 0 ? -0.8f : 0.8f, 0};
		Shape.HalfExtents = {0.05f, 2};
		Wall.Shape = Shape;
		Wall.Restitution = 1;
		World.AttachCollider(Id, Wall);
	}
	const FBodyId2D Id = FastBall_Internal(World, 0, 100);
	World.Step(1.0 / 60.0);
	// 1.667m進む間に両壁へ当たり、最終的に内側へ戻る。
	const FVector2 Rest = World.GetPosition(Id);
	PHYSICS_REQUIRE(Rest.X > -0.6 && Rest.X < 0.6);
	const FContinuousDiagnostics2D Diagnostics = World.GetContinuousDiagnostics();
	PHYSICS_REQUIRE(Diagnostics.HitsResolved >= 2);
}
void GrazingCornerMisses_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FContinuousSettings2D Settings;
	Settings.bEnabled = true;
	World.SetContinuousSettings(Settings);
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId2D WallId = World.CreateBody(Ground);
	FColliderDescription2D Wall;
	FOrientedBox2D Shape;
	Shape.Center = {2, -1};
	Shape.HalfExtents = {1, 1};
	Wall.Shape = Shape;
	World.AttachCollider(WallId, Wall);
	// 角(1,0)の上方0.3をかすめる軌道。半径0.25では接触しない。
	FBodyDescription2D Ball;
	Ball.Position = {-2, 0.3f};
	Ball.Velocity = {10, 0};
	Ball.Mass = 1;
	Ball.bUseContinuous = true;
	const FBodyId2D Id = World.CreateBody(Ball);
	FColliderDescription2D Collider;
	FCircle2D Circle;
	Circle.Center = {0, 0};
	Circle.Radius = 0.25f;
	Collider.Shape = Circle;
	World.AttachCollider(Id, Collider);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 接触せずに通過する。
	PHYSICS_REQUIRE(World.GetPosition(Id).X > 2.9);
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.3, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).X, 10, 1e-4, 0));
}
void SeparatingContactLeaves_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FContinuousSettings2D Settings;
	Settings.bEnabled = true;
	World.SetContinuousSettings(Settings);
	ThinWall_Internal(World);
	FBodyDescription2D Ball;
	Ball.Position = {-0.25f, 0};
	Ball.Velocity = {-5, 0};
	Ball.Mass = 1;
	Ball.bUseContinuous = true;
	const FBodyId2D Id = World.CreateBody(Ball);
	FColliderDescription2D Collider;
	FCircle2D Shape;
	Shape.Center = {0, 0};
	Shape.Radius = 0.25f;
	Collider.Shape = Shape;
	World.AttachCollider(Id, Collider);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 離れる物体はそのまま離れる。
	PHYSICS_REQUIRE(World.GetPosition(Id).X < -2);
}
void InitialOverlapWithCcdIsSafe_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FContinuousSettings2D Settings;
	Settings.bEnabled = true;
	World.SetContinuousSettings(Settings);
	ThinWall_Internal(World);
	FBodyDescription2D Ball;
	Ball.Position = {0, 0};
	Ball.Velocity = {5, 0};
	Ball.Mass = 1;
	Ball.bUseContinuous = true;
	const FBodyId2D Id = World.CreateBody(Ball);
	FColliderDescription2D Collider;
	FCircle2D Shape;
	Shape.Center = {0, 0};
	Shape.Radius = 0.25f;
	Collider.Shape = Shape;
	World.AttachCollider(Id, Collider);
	for (int32 Step = 0; Step < 120; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 初期貫通でも発散せず有限に収まる。
	PHYSICS_REQUIRE(World.GetPosition(Id).IsValid());
	PHYSICS_REQUIRE(World.GetVelocity(Id).IsValid());
}
void IterationCapReportsUnprocessed_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FContinuousSettings2D Settings;
	Settings.bEnabled = true;
	Settings.MaxIterations = 1;
	World.SetContinuousSettings(Settings);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FBodyDescription2D Ground;
		Ground.Type = EBodyType::Static;
		const FBodyId2D Id = World.CreateBody(Ground);
		FColliderDescription2D Wall;
		FOrientedBox2D Shape;
		Shape.Center = {Side == 0 ? -0.8f : 0.8f, 0};
		Shape.HalfExtents = {0.05f, 2};
		Wall.Shape = Shape;
		Wall.Restitution = 1;
		World.AttachCollider(Id, Wall);
	}
	const FBodyId2D Id = FastBall_Internal(World, 0, 100);
	World.Step(1.0 / 60.0);
	const FContinuousDiagnostics2D Diagnostics = World.GetContinuousDiagnostics();
	PHYSICS_REQUIRE(Diagnostics.UnprocessedSeconds > 0);
	const FVector2 Rest = World.GetPosition(Id);
	PHYSICS_REQUIRE(Rest.X > -0.8 && Rest.X < 0.8);
}
void RotatedWallFallsBackToDiscrete_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FContinuousSettings2D Settings;
	Settings.bEnabled = true;
	World.SetContinuousSettings(Settings);
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId2D WallId = World.CreateBody(Ground);
	FColliderDescription2D Wall;
	FOrientedBox2D Shape;
	Shape.Center = {0, 0};
	Shape.HalfExtents = {0.05f, 2};
	Shape.Angle = 0.5f;
	Wall.Shape = Shape;
	World.AttachCollider(WallId, Wall);
	const FBodyId2D Id = FastBall_Internal(World, -2, 10);
	for (int32 Step = 0; Step < 12; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	// 回転箱は線形CCDの対象外で、代替組として診断に出る。
	FContinuousDiagnostics2D Early = World.GetContinuousDiagnostics();
	PHYSICS_REQUIRE(Early.FallbackPairs > 0);
	for (int32 Step = 0; Step < 48; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(World.GetPosition(Id).IsValid());
}
void RestingUnaffectedByCcd_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, -9.8f});
	FContinuousSettings2D Settings;
	Settings.bEnabled = true;
	World.SetContinuousSettings(Settings);
	FBodyDescription2D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId2D FloorId = World.CreateBody(Ground);
	FColliderDescription2D Floor;
	FOrientedBox2D Shape;
	Shape.Center = {0, -1};
	Shape.HalfExtents = {5, 1};
	Floor.Shape = Shape;
	World.AttachCollider(FloorId, Floor);
	FBodyDescription2D Fall;
	Fall.Position = {0, 5};
	Fall.bUseContinuous = true;
	const FBodyId2D Id = World.CreateBody(Fall);
	FColliderDescription2D Ball;
	FCircle2D Circle;
	Circle.Center = {0, 0};
	Circle.Radius = 0.5f;
	Ball.Shape = Circle;
	World.AttachCollider(Id, Ball);
	for (int32 Step = 0; Step < 360; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 0.5, 0.1, 0));
}
void FastSphereHitsThinWall_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FContinuousSettings3D Settings;
	Settings.bEnabled = true;
	World.SetContinuousSettings(Settings);
	FBodyDescription3D Ground;
	Ground.Type = EBodyType::Static;
	const FBodyId3D WallId = World.CreateBody(Ground);
	FColliderDescription3D Wall;
	FOBB Box;
	Box.Center = {0.05f, 0, 0};
	Box.HalfExtents = {0.05f, 2, 2};
	Wall.Shape = Box;
	Wall.Restitution = 1;
	World.AttachCollider(WallId, Wall);
	FBodyDescription3D Ball;
	Ball.Position = {-2, 0, 0};
	Ball.Velocity = {20, 0, 0};
	Ball.Mass = 1;
	Ball.bUseContinuous = true;
	const FBodyId3D Id = World.CreateBody(Ball);
	FColliderDescription3D Collider;
	FSphere Sphere;
	Sphere.Center = {0, 0, 0};
	Sphere.Radius = 0.25f;
	Collider.Shape = Sphere;
	Collider.Restitution = 1;
	World.AttachCollider(Id, Collider);
	for (int32 Step = 0; Step < 20; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	const FVector3 Rest = World.GetPosition(Id);
	PHYSICS_REQUIRE(Rest.X < 0.5 && Rest.X > -2.5);
	PHYSICS_REQUIRE(World.GetVelocity(Id).X < 0);
}
void BothSpheresMovingCollide_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FContinuousSettings3D Settings;
	Settings.bEnabled = true;
	World.SetContinuousSettings(Settings);
	FBodyDescription3D Left;
	Left.Position = {-3, 0, 0};
	Left.Velocity = {10, 0, 0};
	Left.Mass = 1;
	Left.bUseContinuous = true;
	const FBodyId3D LeftId = World.CreateBody(Left);
	FBodyDescription3D Right;
	Right.Position = {3, 0, 0};
	Right.Velocity = {-10, 0, 0};
	Right.Mass = 1;
	Right.bUseContinuous = true;
	const FBodyId3D RightId = World.CreateBody(Right);
	FColliderDescription3D Elastic;
	FSphere Shape;
	Shape.Center = {0, 0, 0};
	Shape.Radius = 0.5f;
	Elastic.Shape = Shape;
	Elastic.Restitution = 1;
	World.AttachCollider(LeftId, Elastic);
	World.AttachCollider(RightId, Elastic);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(LeftId).X, -10, 0, 0.1));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(RightId).X, 10, 0, 0.1));
}
const PhysicsTest::FCase ContinuousCases_Internal[] = {
    {"fast ball hits thin wall with CCD", &FastBallHitsThinWall_Internal},
    {"discrete ball tunnels through thin wall", &DiscreteBallTunnelThroughWall_Internal},
    {"both moving bodies collide with CCD", &BothBodiesMovingCollide_Internal},
    {"multiple impacts resolve in one step", &MultipleImpactsInOneStep_Internal},
    {"grazing corner trajectory misses", &GrazingCornerMisses_Internal},
    {"separating contact leaves cleanly", &SeparatingContactLeaves_Internal},
    {"initial overlap with CCD stays safe", &InitialOverlapWithCcdIsSafe_Internal},
    {"iteration cap reports unprocessed time", &IterationCapReportsUnprocessed_Internal},
    {"rotated wall falls back to discrete", &RotatedWallFallsBackToDiscrete_Internal},
    {"resting contact unaffected by CCD", &RestingUnaffectedByCcd_Internal},
    {"fast sphere hits thin wall with CCD", &FastSphereHitsThinWall_Internal},
    {"both moving spheres collide with CCD", &BothSpheresMovingCollide_Internal},
};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetContinuousCases(Toolbox::size_t& Count) noexcept
{
	Count = sizeof(ContinuousCases_Internal) / sizeof(ContinuousCases_Internal[0]);
	return ContinuousCases_Internal;
}
