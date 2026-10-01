// SPDX-License-Identifier: NOASSERTION
// 重力0、質量1kg、慣性1kg m²、dt=1/60s。Anchor1mm、姿勢1mrad、速度0.001。
#include "TestCases.h"
#include "DistanceJointTestSupport.h"
#include "Toolbox/Platform.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Dxf/MechanismConstraintMath.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
constexpr f64 H = 1.0 / 60.0;
struct FObserved
{
	f64 Anchor = 0;
	f64 Angular = 0;
	f64 Coordinate = 0;
	f64 Rate = 0;
};
// 2Dの公開APIへ接続するfixture。
struct FPlane : PhysicsTest::JointTest::F2D
{
	using World = FPhysicsWorld2D;
	using BodyId = FBodyId2D;
	using JointId = FJointId2D;
	using Vector = FVector2;
	static Vector At(f32 X, f32 Y)
	{
		return {X, Y};
	}
	static BodyId Body(World& W, EBodyType Type, Vector Position, bool bRotated = false)
	{
		FBodyDescription2D D;
		D.Type = Type;
		D.Position = Position;
		D.Angle = bRotated ? 0.8f : 0;
		return W.CreateBody(D);
	}
	static void Configure(World& W)
	{
		W.SetGravity({});
		FSleepSettings2D Settings;
		Settings.bEnabled = false;
		W.SetSleepSettings(Settings);
	}
	static void AngularImpulse(World& W, BodyId Id, f32 Value)
	{
		W.ApplyAngularImpulse(Id, Value);
	}
	template <EJointKind Kind>
	static JointId Join(World& W, BodyId A, BodyId B)
	{
		if constexpr (Kind == EJointKind::Revolute)
		{
			return W.CreateRevoluteJoint(A, B, W.MakeRevoluteJointDescription(A, B, {}));
		}
		else if constexpr (Kind == EJointKind::Fixed)
		{
			return W.CreateFixedJoint(A, B, W.MakeFixedJointDescription(A, B, {}));
		}
		else
		{
			return W.CreatePrismaticJoint(A, B, W.MakePrismaticJointDescription(A, B, {}));
		}
	}
	template <EJointKind Kind>
	static FObserved Observe(World& W, JointId Id)
	{
		if constexpr (Kind == EJointKind::Revolute)
		{
			const auto S = W.GetRevoluteJoint(Id);
			return {S.AnchorError, S.AxisAlignmentError, S.Angle, S.AngularSpeed};
		}
		else if constexpr (Kind == EJointKind::Fixed)
		{
			const auto S = W.GetFixedJoint(Id);
			return {S.AnchorError, S.OrientationError, 0, 0};
		}
		else
		{
			const auto S = W.GetPrismaticJoint(Id);
			return {S.AnchorError, S.OrientationError, S.Translation, S.TranslationRate};
		}
	}
};
// 3Dの公開APIへ接続するfixture。
struct FSpace : PhysicsTest::JointTest::F3D
{
	using World = FPhysicsWorld3D;
	using BodyId = FBodyId3D;
	using JointId = FJointId3D;
	using Vector = FVector3;
	static Vector At(f32 X, f32 Y)
	{
		return {X, Y, 0};
	}
	static BodyId Body(World& W, EBodyType Type, Vector Position, bool bRotated = false)
	{
		FBodyDescription3D D;
		D.Type = Type;
		D.Position = Position;
		D.Orientation = bRotated ? FQuaternion::FromAxisAngle({1, 2, 3}, 0.8f) : FQuaternion{};
		return W.CreateBody(D);
	}
	static void Configure(World& W)
	{
		W.SetGravity({});
		FSleepSettings3D Settings;
		Settings.bEnabled = false;
		W.SetSleepSettings(Settings);
	}
	static void AngularImpulse(World& W, BodyId Id, f32 Value)
	{
		W.ApplyAngularImpulse(Id, {0, 0, Value});
	}
	template <EJointKind Kind>
	static JointId Join(World& W, BodyId A, BodyId B)
	{
		if constexpr (Kind == EJointKind::Revolute)
		{
			return W.CreateRevoluteJoint(A, B, W.MakeRevoluteJointDescription(A, B, {}));
		}
		else if constexpr (Kind == EJointKind::Fixed)
		{
			return W.CreateFixedJoint(A, B, W.MakeFixedJointDescription(A, B, {}));
		}
		else
		{
			return W.CreatePrismaticJoint(A, B, W.MakePrismaticJointDescription(A, B, {}));
		}
	}
	template <EJointKind Kind>
	static FObserved Observe(World& W, JointId Id)
	{
		if constexpr (Kind == EJointKind::Revolute)
		{
			const auto S = W.GetRevoluteJoint(Id);
			return {S.AnchorError, S.AxisAlignmentError, S.Angle, S.AngularSpeed};
		}
		else if constexpr (Kind == EJointKind::Fixed)
		{
			const auto S = W.GetFixedJoint(Id);
			return {S.AnchorError, S.OrientationError, 0, 0};
		}
		else
		{
			const auto S = W.GetPrismaticJoint(Id);
			return {S.AnchorError, S.OrientationError, S.Translation, S.TranslationRate};
		}
	}
};
template <typename S, EJointKind Kind>
void Basic()
{
	typename S::World W;
	S::Configure(W);
	const auto A = S::Body(W, EBodyType::Static, S::At(0, 0), true);
	const auto B = S::Body(W, EBodyType::Dynamic, S::At(1, 0.5f), true);
	const auto Id = S::template Join<Kind>(W, A, B);
	PHYSICS_REQUIRE(W.GetJointKind(Id) == Kind);
	W.ApplyLinearImpulse(B, S::At(0.4f, 0.5f));
	S::AngularImpulse(W, B, 0.3f);
	for (uint32 I = 0; I < 600; ++I)
	{
		W.Step(H);
		const auto State = S::template Observe<Kind>(W, Id);
		PHYSICS_REQUIRE(IsFinite(State.Anchor) && IsFinite(State.Angular));
	}
	const auto State = S::template Observe<Kind>(W, Id);
	PHYSICS_REQUIRE(State.Anchor < 0.001 && State.Angular < 0.001);
	if constexpr (Kind != EJointKind::Fixed)
	{
		PHYSICS_REQUIRE(Abs(State.Coordinate) > 0.02);
	}
}
template <typename S, EJointKind Kind>
void Lifetime()
{
	typename S::World W;
	S::Configure(W);
	const auto A = S::Body(W, EBodyType::Static, S::At(0, 0));
	const auto B = S::Body(W, EBodyType::Dynamic, S::At(1, 0));
	const auto First = S::template Join<Kind>(W, A, B);
	bool bWrong = false;
	try
	{
		(void)W.GetDistanceJoint(First);
	}
	catch (const FException&)
	{
		bWrong = true;
	}
	PHYSICS_REQUIRE(bWrong && W.DestroyJoint(First));
	const auto Second = S::template Join<EJointKind::Fixed>(W, A, B);
	PHYSICS_REQUIRE(First.Index == Second.Index && First.Generation != Second.Generation && !W.IsJointAlive(First));
	PHYSICS_REQUIRE(W.DestroyBody(B) && !W.IsJointAlive(Second) && W.IsAlive(A));
}
template <typename S>
void DynamicFixed()
{
	typename S::World W;
	S::Configure(W);
	const auto A = S::Body(W, EBodyType::Dynamic, S::At(-1, 0), true);
	const auto B = S::Body(W, EBodyType::Dynamic, S::At(1, 0), true);
	const auto Id = S::template Join<EJointKind::Fixed>(W, A, B);
	W.ApplyLinearImpulse(B, S::At(2, 0));
	for (uint32 I = 0; I < 60; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(Abs(W.GetVelocity(A).X - 1) < 0.001 && Abs(W.GetVelocity(B).X - 1) < 0.001);
	PHYSICS_REQUIRE(S::template Observe<EJointKind::Fixed>(W, Id).Anchor < 0.001);
	PHYSICS_REQUIRE(W.DestroyJoint(Id));
	W.ApplyLinearImpulse(B, S::At(1, 0));
	W.Step(H);
	PHYSICS_REQUIRE(W.GetVelocity(B).X > W.GetVelocity(A).X + 0.5);
}
void UnitImpulse()
{
	using namespace Dxf::PhysicsPrivate;
	FMechanismBody A;
	FMechanismBody B;
	A.InverseMass = 0.5;
	B.InverseMass = 1;
	A.InverseInertia = {1, 2, 4};
	B.InverseInertia = {3, 1, 2};
	// Y90度を手計算したワールド逆慣性はdiag(4,2,1)。
	A.Rotation = {0, Sqrt(0.5), 0, Sqrt(0.5)};
	FMechanismRow R;
	R.bUsed = true;
	R.LinearA = {-1, 0, 0};
	R.LinearB = {1, 0, 0};
	R.AngularA = {1, 2, 3};
	R.AngularB = {-2, 1, 0.5};
	const f64 Expected = 0.5 + 1 + 4 + 8 + 9 + 12 + 1 + 0.5;
	PHYSICS_REQUIRE(Abs(EffectiveMass(R, A, B) - Expected) < 1e-10);
	ApplyRow(R, A, B, 1, false);
	const f64 Measured = -A.Velocity.X + B.Velocity.X + A.Angular.X + 2 * A.Angular.Y + 3 * A.Angular.Z - 2 * B.Angular.X + B.Angular.Y + 0.5 * B.Angular.Z;
	PHYSICS_REQUIRE(Abs(Measured - Expected) < 1e-10);
}
void MovingAxisDerivative()
{
	using namespace Dxf::PhysicsPrivate;
	FMechanismBody A;
	FMechanismBody B;
	A.Angular = {0, 0, 0.7};
	A.Velocity = {0.2, 0.4, 0};
	B.Position = {2, 3, 0};
	B.Velocity = {2, 0.3, 0};
	FMechanismSettings S;
	const auto Rows = BuildMechanismRows(EJointKind::Prismatic, true, S, A, B, H);
	PHYSICS_REQUIRE(Abs(Rows.Rate - 3.9) < 1e-12);
	const f64 E = 1e-7;
	const f64 Predicted = Cos(0.7 * E) * (2 + 1.8 * E) + Sin(0.7 * E) * (3 - 0.1 * E);
	PHYSICS_REQUIRE(Abs((Predicted - 2) / E - Rows.Rate) < 1e-6);
}
void RevoluteBudget2D()
{
	const uint32 IterationValues[] = {1, 8, 32};
	const uint32 SubStepValues[] = {1, 4};
	for (const uint32 Iterations : IterationValues)
	{
		for (const uint32 SubSteps : SubStepValues)
		{
			FPlane::World W;
			FPlane::Configure(W);
			FContactSettings2D C;
			C.VelocityIterations = Iterations;
			W.SetContactSettings(C);
			const auto A = FPlane::Body(W, EBodyType::Static, {});
			const auto B = FPlane::Body(W, EBodyType::Dynamic, {});
			auto D = W.MakeRevoluteJointDescription(A, B, {});
			D.Drive = {true, 100, 6};
			const auto Id = W.CreateRevoluteJoint(A, B, D);
			W.Step(H, SubSteps);
			PHYSICS_REQUIRE(Abs(W.GetRevoluteJoint(Id).AngularSpeed - 0.1) < 1e-4);
			W.SetRevoluteJointDrive(Id, {false, 100, 6});
			W.Step(H);
			PHYSICS_REQUIRE(Abs(W.GetRevoluteJoint(Id).AngularSpeed - 0.1) < 1e-4);
		}
	}
}
void RevoluteLimits2D()
{
	FPlane::World W;
	FPlane::Configure(W);
	const auto A = FPlane::Body(W, EBodyType::Static, {});
	const auto B = FPlane::Body(W, EBodyType::Dynamic, {});
	auto D = W.MakeRevoluteJointDescription(A, B, {});
	D.Drive = {true, 2, 10};
	D.Limits = {true, -0.4, 0.4};
	const auto Id = W.CreateRevoluteJoint(A, B, D);
	for (uint32 I = 0; I < 120; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(W.GetRevoluteJoint(Id).Angle <= 0.401 && W.GetRevoluteJoint(Id).Angle >= 0.39);
	W.SetRevoluteJointDrive(Id, {true, -2, 10});
	W.Step(H);
	PHYSICS_REQUIRE(W.GetRevoluteJoint(Id).AngularSpeed < -0.1);
	for (uint32 I = 0; I < 120; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(W.GetRevoluteJoint(Id).Angle >= -0.401 && W.GetRevoluteJoint(Id).Angle <= -0.39);
	W.SetRevoluteJointLimits(Id, {true, 0.1, 0.1});
	for (uint32 I = 0; I < 60; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(Abs(W.GetRevoluteJoint(Id).Angle - 0.1) < 0.001);
	PHYSICS_REQUIRE(W.GetRevoluteJoint(Id).LimitState == EJointLimitState::Locked);
	bool bRejected = false;
	try
	{
		W.SetRevoluteJointDrive(Id, {true, 1, -1});
	}
	catch (const FException&)
	{
		bRejected = true;
	}
	PHYSICS_REQUIRE(bRejected && W.IsJointAlive(Id) && W.GetRevoluteJoint(Id).Drive.TargetAngularSpeed == -2);
}
void PrismaticBudget2D()
{
	const uint32 IterationValues[] = {1, 8, 32};
	const uint32 SubStepValues[] = {1, 4};
	for (const uint32 Iterations : IterationValues)
	{
		for (const uint32 SubSteps : SubStepValues)
		{
			FPlane::World W;
			FPlane::Configure(W);
			FContactSettings2D C;
			C.VelocityIterations = Iterations;
			W.SetContactSettings(C);
			const auto A = FPlane::Body(W, EBodyType::Static, {});
			const auto B = FPlane::Body(W, EBodyType::Dynamic, {});
			auto D = W.MakePrismaticJointDescription(A, B, {});
			D.Drive = {true, 100, 6};
			const auto Id = W.CreatePrismaticJoint(A, B, D);
			W.Step(H, SubSteps);
			PHYSICS_REQUIRE(Abs(W.GetPrismaticJoint(Id).TranslationRate - 0.1) < 1e-4);
			W.SetPrismaticJointDrive(Id, {false, 100, 6});
			W.Step(H);
			PHYSICS_REQUIRE(Abs(W.GetPrismaticJoint(Id).TranslationRate - 0.1) < 1e-4);
		}
	}
}
void PrismaticLimits2D()
{
	FPlane::World W;
	FPlane::Configure(W);
	const auto A = FPlane::Body(W, EBodyType::Static, {});
	const auto B = FPlane::Body(W, EBodyType::Dynamic, {});
	auto D = W.MakePrismaticJointDescription(A, B, {});
	D.Drive = {true, 2, 10};
	D.Limits = {true, -0.4, 0.4};
	const auto Id = W.CreatePrismaticJoint(A, B, D);
	for (uint32 I = 0; I < 120; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(W.GetPrismaticJoint(Id).Translation <= 0.401 && W.GetPrismaticJoint(Id).Translation >= 0.39);
	W.SetPrismaticJointDrive(Id, {true, -2, 10});
	W.Step(H);
	PHYSICS_REQUIRE(W.GetPrismaticJoint(Id).TranslationRate < -0.1);
	for (uint32 I = 0; I < 120; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(W.GetPrismaticJoint(Id).Translation >= -0.401 && W.GetPrismaticJoint(Id).Translation <= -0.39);
	W.SetPrismaticJointLimits(Id, {true, 0.1, 0.1});
	for (uint32 I = 0; I < 60; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(Abs(W.GetPrismaticJoint(Id).Translation - 0.1) < 0.001);
	PHYSICS_REQUIRE(W.GetPrismaticJoint(Id).LimitState == EJointLimitState::Locked);
	bool bRejected = false;
	try
	{
		W.SetPrismaticJointDrive(Id, {true, 1, -1});
	}
	catch (const FException&)
	{
		bRejected = true;
	}
	PHYSICS_REQUIRE(bRejected && W.IsJointAlive(Id) && W.GetPrismaticJoint(Id).Drive.TargetSpeed == -2);
}
void RevoluteBudget3D()
{
	const uint32 IterationValues[] = {1, 8, 32};
	const uint32 SubStepValues[] = {1, 4};
	for (const uint32 Iterations : IterationValues)
	{
		for (const uint32 SubSteps : SubStepValues)
		{
			FSpace::World W;
			FSpace::Configure(W);
			FContactSettings3D C;
			C.VelocityIterations = Iterations;
			W.SetContactSettings(C);
			const auto A = FSpace::Body(W, EBodyType::Static, {});
			const auto B = FSpace::Body(W, EBodyType::Dynamic, {});
			auto D = W.MakeRevoluteJointDescription(A, B, {});
			D.Drive = {true, 100, 6};
			const auto Id = W.CreateRevoluteJoint(A, B, D);
			W.Step(H, SubSteps);
			PHYSICS_REQUIRE(Abs(W.GetRevoluteJoint(Id).AngularSpeed - 0.1) < 1e-4);
			W.SetRevoluteJointDrive(Id, {false, 100, 6});
			W.Step(H);
			PHYSICS_REQUIRE(Abs(W.GetRevoluteJoint(Id).AngularSpeed - 0.1) < 1e-4);
		}
	}
}
void RevoluteLimits3D()
{
	FSpace::World W;
	FSpace::Configure(W);
	const auto A = FSpace::Body(W, EBodyType::Static, {});
	const auto B = FSpace::Body(W, EBodyType::Dynamic, {});
	auto D = W.MakeRevoluteJointDescription(A, B, {});
	D.Drive = {true, 2, 10};
	D.Limits = {true, -0.4, 0.4};
	const auto Id = W.CreateRevoluteJoint(A, B, D);
	for (uint32 I = 0; I < 120; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(W.GetRevoluteJoint(Id).Angle <= 0.401 && W.GetRevoluteJoint(Id).Angle >= 0.39);
	W.SetRevoluteJointDrive(Id, {true, -2, 10});
	W.Step(H);
	PHYSICS_REQUIRE(W.GetRevoluteJoint(Id).AngularSpeed < -0.1);
	for (uint32 I = 0; I < 120; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(W.GetRevoluteJoint(Id).Angle >= -0.401 && W.GetRevoluteJoint(Id).Angle <= -0.39);
	W.SetRevoluteJointLimits(Id, {true, 0.1, 0.1});
	for (uint32 I = 0; I < 60; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(Abs(W.GetRevoluteJoint(Id).Angle - 0.1) < 0.001);
	PHYSICS_REQUIRE(W.GetRevoluteJoint(Id).LimitState == EJointLimitState::Locked);
	bool bRejected = false;
	try
	{
		W.SetRevoluteJointDrive(Id, {true, 1, -1});
	}
	catch (const FException&)
	{
		bRejected = true;
	}
	PHYSICS_REQUIRE(bRejected && W.IsJointAlive(Id) && W.GetRevoluteJoint(Id).Drive.TargetAngularSpeed == -2);
}
void PrismaticBudget3D()
{
	const uint32 IterationValues[] = {1, 8, 32};
	const uint32 SubStepValues[] = {1, 4};
	for (const uint32 Iterations : IterationValues)
	{
		for (const uint32 SubSteps : SubStepValues)
		{
			FSpace::World W;
			FSpace::Configure(W);
			FContactSettings3D C;
			C.VelocityIterations = Iterations;
			W.SetContactSettings(C);
			const auto A = FSpace::Body(W, EBodyType::Static, {});
			const auto B = FSpace::Body(W, EBodyType::Dynamic, {});
			auto D = W.MakePrismaticJointDescription(A, B, {});
			D.Drive = {true, 100, 6};
			const auto Id = W.CreatePrismaticJoint(A, B, D);
			W.Step(H, SubSteps);
			PHYSICS_REQUIRE(Abs(W.GetPrismaticJoint(Id).TranslationRate - 0.1) < 1e-4);
			W.SetPrismaticJointDrive(Id, {false, 100, 6});
			W.Step(H);
			PHYSICS_REQUIRE(Abs(W.GetPrismaticJoint(Id).TranslationRate - 0.1) < 1e-4);
		}
	}
}
void PrismaticLimits3D()
{
	FSpace::World W;
	FSpace::Configure(W);
	const auto A = FSpace::Body(W, EBodyType::Static, {});
	const auto B = FSpace::Body(W, EBodyType::Dynamic, {});
	auto D = W.MakePrismaticJointDescription(A, B, {});
	D.Drive = {true, 2, 10};
	D.Limits = {true, -0.4, 0.4};
	const auto Id = W.CreatePrismaticJoint(A, B, D);
	for (uint32 I = 0; I < 120; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(W.GetPrismaticJoint(Id).Translation <= 0.401 && W.GetPrismaticJoint(Id).Translation >= 0.39);
	W.SetPrismaticJointDrive(Id, {true, -2, 10});
	W.Step(H);
	PHYSICS_REQUIRE(W.GetPrismaticJoint(Id).TranslationRate < -0.1);
	for (uint32 I = 0; I < 120; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(W.GetPrismaticJoint(Id).Translation >= -0.401 && W.GetPrismaticJoint(Id).Translation <= -0.39);
	W.SetPrismaticJointLimits(Id, {true, 0.1, 0.1});
	for (uint32 I = 0; I < 60; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(Abs(W.GetPrismaticJoint(Id).Translation - 0.1) < 0.001);
	PHYSICS_REQUIRE(W.GetPrismaticJoint(Id).LimitState == EJointLimitState::Locked);
	bool bRejected = false;
	try
	{
		W.SetPrismaticJointDrive(Id, {true, 1, -1});
	}
	catch (const FException&)
	{
		bRejected = true;
	}
	PHYSICS_REQUIRE(bRejected && W.IsJointAlive(Id) && W.GetPrismaticJoint(Id).Drive.TargetSpeed == -2);
}
// 主軸90度と非主軸120度を独立した期待軸で照合する。
void FrameAxes()
{
	using namespace Dxf::PhysicsPrivate;
	const f64 S = Sqrt(0.5);
	const FMechanismRotation Rotations[] = {{S, 0, 0, S}, {0, S, 0, S}, {0, 0, S, S}, {0.5, 0.5, 0.5, 0.5}};
	const FMechanismVector Inputs[] = {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}, {1, 0, 0}};
	const FMechanismVector Expected[] = {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {0, 1, 0}};
	for (size_t I = 0; I < 4; ++I)
	{
		const auto V = Rotate(Rotations[I], Inputs[I]);
		PHYSICS_REQUIRE(Length(Subtract(V, Expected[I])) < 1e-12);
		const auto Q = Rotations[I];
		const auto Negative = Rotate({-Q.X, -Q.Y, -Q.Z, -Q.W}, Inputs[I]);
		PHYSICS_REQUIRE(Length(Subtract(Negative, Expected[I])) < 1e-12);
	}
}
// NaNが最大成分の比較で消えても、入力成分そのものを拒否する。
void InvalidFrame()
{
	FPhysicsWorld3D W;
	FBodyDescription3D Static;
	Static.Type = EBodyType::Static;
	const auto A = W.CreateBody(Static);
	const auto B = W.CreateBody({});
	auto D = W.MakeFixedJointDescription(A, B, {});
	D.FrameA.LocalRotation.X = TNumericLimits<f32>::QuietNaN();
	bool bRejected = false;
	try
	{
		(void)W.CreateFixedJoint(A, B, D);
	}
	catch (const FException&)
	{
		bRejected = true;
	}
	PHYSICS_REQUIRE(bRejected && W.IsAlive(A) && W.IsAlive(B));
	const auto Id = W.CreateFixedJoint(A, B, W.MakeFixedJointDescription(A, B, {}));
	PHYSICS_REQUIRE(Id.Index == 0);
}

// 誤差の微分を、微小回転で予測したPose差から独立に検算する。
void AngularDerivatives()
{
	using namespace Dxf::PhysicsPrivate;
	FMechanismBody A;
	FMechanismBody B;
	A.Rotation = Normalize({0.1, -0.2, 0.3, 0.9});
	B.Rotation = Normalize({-0.2, 0.1, 0.4, 0.85});
	A.Angular = {0.2, -0.3, 0.4};
	B.Angular = {0.4, 0.7, -0.2};
	FMechanismSettings S;
	const auto Fixed = BuildMechanismRows(EJointKind::Fixed, false, S, A, B, H);
	S.bMotor = true;
	S.Maximum = 10;
	const auto Hinge = BuildMechanismRows(EJointKind::Revolute, false, S, A, B, H);
	const f64 E = 1e-7;
	// 正確な指数回転をワールド左側から掛ける。製品の位置補正を使わない。
	const auto Predict = [](FMechanismBody Body, f64 Seconds)
	{
		const f64 W = Length(Body.Angular);
		const f64 ScaleValue = Sin(W * Seconds * 0.5) / W;
		const auto V = Body.Angular;
		Body.Rotation = Multiply({V.X * ScaleValue, V.Y * ScaleValue, V.Z * ScaleValue, Cos(W * Seconds * 0.5)}, Body.Rotation);
		return Body;
	};
	const auto NextA = Predict(A, E);
	const auto NextB = Predict(B, E);
	const auto NextFixed = BuildMechanismRows(EJointKind::Fixed, false, S, NextA, NextB, H);
	const auto NextHinge = BuildMechanismRows(EJointKind::Revolute, false, S, NextA, NextB, H);
	for (size_t I = 3; I < 6; ++I)
	{
		PHYSICS_REQUIRE(Abs((NextFixed.Values[I].Error - Fixed.Values[I].Error) / E - Rate(Fixed.Values[I], A, B)) < 1e-6);
	}
	for (size_t I = 3; I < 5; ++I)
	{
		PHYSICS_REQUIRE(Abs((NextHinge.Values[I].Error - Hinge.Values[I].Error) / E - Rate(Hinge.Values[I], A, B)) < 1e-6);
	}
	PHYSICS_REQUIRE(Abs((NextHinge.Coordinate - Hinge.Coordinate) / E - Hinge.Rate) < 1e-6);
}
// 非主軸120度のFrameはZ軸をWorld Xへ向ける。twistとswingを区別する。
void NonPrincipalHinge()
{
	FPhysicsWorld3D W;
	FSpace::Configure(W);
	const auto A = FSpace::Body(W, EBodyType::Static, {}, true);
	const auto B = FSpace::Body(W, EBodyType::Dynamic, {}, true);
	const FQuaternion Frame{0.5f, 0.5f, 0.5f, 0.5f};
	auto D = W.MakeRevoluteJointDescription(A, B, {}, Frame);
	const auto Id = W.CreateRevoluteJoint(A, B, D);
	W.ApplyAngularImpulse(B, {0.3f, 0.4f, 0.2f});
	for (uint32 I = 0; I < 600; ++I)
	{
		W.Step(H);
	}
	const auto State = W.GetRevoluteJoint(Id);
	PHYSICS_REQUIRE(State.AnchorError < 0.001 && State.AxisAlignmentError < 0.001);
	PHYSICS_REQUIRE(Abs(State.Angle) > 0.02 && State.AngularSpeed > 0.2);
	// q/-qの同じFrameで再構築しても観察値は変わらない。
	const auto Before = State.Angle;
	const auto Q = D.FrameB.LocalRotation;
	D.FrameB.LocalRotation = {-Q.X, -Q.Y, -Q.Z, -Q.W};
	const auto Second = W.CreateRevoluteJoint(A, B, D);
	PHYSICS_REQUIRE(Abs(W.GetRevoluteJoint(Second).Angle - Before) < 1e-6);
}
// 静止・速度0ブレーキでSleepでき、同値要求で起こさず非ゼロ指示で起こす。
template <typename T, EJointKind Kind>
void SleepDrive()
{
	typename T::World W;
	W.SetGravity({});
	const auto A = T::Body(W, EBodyType::Static, {});
	const auto B = T::Body(W, EBodyType::Dynamic, {});
	const auto C = T::Body(W, EBodyType::Dynamic, T::At(5, 0));
	const auto Id = T::template Join<Kind>(W, A, B);
	const auto Other = T::template Join<EJointKind::Fixed>(W, A, C);
	for (uint32 I = 0; I < 120; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(W.IsSleeping(B) && W.IsSleeping(C));
	if constexpr (Kind == EJointKind::Fixed)
	{
		return;
	}
	else if constexpr (Kind == EJointKind::Revolute)
	{
		W.SetRevoluteJointDrive(Id, {true, 0, 10});
	}
	else
	{
		W.SetPrismaticJointDrive(Id, {true, 0, 10});
	}
	for (uint32 I = 0; I < 120; ++I)
	{
		if constexpr (Kind == EJointKind::Revolute)
		{
			W.SetRevoluteJointDrive(Id, {true, 0, 10});
		}
		else
		{
			W.SetPrismaticJointDrive(Id, {true, 0, 10});
		}
		W.Step(H);
	}
	PHYSICS_REQUIRE(W.IsSleeping(B) && W.IsSleeping(C) && W.IsJointAlive(Other));
	if constexpr (Kind == EJointKind::Revolute)
	{
		W.SetRevoluteJointDrive(Id, {true, 1, 10});
	}
	else
	{
		W.SetPrismaticJointDrive(Id, {true, 1, 10});
	}
	W.Step(H);
	PHYSICS_REQUIRE(!W.IsSleeping(B) && W.IsSleeping(C));
	PHYSICS_REQUIRE(T::template Observe<Kind>(W, Id).Rate > 0.01);
}
// 共通slotをDistance→Fixed→Revoluteへ再利用し、古い駆動値や世代を持ち越さない。
template <typename T>
void MixedSlot()
{
	typename T::World W;
	T::Configure(W);
	const auto A = T::Body(W, EBodyType::Static, {});
	const auto B = T::Body(W, EBodyType::Dynamic, T::At(1, 0));
	typename T::FJointDescription D;
	D.Length = 1;
	const auto First = W.CreateDistanceJoint(A, B, D);
	W.ApplyLinearImpulse(B, T::At(1, 0));
	W.Step(H);
	PHYSICS_REQUIRE(W.DestroyJoint(First));
	const auto Second = T::template Join<EJointKind::Fixed>(W, A, B);
	PHYSICS_REQUIRE(First.Index == Second.Index && First.Generation != Second.Generation);
	W.Step(H);
	PHYSICS_REQUIRE(W.DestroyJoint(Second));
	const auto Third = T::template Join<EJointKind::Revolute>(W, A, B);
	PHYSICS_REQUIRE(Second.Index == Third.Index && Second.Generation != Third.Generation);
	const auto Collider = W.AttachCollider(B, T::Ball({}, 0.25f));
	PHYSICS_REQUIRE(W.DetachCollider(Collider) && W.IsJointAlive(Third));
	W.Step(H);
	PHYSICS_REQUIRE(Abs(W.GetRevoluteJoint(Third).AngularSpeed) < 0.001);
	bool bWrong = false;
	try
	{
		W.SetPrismaticJointDrive(Third, {true, 1, 10});
	}
	catch (const FException&)
	{
		bWrong = true;
	}
	PHYSICS_REQUIRE(bWrong && W.IsJointAlive(Third));
	PHYSICS_REQUIRE(W.DestroyBody(B) && !W.IsJointAlive(Third));
}
// 混合した4種類とContactを同じ登録列で構築し、実Workerとの成分比較に使う。
template <typename T>
TVector<f64> MixedRun(FJobSystem* Jobs, bool bChain, bool bKinematic)
{
	typename T::World W;
	T::Configure(W);
	W.SetGravity(T::At(0, -1));
	FPhysicsExecutionSettings Execution;
	Execution.JobSystem = Jobs;
	W.SetExecutionSettings(Execution);
	const auto A = T::Body(W, bKinematic ? EBodyType::Kinematic : EBodyType::Static, {});
	if (bKinematic)
	{
		W.SetVelocity(A, T::At(0.1f, 0));
	}
	TArray<typename T::BodyId, 16> Bodies;
	TArray<typename T::JointId, 16> Joints;
	for (size_t I = 0; I < 16; ++I)
	{
		Bodies[I] = T::Body(W, EBodyType::Dynamic, T::At(static_cast<f32>(I * 4), 1));
		const auto Endpoint = bChain && I > 0 ? Bodies[I - 1] : A;
		switch (I % 4)
		{
		case 0:
		{
			typename T::FJointDescription D;
			D.LocalAnchorA = bChain ? T::At(0, 0) : T::At(static_cast<f32>(I * 4), 0);
			const auto Delta = W.GetPosition(Bodies[I]) - W.GetPosition(Endpoint) - D.LocalAnchorA;
			D.Length = Sqrt(f64(Delta.X) * Delta.X + f64(Delta.Y) * Delta.Y);
			Joints[I] = W.CreateDistanceJoint(Endpoint, Bodies[I], D);
			break;
		}
		case 1:
			Joints[I] = W.CreateRevoluteJoint(Endpoint, Bodies[I], W.MakeRevoluteJointDescription(Endpoint, Bodies[I], W.GetPosition(Bodies[I])));
			W.SetRevoluteJointDrive(Joints[I], {true, 0.3, 2});
			break;
		case 2:
			Joints[I] = W.CreateFixedJoint(Endpoint, Bodies[I], W.MakeFixedJointDescription(Endpoint, Bodies[I], W.GetPosition(Bodies[I])));
			break;
		case 3:
			Joints[I] = W.CreatePrismaticJoint(Endpoint, Bodies[I], W.MakePrismaticJointDescription(Endpoint, Bodies[I], W.GetPosition(Bodies[I])));
			W.SetPrismaticJointDrive(Joints[I], {true, 0.3, 2});
			break;
		}
		W.AttachCollider(Bodies[I], T::Ball({}, 0.5f));
		W.AttachCollider(A, T::Ball(T::At(static_cast<f32>(I * 4), 0.01f), 0.5f));
	}
	TVector<f64> Values;
	for (uint32 Frame = 0; Frame < 40; ++Frame)
	{
		W.Step(H);
		for (size_t I = 0; I < 16; ++I)
		{
			PhysicsTest::JointTest::AppendBody<T>(W, Bodies[I], Values);
			FObserved State;
			switch (I % 4)
			{
			case 0:
				State.Anchor = W.GetDistanceJoint(Joints[I]).Error;
				break;
			case 1:
				State = T::template Observe<EJointKind::Revolute>(W, Joints[I]);
				break;
			case 2:
				State = T::template Observe<EJointKind::Fixed>(W, Joints[I]);
				break;
			case 3:
				State = T::template Observe<EJointKind::Prismatic>(W, Joints[I]);
				break;
			}
			PHYSICS_REQUIRE(IsFinite(State.Anchor) && IsFinite(State.Angular));
			Values.PushBack(State.Anchor);
			Values.PushBack(State.Angular);
			Values.PushBack(State.Coordinate);
			Values.PushBack(State.Rate);
			Values.PushBack(W.IsJointAlive(Joints[I]) ? 1 : 0);
		}
		PhysicsTest::JointTest::AppendBody<T>(W, A, Values);
		const auto Diagnostics = W.GetExecutionDiagnostics();
		PHYSICS_REQUIRE(Diagnostics.IslandCount == (bChain ? 1 : 16));
		PHYSICS_REQUIRE(Diagnostics.SolverIslandCount == (Jobs ? Diagnostics.IslandCount : 0));
		if (Frame == 0)
		{
			PHYSICS_REQUIRE(Diagnostics.ManifoldCount > 0);
		}
	}
	return Values;
}
template <typename T, bool bChain, bool bKinematic>
void MixedLanes()
{
	const auto Reference = MixedRun<T>(nullptr, bChain, bKinematic);
	const uint32 LaneValues[] = {1, 2, 4, 8};
	for (uint32 Lanes : LaneValues)
	{
		FJobSystem Jobs(Lanes);
		PhysicsTest::JointTest::RequireBits(Reference, MixedRun<T>(&Jobs, bChain, bKinematic));
	}
}

template <typename T>
void Wall_Internal(typename T::FWorld& World, f32 X, f32 Y)
{
	// 共有接続先を動かさないための生成条件。
	typename T::FBodyDescription Static;
	Static.Type = EBodyType::Static;
	Static.Position = T::At(X, Y);
	// この試験で観測するBody ID。
	const auto Body = World.CreateBody(Static);
	// 壁または球の反発と形状。
	typename T::FColliderDescription Collider;
	Collider.Restitution = 1;
	if constexpr (T::b3D)
	{
		Collider.Shape = FOBB{{}, {0.05f, 5, 5}};
	}
	else
	{
		Collider.Shape = FOrientedBox2D{{}, {0.05f, 5}};
	}
	World.AttachCollider(Body, Collider);
}
template <typename T, EJointKind Kind>
TVector<f64> IsolationRun_Internal(int32 Mode, uint32& Hits)
{
	// 試験ごとに独立して構築するWorld。
	typename T::FWorld World;
	World.SetGravity({});
	// 離れた衝突の回数を比較するCCD設定。
	typename T::FContinuous Continuous;
	Continuous.bEnabled = true;
	Continuous.MaxIterations = 16;
	World.SetContinuousSettings(Continuous);
	// 再利用値の影響を確認する速度求解の反復設定。
	typename T::FContact Contact;
	Contact.VelocityIterations = 1;
	World.SetContactSettings(Contact);
	// 共有接続先を動かさないための生成条件。
	typename T::FBodyDescription Static;
	Static.Type = EBodyType::Static;
	// 拘束の反対側の接続先ID。
	const auto Anchor = World.CreateBody(Static);
	// 観測するBodyの生成条件。
	typename T::FBodyDescription Body;
	Body.Position = T::At(2, 1);
	Body.Velocity = T::At(0.37f, -0.71f);
	Body.bAllowSleep = false;
	// 離れたCCD島から独立したJointの動くBody。
	const auto Weight = World.CreateBody(Body);
	const auto Id = T::template Join<Kind>(World, Anchor, Weight);
	if constexpr (Kind == EJointKind::Revolute)
	{
		World.SetRevoluteJointDrive(Id, {true, 100, 0.5});
	}
	if constexpr (Kind == EJointKind::Prismatic)
	{
		World.SetPrismaticJointDrive(Id, {true, 100, 0.5});
	}
	// Mode0も同じCCD移動区間を通す。離れた球に当たる壁の有無と幅だけが異なる。
	Body.Position = T::At(-2, 100);
	Body.Velocity = T::At(100, 0);
	Body.bUseContinuous = true;
	// Joint島から離したCCD対象Body。
	const auto Fast = World.CreateBody(Body);
	// 壁または球の反発と形状。
	auto Collider = T::Ball({}, 0.25f);
	Collider.Restitution = 1;
	World.AttachCollider(Fast, Collider);
	if (Mode > 0)
	{
		Wall_Internal<T>(World, 0, 100);
	}
	if (Mode == 2)
	{
		Wall_Internal<T>(World, -4, 100);
	}
	// 時系列のBody・Joint観測値。
	TVector<f64> Values;
	World.Step(0.1);
	Hits = World.GetContinuousDiagnostics().HitsResolved;
	PHYSICS_REQUIRE(World.GetContinuousDiagnostics().UnprocessedSeconds == 0);
	PhysicsTest::JointTest::AppendBody<T>(World, Weight, Values);
	const auto First = T::template Observe<Kind>(World, Id);
	Values.PushBack(First.Anchor);
	Values.PushBack(First.Angular);
	Values.PushBack(First.Coordinate);
	Values.PushBack(First.Rate);
	// 次のStepにも余分なTOI求解の再利用値が漏れない。
	World.SetContinuousSettings({});
	World.Step(1.0 / 60.0);
	PhysicsTest::JointTest::AppendBody<T>(World, Weight, Values);
	const auto Next = T::template Observe<Kind>(World, Id);
	Values.PushBack(Next.Anchor);
	Values.PushBack(Next.Angular);
	Values.PushBack(Next.Coordinate);
	Values.PushBack(Next.Rate);
	return Values;
}
// 離れたTOIの0/1/3回を、Joint・Motorと次Stepの状態を変えずに比較する。
template <typename T, EJointKind Kind>
void MechanismCcdIsolation()
{
	uint32 NoHits = 0;
	uint32 OneHit = 0;
	uint32 ManyHits = 0;
	const auto Reference = IsolationRun_Internal<T, Kind>(0, NoHits);
	const auto One = IsolationRun_Internal<T, Kind>(1, OneHit);
	const auto Many = IsolationRun_Internal<T, Kind>(2, ManyHits);
	Toolbox::Out << "K C01 " << T::Name << " kind=" << static_cast<int32>(Kind) << " CCD hits=" << NoHits << "/" << OneHit << "/" << ManyHits << "\n";
	PHYSICS_REQUIRE(NoHits == 0 && OneHit == 1 && ManyHits == 3);
	PhysicsTest::JointTest::RequireBits(Reference, One);
	PhysicsTest::JointTest::RequireBits(Reference, Many);
}
// 接続後に大きい姿勢誤差を与える。毎Step現在姿勢へ基準を更新してはいけない。
template <typename T>
void FixedHalfTurn()
{
	typename T::World W;
	T::Configure(W);
	const auto A = T::Body(W, EBodyType::Static, {});
	const auto B = T::Body(W, EBodyType::Dynamic, {});
	const auto Id = T::template Join<EJointKind::Fixed>(W, A, B);
	if constexpr (T::b3D)
	{
		const auto Q = FQuaternion::FromAxisAngle({1, 2, 3}, 3.09f);
		W.SetBodyTransform(B, {}, Q);
		const auto Positive = W.GetFixedJoint(Id).OrientationError;
		W.SetBodyTransform(B, {}, {-Q.X, -Q.Y, -Q.Z, -Q.W});
		PHYSICS_REQUIRE(Abs(W.GetFixedJoint(Id).OrientationError - Positive) < 1e-6);
	}
	else
	{
		W.SetBodyTransform(B, {}, 3.09f);
	}
	PHYSICS_REQUIRE(W.GetFixedJoint(Id).OrientationError > 3.08);
	for (uint32 I = 0; I < 60; ++I)
	{
		W.Step(H);
	}
	PHYSICS_REQUIRE(W.GetFixedJoint(Id).OrientationError < 0.001);
	// getterだけを直した実装を、Body姿勢の独立した方向で検出する。
	if constexpr (T::b3D)
	{
		const auto Direction = W.GetOrientation(B).Rotate({1, 0, 0});
		PHYSICS_REQUIRE(Abs(Direction.X - 1) < 0.001 && Abs(Direction.Y) < 0.001 && Abs(Direction.Z) < 0.001);
		PHYSICS_REQUIRE(W.GetAngularVelocity(B) == FVector3{});
	}
	else
	{
		PHYSICS_REQUIRE(Abs(W.GetAngle(B)) < 0.001 && W.GetAngularVelocity(B) == 0);
	}
}
// 動く・回るKinematic支点に対する横ずれと姿勢保持を実Worldで確認する。
template <typename T>
void MovingPrismatic()
{
	typename T::World W;
	T::Configure(W);
	const auto A = T::Body(W, EBodyType::Kinematic, {}, true);
	const auto B = T::Body(W, EBodyType::Dynamic, T::At(2, 0), true);
	auto D = W.MakePrismaticJointDescription(A, B, {});
	D.Limits = {true, 1, 3};
	D.Drive = {true, 0, 1000};
	// Make helperの両Anchor一致を、A重心を0・B重心を現在の座標として使う基準へ変える。
	D.FrameB.LocalAnchor = {};
	const auto Id = W.CreatePrismaticJoint(A, B, D);
	PHYSICS_REQUIRE(Abs(W.GetPrismaticJoint(Id).Translation - 2) < 1e-6);
	W.SetVelocity(A, T::At(0.1f, 0.2f));
	if constexpr (T::b3D)
	{
		W.SetAngularVelocity(A, {0.1f, 0.15f, 0.2f});
	}
	else
	{
		W.SetAngularVelocity(A, 0.2f);
	}
	// 速度0のMotorは位置ロックではない。半陰的Eulerの自由座標変化を独立に計算する。
	const f64 Omega = T::b3D ? Sqrt(f64(0.1f) * 0.1f + f64(0.15f) * 0.15f + f64(0.2f) * 0.2f) : f64(0.2f);
	const f64 ParallelFraction = T::b3D ? f64(0.1f) * 0.1f / (Omega * Omega) : 0;
	const f64 Theta = T::b3D ? 2 * Atan2(0.5 * Omega * H, 1.0) : Omega * H;
	const f64 Ratio = ParallelFraction + (1 - ParallelFraction) * (Cos(Theta) + H * Omega * Sin(Theta));
	f64 ExpectedTranslation = 2;
	f64 ExpectedRate = 0;
	for (uint32 I = 0; I < 600; ++I)
	{
		ExpectedRate = ExpectedTranslation * (1 - ParallelFraction) * Omega * Sin(Theta);
		ExpectedTranslation *= Ratio;
		W.Step(H);
		const auto State = W.GetPrismaticJoint(Id);
		PHYSICS_REQUIRE(IsFinite(State.TranslationRate) && State.AnchorError < 0.001 && State.OrientationError < 0.001);
		if (I == 0)
		{
			PHYSICS_REQUIRE(Abs(State.Translation - ExpectedTranslation) < 1e-6 && Abs(State.TranslationRate - ExpectedRate) < 1e-6);
		}
	}
	const auto State = W.GetPrismaticJoint(Id);
	Toolbox::Out << "P01 " << T::Name << " final translation-um=" << static_cast<int64>(State.Translation * 1e6) << " rate-ums=" << static_cast<int64>(State.TranslationRate * 1e6) << " transverse-um=" << static_cast<int64>(State.AnchorError * 1e6) << " angle-urad=" << static_cast<int64>(State.OrientationError * 1e6) << "\n";
	Toolbox::Out << "P01 " << T::Name << " independent expected translation-um=" << static_cast<int64>(ExpectedTranslation * 1e6) << " rate-ums=" << static_cast<int64>(ExpectedRate * 1e6) << "\n";
	PHYSICS_REQUIRE(Abs(State.Translation - ExpectedTranslation) < 1e-4 && Abs(State.TranslationRate - ExpectedRate) < 1e-5);
}
// Localベクトルの回転を非主軸120度のRodrigues式で独立に比較する。
void ExplicitNonprincipalRotation()
{
	using namespace Dxf::PhysicsPrivate;
	const f64 N = Sqrt(14.0);
	const f64 S = Sqrt(0.75);
	const FMechanismRotation Q{S / N, 2 * S / N, 3 * S / N, 0.5};
	const auto Rotated = Rotate(Q, {1, 0, 0});
	const FMechanismVector Expected{-0.5 + 1.5 / 14, 3.0 / 14 + S * 3 / N, 4.5 / 14 - S * 2 / N};
	PHYSICS_REQUIRE(Length(Subtract(Rotated, Expected)) < 1e-12);
}
// 反平行の軸を誤差0とせず登録前に拒否する。非主軸Frameで確認する。
void AntiparallelInput()
{
	FPhysicsWorld3D W;
	FSpace::Configure(W);
	const auto A = FSpace::Body(W, EBodyType::Static, {});
	const auto B = FSpace::Body(W, EBodyType::Dynamic, {});
	FRevoluteJointDescription3D D;
	D.FrameA.LocalRotation = {0.5f, 0.5f, 0.5f, 0.5f};
	D.FrameB.LocalRotation = D.FrameA.LocalRotation * FQuaternion{1, 0, 0, 0};
	bool bRejected = false;
	try
	{
		(void)W.CreateRevoluteJoint(A, B, D);
	}
	catch (const FException&)
	{
		bRejected = true;
	}
	PHYSICS_REQUIRE(bRejected);
	D.FrameB.LocalRotation = {0, 0, 0, 0};
	bRejected = false;
	try
	{
		(void)W.CreateRevoluteJoint(A, B, D);
	}
	catch (const FException&)
	{
		bRejected = true;
	}
	PHYSICS_REQUIRE(bRejected);
	const auto Id = W.CreateRevoluteJoint(A, B, W.MakeRevoluteJointDescription(A, B, {}));
	PHYSICS_REQUIRE(Id.Index == 0 && W.IsAlive(A) && W.IsAlive(B));
}
// 有限のワールド座標でもLocalへの差が公開float精度を超えたらhelperは拒否する。
template <typename T>
void HelperOverflow()
{
	typename T::World World;
	const f32 Maximum = TNumericLimits<f32>::Max();
	const auto A = T::Body(World, EBodyType::Static, T::At(-Maximum, 0));
	const auto B = T::Body(World, EBodyType::Dynamic, {});
	bool Rejected = false;
	try
	{
		(void)World.MakeFixedJointDescription(A, B, T::At(Maximum, 0));
	}
	catch (const FException&)
	{
		Rejected = true;
	}
	PHYSICS_REQUIRE(Rejected);
	PHYSICS_REQUIRE(World.IsAlive(A) && World.IsAlive(B));
}
// 非ゼロ履歴を持つFixedのslotをRevoluteへ再利用し、新規Worldと一反復を比べる。
template <typename T>
void MixedCache()
{
	typename T::World Reused;
	typename T::World Fresh;
	T::Configure(Reused);
	T::Configure(Fresh);
	typename T::FContact Contact;
	Contact.VelocityIterations = 1;
	Reused.SetContactSettings(Contact);
	Fresh.SetContactSettings(Contact);
	const auto A = T::Body(Reused, EBodyType::Static, {});
	const auto B = T::Body(Reused, EBodyType::Dynamic, T::At(1, 0));
	const auto C = T::Body(Fresh, EBodyType::Static, {});
	const auto D = T::Body(Fresh, EBodyType::Dynamic, T::At(1, 0));
	const auto Old = Reused.CreateFixedJoint(A, B, Reused.MakeFixedJointDescription(A, B, T::At(0.3f, 0.4f)));
	Reused.ApplyLinearImpulse(B, T::At(0.7f, -0.4f));
	T::AngularImpulse(Reused, B, 0.6f);
	Reused.Step(H);
	PHYSICS_REQUIRE(Reused.DestroyJoint(Old));
	T::Reset(Reused, B, T::At(1, 0));
	Reused.SetVelocity(B, T::At(0.3f, -0.4f));
	Fresh.SetVelocity(D, T::At(0.3f, -0.4f));
	T::AngularImpulse(Reused, B, 0.7f);
	T::AngularImpulse(Fresh, D, 0.7f);
	const auto R = Reused.CreateRevoluteJoint(A, B, Reused.MakeRevoluteJointDescription(A, B, T::At(0.3f, 0.4f)));
	const auto F = Fresh.CreateRevoluteJoint(C, D, Fresh.MakeRevoluteJointDescription(C, D, T::At(0.3f, 0.4f)));
	PHYSICS_REQUIRE(R.Index == Old.Index && R.Generation != Old.Generation);
	Reused.Step(H);
	Fresh.Step(H);
	PHYSICS_REQUIRE(Reused.GetPosition(B) == Fresh.GetPosition(D));
	PHYSICS_REQUIRE(Reused.GetVelocity(B) == Fresh.GetVelocity(D));
	PHYSICS_REQUIRE(Reused.GetAngularVelocity(B) == Fresh.GetAngularVelocity(D));
	PHYSICS_REQUIRE(Reused.GetRevoluteJoint(R).Angle == Fresh.GetRevoluteJoint(F).Angle);
}
// 質量2kg/0.5kg、腕2m/-3m、逆慣性0.5/2。独立式K=22.5。
void PlaneUnitImpulse()
{
	using namespace Dxf::PhysicsPrivate;
	FMechanismBody A;
	FMechanismBody B;
	A.InverseMass = 0.5;
	B.InverseMass = 2;
	A.InverseInertia = {0, 0, 0.5};
	B.InverseInertia = {0, 0, 2};
	const auto Row = LinearRow({1, 0, 0}, {0, 2, 0}, {0, -3, 0}, {}, false);
	PHYSICS_REQUIRE(Abs(EffectiveMass(Row, A, B) - 22.5) < 1e-10);
	ApplyRow(Row, A, B, 1, false);
	PHYSICS_REQUIRE(Abs(B.Velocity.X - A.Velocity.X + 2 * A.Angular.Z + 3 * B.Angular.Z - 22.5) < 1e-10);
	const auto Swapped = LinearRow({-1, 0, 0}, {0, -3, 0}, {0, 2, 0}, {}, false);
	PHYSICS_REQUIRE(Abs(EffectiveMass(Swapped, B, A) - 22.5) < 1e-10);
}
// 重力0、半径0.2mの荷物と壁。速度1m/s、最大力10N、600Stepでも壁を越えない。
template <typename T>
void MotorContact()
{
	typename T::World World;
	T::Configure(World);
	const auto A = T::Body(World, EBodyType::Static, {});
	const auto B = T::Body(World, EBodyType::Dynamic, {});
	const auto Wall = T::Body(World, EBodyType::Static, T::At(0.8f, 0));
	typename T::FColliderDescription Collider;
	if constexpr (T::b3D)
	{
		Collider.Shape = FSphere{{}, 0.2f};
	}
	else
	{
		Collider.Shape = FCircle2D{{}, 0.2f};
	}
	World.AttachCollider(B, Collider);
	World.AttachCollider(Wall, Collider);
	auto Description = World.MakePrismaticJointDescription(A, B, {});
	Description.Drive = {true, 1, 10};
	const auto Joint = World.CreatePrismaticJoint(A, B, Description);
	for (int32 Step = 0; Step < 600; ++Step)
	{
		World.Step(H);
	}
	const auto State = World.GetPrismaticJoint(Joint);
	PHYSICS_REQUIRE(State.Translation > 0.38 && State.Translation < 0.43);
	PHYSICS_REQUIRE(Abs(State.TranslationRate) < 0.03);
	PHYSICS_REQUIRE(State.AnchorError < 0.001);
	PHYSICS_REQUIRE(State.Drive.TargetSpeed == 1);
	PHYSICS_REQUIRE(World.GetExecutionDiagnostics().ManifoldCount > 0);
}

// 重力0、COM同士接続、速度2rad/s・最大100Nm。600Stepの主値折返しで逆転しない。
template <typename T>
void MultipleMotorTurns()
{
	// 自由な一軸だけを持つ比較用World。
	typename T::World World;
	T::Configure(World);
	// COMが一致した支点と回転体。
	const auto A = T::Body(World, EBodyType::Static, {});
	const auto B = T::Body(World, EBodyType::Dynamic, {});
	// Limitを使わず、複数回転を有限Motorで駆動する。
	auto Description = World.MakeRevoluteJointDescription(A, B, {});
	Description.Drive = {true, 2, 100};
	const auto Joint = World.CreateRevoluteJoint(A, B, Description);
	// 前Stepの主値と折返し回数。通算角度は公開状態へ追加しない。
	f64 Previous = 0;
	int32 Wraps = 0;
	for (int32 Step = 0; Step < 600; ++Step)
	{
		World.Step(H);
		// Poseから得た角度と速度を独立に判定する。
		const auto State = World.GetRevoluteJoint(Joint);
		PHYSICS_REQUIRE(State.Angle > -3.14159265358979323846 && State.Angle <= 3.14159265358979323846);
		if (Previous > 3 && State.Angle < -3)
		{
			++Wraps;
		}
		if (Step > 2)
		{
			PHYSICS_REQUIRE(State.AngularSpeed > 1.999 && State.AngularSpeed < 2.001);
		}
		PHYSICS_REQUIRE(State.AnchorError < 0.001 && State.AxisAlignmentError < 0.001);
		Previous = State.Angle;
	}
	PHYSICS_REQUIRE(Wraps >= 3);
}
const PhysicsTest::FCase Cases[] = {
    {"M01 2D multi turn motor keeps positive speed", MultipleMotorTurns<FPlane>},
    {"M01 3D multi turn motor keeps positive speed", MultipleMotorTurns<FSpace>},
    {"G02 2D unit impulse and A B exchange K", PlaneUnitImpulse},
    {"M02 2D finite motor stalls at contact", MotorContact<FPlane>},
    {"M02 3D finite motor stalls at contact", MotorContact<FSpace>},
    {"G01 2D helper rejects local precision overflow", HelperOverflow<FPlane>},
    {"G01 3D helper rejects local precision overflow", HelperOverflow<FSpace>},
    {"O01 2D reused kind rejects stale nonzero cache", MixedCache<FPlane>},
    {"O01 3D reused kind rejects stale nonzero cache", MixedCache<FSpace>},

    {"G01 nonprincipal Rodrigues independent vector rotation", ExplicitNonprincipalRotation},
    {"R01 nonprincipal antiparallel and zero frame rejected", AntiparallelInput},
    {"F01 2D fixed half turn finite sign and independent pose", FixedHalfTurn<FPlane>},
    {"P01 2D moving rotating kinematic rail", MovingPrismatic<FPlane>},
    {"F01 3D fixed half turn finite sign and independent pose", FixedHalfTurn<FSpace>},
    {"P01 3D moving rotating kinematic rail", MovingPrismatic<FSpace>},

    {"C01 2D Revolute unrelated TOI 0 1 3", MechanismCcdIsolation<FPlane, EJointKind::Revolute>},
    {"C01 2D Fixed unrelated TOI 0 1 3", MechanismCcdIsolation<FPlane, EJointKind::Fixed>},
    {"C01 2D Prismatic unrelated TOI 0 1 3", MechanismCcdIsolation<FPlane, EJointKind::Prismatic>},
    {"C01 3D Revolute unrelated TOI 0 1 3", MechanismCcdIsolation<FSpace, EJointKind::Revolute>},
    {"C01 3D Fixed unrelated TOI 0 1 3", MechanismCcdIsolation<FSpace, EJointKind::Fixed>},
    {"C01 3D Prismatic unrelated TOI 0 1 3", MechanismCcdIsolation<FSpace, EJointKind::Prismatic>},

    {"G02 exact angular constraint finite differences", AngularDerivatives},
    {"R01 3D nonprincipal hinge swing and twist", NonPrincipalHinge},
    {"O01 2D mixed kind slot and collider detach", MixedSlot<FPlane>},
    {"S01 2D Revolute sleep wake same drive", SleepDrive<FPlane, EJointKind::Revolute>},
    {"S01 2D Fixed sleep wake same drive", SleepDrive<FPlane, EJointKind::Fixed>},
    {"S01 2D Prismatic sleep wake same drive", SleepDrive<FPlane, EJointKind::Prismatic>},
    {"D01 2D mixed 4 kinds contact static lanes", MixedLanes<FPlane, false, false>},
    {"D01 2D mixed 4 kinds contact kinematic lanes", MixedLanes<FPlane, false, true>},
    {"D01 2D mixed 4 kinds contact chain lanes", MixedLanes<FPlane, true, false>},
    {"O01 3D mixed kind slot and collider detach", MixedSlot<FSpace>},
    {"S01 3D Revolute sleep wake same drive", SleepDrive<FSpace, EJointKind::Revolute>},
    {"S01 3D Fixed sleep wake same drive", SleepDrive<FSpace, EJointKind::Fixed>},
    {"S01 3D Prismatic sleep wake same drive", SleepDrive<FSpace, EJointKind::Prismatic>},
    {"D01 3D mixed 4 kinds contact static lanes", MixedLanes<FSpace, false, false>},
    {"D01 3D mixed 4 kinds contact kinematic lanes", MixedLanes<FSpace, false, true>},
    {"D01 3D mixed 4 kinds contact chain lanes", MixedLanes<FSpace, true, false>},

    {"G01 explicit axis rotations and quaternion signs", FrameAxes},
    {"G01 invalid frame rejected before registration", InvalidFrame},
    {"G02 independent unit impulse K", UnitImpulse},
    {"G02 independent rotating axis finite difference", MovingAxisDerivative},
    {"K 2D Revolute 600 steps freedom and error", Basic<FPlane, EJointKind::Revolute>},
    {"O01 2D Revolute lifetime and kind", Lifetime<FPlane, EJointKind::Revolute>},
    {"K 2D Fixed 600 steps freedom and error", Basic<FPlane, EJointKind::Fixed>},
    {"O01 2D Fixed lifetime and kind", Lifetime<FPlane, EJointKind::Fixed>},
    {"K 2D Prismatic 600 steps freedom and error", Basic<FPlane, EJointKind::Prismatic>},
    {"O01 2D Prismatic lifetime and kind", Lifetime<FPlane, EJointKind::Prismatic>},
    {"F01 2D dynamic fixed impulse and release", DynamicFixed<FPlane>},
    {"M01 2D Revolute dt effort iterations disabled", RevoluteBudget2D},
    {"L01 2D Revolute limits reverse equal invalid", RevoluteLimits2D},
    {"M01 2D Prismatic dt effort iterations disabled", PrismaticBudget2D},
    {"L01 2D Prismatic limits reverse equal invalid", PrismaticLimits2D},
    {"K 3D Revolute 600 steps freedom and error", Basic<FSpace, EJointKind::Revolute>},
    {"O01 3D Revolute lifetime and kind", Lifetime<FSpace, EJointKind::Revolute>},
    {"K 3D Fixed 600 steps freedom and error", Basic<FSpace, EJointKind::Fixed>},
    {"O01 3D Fixed lifetime and kind", Lifetime<FSpace, EJointKind::Fixed>},
    {"K 3D Prismatic 600 steps freedom and error", Basic<FSpace, EJointKind::Prismatic>},
    {"O01 3D Prismatic lifetime and kind", Lifetime<FSpace, EJointKind::Prismatic>},
    {"F01 3D dynamic fixed impulse and release", DynamicFixed<FSpace>},
    {"M01 3D Revolute dt effort iterations disabled", RevoluteBudget3D},
    {"L01 3D Revolute limits reverse equal invalid", RevoluteLimits3D},
    {"M01 3D Prismatic dt effort iterations disabled", PrismaticBudget3D},
    {"L01 3D Prismatic limits reverse equal invalid", PrismaticLimits3D},
};
} // namespace
namespace PhysicsTest
{
const FCase* GetMechanismJointCases(size_t& Count) noexcept
{
	Count = sizeof(Cases) / sizeof(Cases[0]);
	return Cases;
}
} // namespace PhysicsTest
