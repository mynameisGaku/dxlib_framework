// SPDX-License-Identifier: NOASSERTION
// 壁へ進む箱が、壁の面で止まり貫通しないこと（2D／3D）。3Dの箱どうしの接触でほぼ平行な辺の外積の軸を使わない修正の回帰。
// 床（上面y=0）・Dynamicの1m箱（x=2）・壁（x∈[2.5,4.5]）をこの順に作り、箱へ+Xの速度を与えて60回Stepする。
// 箱の右面は壁の左面（x=2.5）で止まるので、箱の中心は約2.0（接触の許容幅の分だけ内側まで）。
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
constexpr f64 StepSeconds = 1.0 / 60.0;

void BoxWall2D_Internal()
{
	for (f32 Speed : {1.0f, 4.0f})
	{
		FPhysicsWorld2D World;
		FBodyDescription2D Static;
		Static.Type = EBodyType::Static;
		Static.Position = {0, -0.5f};
		const auto Floor = World.CreateBody(Static);
		FColliderDescription2D FloorShape;
		FloorShape.Shape = FOrientedBox2D{{0, 0}, {50, 0.5f}, 0};
		World.AttachCollider(Floor, FloorShape);
		FBodyDescription2D Dynamic;
		Dynamic.Position = {2, 0.5f};
		Dynamic.Velocity = {Speed, 0};
		const auto Box = World.CreateBody(Dynamic);
		FColliderDescription2D BoxShape;
		BoxShape.Shape = FOrientedBox2D{{0, 0}, {0.5f, 0.5f}, 0};
		World.AttachCollider(Box, BoxShape);
		Static.Position = {3.5f, 2};
		const auto Wall = World.CreateBody(Static);
		FColliderDescription2D WallShape;
		WallShape.Shape = FOrientedBox2D{{0, 0}, {1, 2}, 0};
		World.AttachCollider(Wall, WallShape);
		for (int32 Index = 0; Index < 60; ++Index)
		{
			World.Step(StepSeconds);
			PHYSICS_REQUIRE(World.GetPosition(Box).X < 2.1f);
		}
		PHYSICS_REQUIRE(World.GetPosition(Box).X > 1.9f);
	}
}

void BoxWall3D_Internal()
{
	for (f32 Speed : {1.0f, 4.0f})
	{
		FPhysicsWorld3D World;
		FBodyDescription3D Static;
		Static.Type = EBodyType::Static;
		Static.Position = {0, -0.5f, 0};
		const auto Floor = World.CreateBody(Static);
		FColliderDescription3D FloorShape;
		FloorShape.Shape = FOBB{{0, 0, 0}, {50, 0.5f, 50}};
		World.AttachCollider(Floor, FloorShape);
		FBodyDescription3D Dynamic;
		Dynamic.Position = {2, 0.5f, 0};
		Dynamic.Velocity = {Speed, 0, 0};
		const auto Box = World.CreateBody(Dynamic);
		FColliderDescription3D BoxShape;
		BoxShape.Shape = FOBB{{0, 0, 0}, {0.5f, 0.5f, 0.5f}};
		World.AttachCollider(Box, BoxShape);
		// 壁のZの厚さは箱と同じ（面が重なり、辺がほぼ平行になる配置）。
		Static.Position = {3.5f, 2, 0};
		const auto Wall = World.CreateBody(Static);
		FColliderDescription3D WallShape;
		WallShape.Shape = FOBB{{0, 0, 0}, {1, 2, 0.5f}};
		World.AttachCollider(Wall, WallShape);
		for (int32 Index = 0; Index < 60; ++Index)
		{
			World.Step(StepSeconds);
			PHYSICS_REQUIRE(World.GetPosition(Box).X < 2.1f);
		}
		PHYSICS_REQUIRE(World.GetPosition(Box).X > 1.9f);
	}
}

const PhysicsTest::FCase Cases_Internal[] = {{"2D box stops at a wall face", &BoxWall2D_Internal},
                                             {"3D box stops at a wall face", &BoxWall3D_Internal}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetBoxWallContactCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
