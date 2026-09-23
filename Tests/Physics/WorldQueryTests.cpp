// SPDX-License-Identifier: NOASSERTION
#include "TestCases.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/JobSystem.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 指定処理がPhysicsの失敗を返すか調べる。
template <typename F>
bool Throws_Internal(F&& Run)
{
	try
	{
		Run();
	}
	catch (const FException&)
	{
		return true;
	}
	return false;
}
// 指定Bodyへローカル球を取り付ける。
FColliderId3D Sphere_Internal(FPhysicsWorld3D& World, FBodyId3D Body, FVector3 Center = {}, f32 Radius = 1)
{
	// 登録するBodyまたは球の設定。
	FColliderDescription3D Description;
	Description.Shape = FSphere{Center, Radius};
	return World.AttachCollider(Body, Description);
}
// 空World、各Body種別、最近傍、接線、端点、内部と有限範囲を解析値で確認する。
void Geometry_Internal()
{
	// このケースだけが所有する実World。
	FPhysicsWorld3D World;
	PHYSICS_REQUIRE(!World.RaycastClosest({0, 0, 0}, {10, 0, 0}));
	// 登録するBodyまたは球の設定。
	FBodyDescription3D Description;
	Description.Type = EBodyType::Static;
	Description.Position = {7, 0, 0};
	// 遠い球の所有Body。
	const auto FarBody = World.CreateBody(Description);
	PHYSICS_REQUIRE(!World.RaycastClosest({0, 0, 0}, {10, 0, 0}));
	// 自己除外後に残る遠いCollider。
	const auto Far = Sphere_Internal(World, FarBody);
	Description.Type = EBodyType::Kinematic;
	Description.Position = {3, 0, 0};
	// 手前の球の所有Body。
	const auto NearBody = World.CreateBody(Description);
	// 最短として期待するCollider。
	const auto Near = Sphere_Internal(World, NearBody);
	// 実Worldから返された交点。
	const auto Hit = World.RaycastClosest({0, 0, 0}, {10, 0, 0});
	PHYSICS_REQUIRE(Hit && Hit->Collider == Near && Abs(Hit->Fraction - .2) < 1e-12 && Hit->Position == FVector3(2, 0, 0));
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0, 0}, {10, 0, 0}, NearBody)->Collider == Far);
	PHYSICS_REQUIRE(!World.RaycastClosest({0, 0, 0}, {1, 0, 0}));
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0, 0}, {2, 0, 0})->Fraction == 1);
	PHYSICS_REQUIRE(World.RaycastClosest({3, 0, 0}, {4, 0, 0})->Fraction == 0);
	PHYSICS_REQUIRE(Abs(World.RaycastClosest({0, 1, 0}, {5, 1, 0})->Fraction - .6) < 1e-12);
	Description.Type = EBodyType::Dynamic;
	Description.Position = {0, 5, 0};
	Description.Orientation = FQuaternion::FromAxisAngle({0, 0, 1}, 1.57079632679f);
	// 回転とローカル中心を検証するBody。
	const auto BoxBody = World.CreateBody(Description);
	// 箱Colliderの設定。
	FColliderDescription3D Box;
	Box.Shape = FOBB{{2, 0, 0}, {2, .5f, .5f}};
	// 回転箱の識別子。
	const auto BoxId = World.AttachCollider(BoxBody, Box);
	// 回転したローカル中心は(0,7,0)、X方向の半幅は0.5。
	const auto BoxHit = World.RaycastClosest({-3, 7, 0}, {3, 7, 0});
	PHYSICS_REQUIRE(BoxHit && BoxHit->Collider == BoxId && Abs(BoxHit->Fraction - 2.5 / 6) < 1e-6);
	PHYSICS_REQUIRE(Abs(BoxHit->Position.X + .5) < 1e-5 && Abs(BoxHit->Position.Y - 7) < 1e-5);
	// 同一Bodyへ追加したローカル球。
	const auto Offset = Sphere_Internal(World, BoxBody, {4, 0, 0}, .25f);
	PHYSICS_REQUIRE(World.RaycastClosest({-3, 8.5f, 0}, {3, 8.5f, 0})->Collider == BoxId);
	PHYSICS_REQUIRE(World.RaycastClosest({0, 10, 0}, {0, 9, 0})->Collider == Offset);
}
// 更新を挟まず移動・回転・着脱・世代変更が反映され、同距離はスロット順となる。
void ImmediateAndIdentity_Internal()
{
	// このケースだけが所有する実World。
	FPhysicsWorld3D World;
	// 操作または登録対象のBody。
	const auto Body = World.CreateBody({});
	// 同距離の先行Collider。
	const auto First = Sphere_Internal(World, Body, {3, 0, 0});
	// 同距離の後続Collider。
	const auto Second = Sphere_Internal(World, Body, {3, 0, 0});
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0, 0}, {10, 0, 0})->Collider == First);
	World.SetBodyTransform(Body, {0, 4, 0}, {});
	PHYSICS_REQUIRE(World.CaptureSnapshot().StepIndex == 0);
	PHYSICS_REQUIRE(!World.RaycastClosest({0, 0, 0}, {10, 0, 0}));
	World.SetBodyTransform(Body, {0, 0, 0}, FQuaternion::FromAxisAngle({0, 0, 1}, 1.57079632679f));
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0, 0}, {0, 10, 0})->Collider == First);
	World.DetachCollider(First);
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0, 0}, {0, 10, 0})->Collider == Second);
	// 空いたスロットの新しいCollider。
	const auto Reattached = Sphere_Internal(World, Body, {3, 0, 0});
	PHYSICS_REQUIRE(Reattached.Index == First.Index && Reattached.Generation != First.Generation);
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0, 0}, {0, 10, 0})->Collider == Reattached);
	PHYSICS_REQUIRE(!World.RaycastClosest({0, 0, 0}, {0, 10, 0}, Body));
	World.DestroyBody(Body);
	// 同じスロットを再利用する新世代Body。
	const auto NewBody = World.CreateBody({});
	PHYSICS_REQUIRE(NewBody.Index == Body.Index && NewBody.Generation != Body.Generation);
	PHYSICS_REQUIRE(!World.IsColliderAlive(Reattached));
	Sphere_Internal(World, NewBody);
	PHYSICS_REQUIRE(Throws_Internal([&]
	                                {
		                                World.RaycastClosest({0, 0, 0}, {0, 10, 0}, Body);
	                                }));
	PHYSICS_REQUIRE(Throws_Internal([&]
	                                {
		                                World.RaycastClosest({0, 0, 0}, {0, 10, 0}, FBodyId3D{});
	                                }));
	// 異なるWorld IDを持つ対照。
	FPhysicsWorld3D Other;
	// 除外指定を拒否する別WorldのID。
	const auto Foreign = Other.CreateBody({});
	PHYSICS_REQUIRE(Throws_Internal([&]
	                                {
		                                World.RaycastClosest({0, 0, 0}, {0, 10, 0}, Foreign);
	                                }));
}
// Debug表示の256件上限より後ろのColliderにも問い合わせる。
void Unlimited_Internal()
{
	// このケースだけが所有する実World。
	FPhysicsWorld3D World;
	// 操作または登録対象のBody。
	const auto Body = World.CreateBody({});
	for (int32 Index = 0; Index < 300; ++Index)
	{
		Sphere_Internal(World, Body, {0, 10, static_cast<f32>(Index)});
	}
	// 表示上限より後ろに登録する最短Collider。
	const auto Last = Sphere_Internal(World, Body, {5, 0, 0});
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0, 0}, {10, 0, 0})->Collider == Last);
}
// 不正入力は空Worldでも失敗し、最初のヒット後にある計算不能形状も隠さない。
void Invalid_Internal()
{
	// このケースだけが所有する実World。
	FPhysicsWorld3D World;
	// f32で表現できる最大値。
	const f32 Max = TNumericLimits<f32>::Max();
	// 入力拒否を検証する無限大。
	const f32 Inf = Max * 2;
	// 入力拒否を検証する非数値。
	const f32 Nan = TNumericLimits<f32>::QuietNaN();
	PHYSICS_REQUIRE(Throws_Internal([&]
	                                {
		                                World.RaycastClosest({}, {});
	                                }));
	PHYSICS_REQUIRE(Throws_Internal([&]
	                                {
		                                World.RaycastClosest({Nan, 0, 0}, {1, 0, 0});
	                                }));
	PHYSICS_REQUIRE(Throws_Internal([&]
	                                {
		                                World.RaycastClosest({}, {Inf, 0, 0});
	                                }));
	PHYSICS_REQUIRE(Throws_Internal([&]
	                                {
		                                World.RaycastClosest({-Max, 0, 0}, {Max, 0, 0});
	                                }));
	// 同距離の先行Collider。
	const auto First = World.CreateBody({});
	Sphere_Internal(World, First);
	// 変換時に桁あふれさせるBody設定。
	FBodyDescription3D Huge;
	Huge.Position = {Max, 0, 0};
	// 操作または登録対象のBody。
	const auto Body = World.CreateBody(Huge);
	Sphere_Internal(World, Body, {Max, 0, 0});
	PHYSICS_REQUIRE(Throws_Internal([&]
	                                {
		                                World.RaycastClosest({0, 0, 0}, {1, 0, 0});
	                                }));
	PHYSICS_REQUIRE(World.RaycastClosest({0, 0, 0}, {1, 0, 0}, Body)->Fraction == 0);
}
// 途中失敗したStepの状態を拒否し、正常更新で回復する。引数拒否は状態を壊さない。
void FailedStep_Internal()
{
	// このケースだけが所有する実World。
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	Sphere_Internal(World, World.CreateBody({}));
	PHYSICS_REQUIRE(Throws_Internal([&]
	                                {
		                                World.Step(0);
	                                }));
	PHYSICS_REQUIRE(World.RaycastClosest({-2, 0, 0}, {2, 0, 0}));
	FJobSystem Jobs(2);
	Jobs.Shutdown();
	// Step内部を失敗させる実行設定。
	FPhysicsExecutionSettings Broken;
	Broken.JobSystem = &Jobs;
	World.SetExecutionSettings(Broken);
	PHYSICS_REQUIRE(Throws_Internal([&]
	                                {
		                                World.Step(.25, 1);
	                                }));
	PHYSICS_REQUIRE(Throws_Internal([&]
	                                {
		                                World.RaycastClosest({-2, 0, 0}, {2, 0, 0});
	                                }));
	World.SetExecutionSettings({});
	World.Step(.5, 5);
	PHYSICS_REQUIRE(World.RaycastClosest({-2, 0, 0}, {2, 0, 0}));
	PHYSICS_REQUIRE(World.CaptureSnapshot().StepIndex == 1);
}
// 一方だけ反復問い合わせし、蓄積力・角運動・位置と更新回数が対照Worldに一致する。
void ReadOnly_Internal()
{
	// 問い合わせを反復する実World。
	FPhysicsWorld3D Queried;
	// 問い合わせを行わない対照World。
	FPhysicsWorld3D Control;
	Queried.SetGravity({0, 0, 0});
	Control.SetGravity({0, 0, 0});
	// 問い合わせ側のBody ID。
	const auto A = Queried.CreateBody({});
	// 対照側のBody ID。
	const auto B = Control.CreateBody({});
	Sphere_Internal(Queried, A);
	Sphere_Internal(Control, B);
	Queried.ApplyForce(A, {2, 3, 4});
	Control.ApplyForce(B, {2, 3, 4});
	Queried.ApplyTorque(A, {1, 2, 3});
	Control.ApplyTorque(B, {1, 2, 3});
	// const経由でのみ問い合わせる参照。
	const FPhysicsWorld3D& Read = Queried;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		PHYSICS_REQUIRE(Read.RaycastClosest({-2, 0, 0}, {2, 0, 0}));
	}
	PHYSICS_REQUIRE(Queried.CaptureSnapshot().StepIndex == 0);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		Read.RaycastClosest({-20, 0, 0}, {20, 0, 0});
		Queried.Step(.01);
		Control.Step(.01);
		PHYSICS_REQUIRE(Queried.GetPosition(A) == Control.GetPosition(B));
		PHYSICS_REQUIRE(Queried.GetVelocity(A) == Control.GetVelocity(B));
		PHYSICS_REQUIRE(Queried.GetAngularVelocity(A) == Control.GetAngularVelocity(B));
		// 問い合わせ側の現在姿勢。
		const auto RotationA = Queried.GetOrientation(A);
		// 対照側の現在姿勢。
		const auto RotationB = Control.GetOrientation(B);
		PHYSICS_REQUIRE(RotationA.X == RotationB.X && RotationA.Y == RotationB.Y && RotationA.Z == RotationB.Z && RotationA.W == RotationB.W);
		PHYSICS_REQUIRE(Queried.IsSleeping(A) == Control.IsSleeping(B));
	}
	PHYSICS_REQUIRE(Queried.CaptureSnapshot().StepIndex == 20);
}
// 接地して休止したBodyも選択でき、問い合わせ後も起床しない。
void Sleeping_Internal()
{
	// このケースだけが所有する実World。
	FPhysicsWorld3D World;
	// 休止に必要な静止床の設定。
	FBodyDescription3D Ground;
	Ground.Type = EBodyType::Static;
	Ground.Position = {0, -1, 0};
	// 箱Colliderの設定。
	FColliderDescription3D Box;
	Box.Shape = FOBB{{}, {5, 1, 5}};
	World.AttachCollider(World.CreateBody(Ground), Box);
	// 操作または登録対象のBody。
	FBodyDescription3D Body;
	Body.Position = {0, .5f, 0};
	// 休止を確認する動的Body。
	const auto Id = World.CreateBody(Body);
	Box.Shape = FOBB{{}, {.5f, .5f, .5f}};
	// 休止中でも選択できる箱のID。
	const auto Collider = World.AttachCollider(Id, Box);
	for (int32 Index = 0; Index < 600; ++Index)
	{
		World.Step(1.0 / 120.0);
	}
	PHYSICS_REQUIRE(World.IsSleeping(Id));
	// 問い合わせ前の休止位置。
	const auto Position = World.GetPosition(Id);
	for (int32 Index = 0; Index < 100; ++Index)
	{
		PHYSICS_REQUIRE(World.RaycastClosest({0, 3, 0}, {0, -2, 0})->Collider == Collider);
	}
	PHYSICS_REQUIRE(World.IsSleeping(Id) && World.GetPosition(Id) == Position);
	PHYSICS_REQUIRE(World.CaptureSnapshot().StepIndex == 600);
}
// 実Worldだけを使う問い合わせ回帰の一覧。
const PhysicsTest::FCase Cases_Internal[] = {
    {"world query analytic geometry", &Geometry_Internal},
    {"world query immediate state and generations", &ImmediateAndIdentity_Internal},
    {"world query beyond debug limit", &Unlimited_Internal},
    {"world query invalid input and transforms", &Invalid_Internal},
    {"world query failed step recovery", &FailedStep_Internal},
    {"world query preserves dynamics", &ReadOnly_Internal},
    {"world query preserves sleeping", &Sleeping_Internal}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetWorldQueryCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
