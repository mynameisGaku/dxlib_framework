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
	// 巨大値と比較する微小値の両方へ対応する混合許容誤差。
	const f64 Scale = 1 + (Abs(A) > Abs(B) ? Abs(A) : Abs(B));
	return Abs(A - B) <= Absolute + Relative * Scale;
}
void Body2DRestsWithoutGravity_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FBodyDescription2D Description;
	Description.Position = {3, -2};
	Description.Angle = 0.5f;
	Description.Velocity = {0, 0};
	const FBodyId2D Id = World.CreateBody(Description);
	World.Step(1.0 / 60.0);
	PHYSICS_REQUIRE(World.IsAlive(Id));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).X, 3, 1e-6, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, -2, 1e-6, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetAngle(Id), 0.5, 1e-6, 0));
}
void Body2DUniformMotion_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FBodyDescription2D Description;
	Description.Velocity = {2, -1};
	const FBodyId2D Id = World.CreateBody(Description);
	for (int32 Step = 0; Step < 120; ++Step)
	{
		World.Step(1.0 / 60.0);
	}
	// 外力なしの半陰的Eulerは等速運動を丸め誤差の範囲で再現する。
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).X, 4, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, -2, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).X, 2, 1e-6, 0));
}
void Body2DGravityIgnoresMass_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, -9.8f});
	FBodyDescription2D Light;
	Light.Mass = 1;
	FBodyDescription2D Heavy;
	Heavy.Mass = 100;
	const FBodyId2D LightId = World.CreateBody(Light);
	const FBodyId2D HeavyId = World.CreateBody(Heavy);
	const f64 StepSeconds = 1.0 / 60.0;
	const int32 Steps = 60;
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		World.Step(StepSeconds);
	}
	// 速度は質量によらず重力加速度と時間の積になる。
	const f64 ExpectedVelocity = -9.8 * Steps * StepSeconds;
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(LightId).Y, ExpectedVelocity, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(HeavyId).Y, ExpectedVelocity, 1e-4, 0));
	// 位置は半陰的Eulerの閉形 h^2*n*(n+1)/2 に従い、連続解とは h*t/2 だけずれる。
	const f64 ExpectedPosition = -9.8 * StepSeconds * StepSeconds * Steps * (Steps + 1) * 0.5;
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(LightId).Y, ExpectedPosition, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(HeavyId).Y, ExpectedPosition, 1e-4, 0));
}
void Body2DFreeFallConvergesLinearly_Internal()
{
	const auto Drop_Internal = [](f64 StepSeconds) {
		FPhysicsWorld2D World;
		World.SetGravity({0, -9.8f});
		const FBodyId2D Id = World.CreateBody({});
		const int32 Steps = static_cast<int32>(1.0 / StepSeconds);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			World.Step(StepSeconds);
		}
		return World.GetPosition(Id).Y;
	};
	const f64 Analytic = -9.8 * 0.5;
	const f64 CoarseError = Abs(Drop_Internal(1.0 / 60.0) - Analytic);
	const f64 FineError = Abs(Drop_Internal(1.0 / 480.0) - Analytic);
	PHYSICS_REQUIRE(CoarseError > 0 && FineError > 0);
	// 一次収束なら刻み1/8で誤差は約1/8になる。実装の退化だけを検出する幅を持たせる。
	const f64 Ratio = CoarseError / FineError;
	PHYSICS_REQUIRE(Ratio > 4 && Ratio < 16);
}
void Body3DRestsWithoutGravity_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D Description;
	Description.Position = {1, 2, 3};
	const FBodyId3D Id = World.CreateBody(Description);
	World.Step(1.0 / 60.0);
	PHYSICS_REQUIRE(World.IsAlive(Id));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).X, 1, 1e-6, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 2, 1e-6, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Z, 3, 1e-6, 0));
}
void Body3DUniformMotion_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D Description;
	Description.Velocity = {1, 2, 3};
	const FBodyId3D Id = World.CreateBody(Description);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		World.Step(1.0 / 60.0);
	}
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).X, 1, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Y, 2, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).Z, 3, 1e-4, 0));
}
void Body3DGravityIgnoresMass_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, -9.8f, 0});
	FBodyDescription3D Light;
	Light.Mass = 2;
	FBodyDescription3D Heavy;
	Heavy.Mass = 50;
	const FBodyId3D LightId = World.CreateBody(Light);
	const FBodyId3D HeavyId = World.CreateBody(Heavy);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		World.Step(1.0 / 60.0);
	}
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(LightId).Y, -9.8, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(HeavyId).Y, -9.8, 1e-4, 0));
}
template <typename TFunc> bool Throws_Internal(TFunc Func)
{
	try
	{
		Func();
	}
	catch (const FException&)
	{
		return true;
	}
	return false;
}
void Body2DForceAcceleratesAndClears_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FBodyDescription2D Description;
	Description.Mass = 2;
	const FBodyId2D Id = World.CreateBody(Description);
	World.ApplyForce(Id, {10, 0});
	World.Step(1.0);
	// 加速度は力割る質量で、更新後は力が消えるため等速になる。
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).X, 5, 1e-4, 0));
	World.Step(1.0);
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).X, 5, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(Id).X, 10, 1e-4, 0));
}
void Body2DForceHeldAcrossSubSteps_Internal()
{
	FPhysicsWorld2D Split;
	Split.SetGravity({0, 0});
	FBodyDescription2D Description;
	Description.Mass = 4;
	const FBodyId2D SplitId = Split.CreateBody(Description);
	Split.ApplyForce(SplitId, {8, 0});
	Split.Step(0.5, 8);
	FPhysicsWorld2D Whole;
	Whole.SetGravity({0, 0});
	const FBodyId2D WholeId = Whole.CreateBody(Description);
	Whole.ApplyForce(WholeId, {8, 0});
	Whole.Step(0.5, 1);
	// 分割数によらず同じ更新の力は一度分の速度変化になる。
	PHYSICS_REQUIRE(Near_Internal(Split.GetVelocity(SplitId).X, 1, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(Whole.GetVelocity(WholeId).X, 1, 1e-5, 0));
}
void Body2DImpulseActsOnce_Internal()
{
	FPhysicsWorld2D Single;
	Single.SetGravity({0, 0});
	FBodyDescription2D Description;
	Description.Mass = 2;
	const FBodyId2D SingleId = Single.CreateBody(Description);
	Single.ApplyLinearImpulse(SingleId, {6, 0});
	Single.Step(1.0, 1);
	FPhysicsWorld2D Many;
	Many.SetGravity({0, 0});
	const FBodyId2D ManyId = Many.CreateBody(Description);
	Many.ApplyLinearImpulse(ManyId, {6, 0});
	Many.Step(1.0, 16);
	// 力積は即時反映で分割数に比例しないし、二重にも作用しない。
	PHYSICS_REQUIRE(Near_Internal(Single.GetVelocity(SingleId).X, 3, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(Many.GetVelocity(ManyId).X, 3, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(Single.GetPosition(SingleId).X, 3, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(Many.GetPosition(ManyId).X, 3, 1e-4, 0));
}
void Body2DOffCenterImpulseRotates_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	FBodyDescription2D Description;
	Description.Mass = 1;
	Description.Inertia = 2;
	const FBodyId2D Id = World.CreateBody(Description);
	World.ApplyImpulseAtPoint(Id, {0, 4}, {1, 0});
	// 重心への並進は力積割る質量、回転は腕の外積割る慣性。
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).Y, 4, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetAngularVelocity(Id), 2, 1e-5, 0));
	World.ApplyImpulseAtPoint(Id, {0, 4}, {0, 0});
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).Y, 8, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetAngularVelocity(Id), 2, 1e-5, 0));
	// 回転は等角速度で進み、力積の再適用は起きない。
	World.Step(1.0);
	PHYSICS_REQUIRE(Near_Internal(World.GetAngularVelocity(Id), 2, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetAngle(Id), 2, 1e-4, 0));
}
void Body2DStaticAndKinematic_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, -9.8f});
	FBodyDescription2D StaticDescription;
	StaticDescription.Type = EBodyType::Static;
	StaticDescription.Position = {5, 5};
	const FBodyId2D StaticId = World.CreateBody(StaticDescription);
	FBodyDescription2D KinematicDescription;
	KinematicDescription.Type = EBodyType::Kinematic;
	KinematicDescription.Velocity = {1, 2};
	KinematicDescription.AngularVelocity = 0.5f;
	const FBodyId2D KinematicId = World.CreateBody(KinematicDescription);
	World.Step(1.0);
	// Staticは重力でも動かず、速度指定と外力を拒否する。
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(StaticId).X, 5, 1e-6, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(StaticId).Y, 5, 1e-6, 0));
	PHYSICS_REQUIRE(Throws_Internal([&] { World.SetVelocity(StaticId, {1, 0}); }));
	PHYSICS_REQUIRE(Throws_Internal([&] { World.ApplyForce(StaticId, {1, 0}); }));
	PHYSICS_REQUIRE(Throws_Internal([&] { World.ApplyLinearImpulse(StaticId, {1, 0}); }));
	// Kinematicは重力を無視して指定運動だけを行う。
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(KinematicId).X, 1, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(KinematicId).Y, 2, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetAngle(KinematicId), 0.5, 1e-5, 0));
	PHYSICS_REQUIRE(Throws_Internal([&] { World.ApplyForce(KinematicId, {1, 0}); }));
}
void Body2DInvalidInputsThrow_Internal()
{
	FPhysicsWorld2D World;
	FPhysicsWorld2D Other;
	FBodyDescription2D Description;
	Description.Mass = 0;
	PHYSICS_REQUIRE(Throws_Internal([&] { World.CreateBody(Description); }));
	Description.Mass = 1;
	Description.Inertia = -1;
	PHYSICS_REQUIRE(Throws_Internal([&] { World.CreateBody(Description); }));
	Description.Inertia = 1;
	Description.LinearDamping = -0.5f;
	PHYSICS_REQUIRE(Throws_Internal([&] { World.CreateBody(Description); }));
	Description.LinearDamping = 0;
	const FBodyId2D Id = World.CreateBody(Description);
	PHYSICS_REQUIRE(Throws_Internal([&] { World.Step(0); }));
	PHYSICS_REQUIRE(Throws_Internal([&] { World.Step(-0.01); }));
	PHYSICS_REQUIRE(Throws_Internal([&] { World.Step(0.01, 0); }));
	// 削除後のIDは無効になり、別ワールドのIDも受け付けない。
	PHYSICS_REQUIRE(World.DestroyBody(Id));
	PHYSICS_REQUIRE(!World.IsAlive(Id));
	PHYSICS_REQUIRE(!World.DestroyBody(Id));
	PHYSICS_REQUIRE(Throws_Internal([&] { World.GetPosition(Id); }));
	PHYSICS_REQUIRE(!Other.DestroyBody(Id));
	PHYSICS_REQUIRE(Throws_Internal([&] { Other.GetPosition(Id); }));
	// スロット再使用後は新しい世代だけが有効になる。
	const FBodyId2D Next = World.CreateBody(Description);
	PHYSICS_REQUIRE(World.IsAlive(Next));
	PHYSICS_REQUIRE(!(Next == Id));
	PHYSICS_REQUIRE(Throws_Internal([&] { World.GetPosition(Id); }));
}
void Body3DForceAndTorque_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D Description;
	Description.Mass = 2;
	Description.DiagonalInertia = {1, 1, 1};
	const FBodyId3D Id = World.CreateBody(Description);
	World.ApplyForce(Id, {0, 8, 0});
	World.ApplyTorque(Id, {0, 0, 4});
	World.Step(2.0);
	// 並進は力割る質量、回転はワールド逆慣性で等方なのでトルクそのまま。
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).Y, 8, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetAngularVelocity(Id).Z, 8, 1e-4, 0));
	World.Step(1.0);
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).Y, 8, 1e-4, 0));
}
void Body3DImpulseAtPointRotates_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D Description;
	Description.Mass = 1;
	Description.DiagonalInertia = {2, 2, 2};
	const FBodyId3D Id = World.CreateBody(Description);
	World.ApplyImpulseAtPoint(Id, {0, 0, 6}, {0, 2, 0});
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).Z, 6, 1e-5, 0));
	// 腕(0,2,0)と力積(0,0,6)の外積は(12,0,0)で、慣性2で割って6。
	PHYSICS_REQUIRE(Near_Internal(World.GetAngularVelocity(Id).X, 6, 1e-4, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetAngularVelocity(Id).Y, 0, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetAngularVelocity(Id).Z, 0, 1e-5, 0));
}
void Body3DFreeRotationConservesMomentum_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D Description;
	Description.DiagonalInertia = {1, 2, 3};
	Description.AngularVelocity = {1, 0.5f, 0.25f};
	const FBodyId3D Id = World.CreateBody(Description);
	// 初期角運動量の大きさ。初期姿勢が無回転なので対角成分を直接掛ける。
	const f64 Initial = Sqrt(1.0 + 1.0 + 0.5625);
	for (int32 Step = 0; Step < 600; ++Step)
	{
		World.Step(1.0 / 240.0);
	}
	// 外力なしでは角運動量ベクトルが保存されるため大きさも保たれる。
	const FVector3 Momentum = World.GetAngularMomentum(Id);
	const f64 Final = Sqrt(f64(Momentum.X) * Momentum.X + f64(Momentum.Y) * Momentum.Y +
	                       f64(Momentum.Z) * Momentum.Z);
	PHYSICS_REQUIRE(Near_Internal(Final, Initial, 0, 2e-2));
	// 姿勢は単位四元数を保つ。
	const FQuaternion Q = World.GetOrientation(Id);
	const f64 Norm = Sqrt(f64(Q.X) * Q.X + f64(Q.Y) * Q.Y + f64(Q.Z) * Q.Z + f64(Q.W) * Q.W);
	PHYSICS_REQUIRE(Near_Internal(Norm, 1, 1e-6, 0));
}
void Body3DStaticAndKinematic_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, -9.8f, 0});
	FBodyDescription3D StaticDescription;
	StaticDescription.Type = EBodyType::Static;
	StaticDescription.Position = {4, 4, 4};
	const FBodyId3D StaticId = World.CreateBody(StaticDescription);
	FBodyDescription3D KinematicDescription;
	KinematicDescription.Type = EBodyType::Kinematic;
	KinematicDescription.Velocity = {0, 3, 0};
	KinematicDescription.AngularVelocity = {0, 0, 1};
	const FBodyId3D KinematicId = World.CreateBody(KinematicDescription);
	// 陽解法の刻み誤差を抑えるため1秒を60分割して進める。
	World.Step(1.0, 60);
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(StaticId).X, 4, 1e-6, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(StaticId).Y, 4, 1e-6, 0));
	PHYSICS_REQUIRE(Throws_Internal([&] { World.ApplyTorque(StaticId, {0, 0, 1}); }));
	PHYSICS_REQUIRE(Near_Internal(World.GetPosition(KinematicId).Y, 3, 1e-4, 0));
	// Z軸回りに1ラジアン回転した姿勢になる。
	const FQuaternion Turned = World.GetOrientation(KinematicId);
	const FQuaternion Expected = FQuaternion::FromAxisAngle({0, 0, 1}, 1);
	const f32 Agreement = Abs(Turned.X * Expected.X + Turned.Y * Expected.Y + Turned.Z * Expected.Z +
	                          Turned.W * Expected.W);
	PHYSICS_REQUIRE(Agreement > 1 - 1e-4f);
}
void Body3DInvalidInputsThrow_Internal()
{
	FPhysicsWorld3D World;
	FBodyDescription3D Description;
	Description.DiagonalInertia = {0, 1, 1};
	PHYSICS_REQUIRE(Throws_Internal([&] { World.CreateBody(Description); }));
	Description.DiagonalInertia = {1, 1, 1};
	Description.Orientation = {0, 0, 0, 0};
	PHYSICS_REQUIRE(Throws_Internal([&] { World.CreateBody(Description); }));
	Description.Orientation = {};
	const FBodyId3D Id = World.CreateBody(Description);
	PHYSICS_REQUIRE(Throws_Internal([&] { World.Step(TNumericLimits<f64>::QuietNaN()); }));
	PHYSICS_REQUIRE(Throws_Internal([&] { World.ApplyForce(Id, {0, TNumericLimits<f32>::QuietNaN(), 0}); }));
	PHYSICS_REQUIRE(World.DestroyBody(Id));
	PHYSICS_REQUIRE(Throws_Internal([&] { World.GetOrientation(Id); }));
}
void Body2DDampingAndGravityScale_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, -10});
	FBodyDescription2D Damped;
	Damped.Velocity = {10, 0};
	Damped.LinearDamping = 2;
	const FBodyId2D DampedId = World.CreateBody(Damped);
	FBodyDescription2D Weightless;
	Weightless.Velocity = {3, 4};
	Weightless.GravityScale = 0;
	const FBodyId2D WeightlessId = World.CreateBody(Weightless);
	World.Step(0.5);
	// 減衰は 1/(1+d*h) の係数で、重力はその後に加わる。
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(DampedId).X, 5, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(DampedId).Y, -5, 1e-5, 0));
	// 倍率ゼロは重力を無視して等速運動する。
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(WeightlessId).X, 3, 1e-6, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(WeightlessId).Y, 4, 1e-6, 0));
	World.Step(0.5);
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(DampedId).X, 2.5, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(DampedId).Y, -7.5, 1e-5, 0));
}
void Body3DDampingReducesSpeed_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	FBodyDescription3D Description;
	Description.Velocity = {6, 0, 0};
	Description.AngularVelocity = {0, 4, 0};
	Description.LinearDamping = 1;
	Description.AngularDamping = 1;
	const FBodyId3D Id = World.CreateBody(Description);
	World.Step(1.0);
	PHYSICS_REQUIRE(Near_Internal(World.GetVelocity(Id).X, 3, 1e-5, 0));
	PHYSICS_REQUIRE(Near_Internal(World.GetAngularVelocity(Id).Y, 2, 1e-5, 0));
}
const PhysicsTest::FCase RigidBodyCases_Internal[] = {
    {"2D body rests without gravity", &Body2DRestsWithoutGravity_Internal},
    {"2D body moves uniformly without forces", &Body2DUniformMotion_Internal},
    {"2D gravity acceleration ignores mass", &Body2DGravityIgnoresMass_Internal},
    {"2D free fall converges linearly with step", &Body2DFreeFallConvergesLinearly_Internal},
    {"3D body rests without gravity", &Body3DRestsWithoutGravity_Internal},
    {"3D body moves uniformly without forces", &Body3DUniformMotion_Internal},
    {"3D gravity acceleration ignores mass", &Body3DGravityIgnoresMass_Internal},
    {"2D force accelerates and clears after step", &Body2DForceAcceleratesAndClears_Internal},
    {"2D force is held across sub steps", &Body2DForceHeldAcrossSubSteps_Internal},
    {"2D impulse acts once regardless of sub steps", &Body2DImpulseActsOnce_Internal},
    {"2D off-center impulse rotates the body", &Body2DOffCenterImpulseRotates_Internal},
    {"2D static and kinematic bodies behave", &Body2DStaticAndKinematic_Internal},
    {"2D invalid inputs throw without changing state", &Body2DInvalidInputsThrow_Internal},
    {"3D force and torque accelerate the body", &Body3DForceAndTorque_Internal},
    {"3D off-center impulse rotates the body", &Body3DImpulseAtPointRotates_Internal},
    {"3D free rotation conserves angular momentum", &Body3DFreeRotationConservesMomentum_Internal},
    {"3D static and kinematic bodies behave", &Body3DStaticAndKinematic_Internal},
    {"3D invalid inputs throw without changing state", &Body3DInvalidInputsThrow_Internal},
    {"2D damping and gravity scale behave", &Body2DDampingAndGravityScale_Internal},
    {"3D damping reduces speeds", &Body3DDampingReducesSpeed_Internal},
};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetRigidBodyCases(Toolbox::size_t& Count) noexcept
{
	Count = sizeof(RigidBodyCases_Internal) / sizeof(RigidBodyCases_Internal[0]);
	return RigidBodyCases_Internal;
}
