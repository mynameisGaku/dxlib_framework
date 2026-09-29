// SPDX-License-Identifier: NOASSERTION
// 3D距離拘束のLocal Anchor姿勢回転と平面oracle（J4-B/RCA）。
// 確認:
// 主軸90度回転でWorld Anchorが解析値と一致する（Solverを動かさず検出）、
// 非主軸120度回転、getterが回転済みAnchorを使う、solverが回転済みAnchorを使う、
// 平面条件で2Dと3Dの1Step／複数Stepが一致する、off-center 20Step回帰、
// off-center 600Step有界、回転Anchor後のWarm Start、Kinematic回転Anchor、Static事前回転Anchor。
// 期待値は90度主軸と120度(1,1,1)の解析値から作る。実装と同じhelperで期待値を生成しない。
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 全ケースの固定Step。baselineと同じdtを使う。
constexpr f64 Step_Internal = 1.0 / 60.0;
// 姿勢回転の解析比較許容。f32の姿勢とf64内部計算の丸め量。
constexpr f64 FrameTolerance_Internal = 1e-5;
// 2D/3D平面対応の許容。f32状態と積分方式の差を考慮する。
constexpr f64 PlanarTolerance_Internal = 1e-3;
// 90度回転のHalf Sine/Cosine。sqrt(2)/2。
constexpr f32 HalfSine_Internal = 0.70710678118654752440f;
// 120度回転（1,1,1軸）のHalf Sine/Cosine。sin60/2 と cos60。
// 120度回転（1,1,1軸）の単位四元数成分。
// q = (sin(60) * axis, cos(60))、axis = (1,1,1)/sqrt(3) なので
// sin(60)/sqrt(3) = 0.5、cos(60) = 0.5。単位四元数{(0.5,0.5,0.5,0.5)}。
// この回転は x->y, y->z, z->x の循環になる。
constexpr f32 Axis120Sine_Internal = 0.5f;
constexpr f32 Axis120Cosine_Internal = 0.5f;

// 平面条件で2Dと3DのBodyを作る。質量1・慣性1。
struct F3DPlanar
{
	FBodyDescription3D Make(f32 X, f32 Y, EBodyType Type, f32 InertiaZ)
	{
		FBodyDescription3D Description;
		Description.Type = Type;
		Description.Position = {X, Y, 0};
		Description.Mass = 1;
		// 平面運動はZ軸まわりだけなのでZ慣性を2Dのスカラー慣性へ一致させる。
		// X/Yは平面運動へ影響しない値にして別問題を混ぜない。
		Description.DiagonalInertia = {1, 1, InertiaZ};
		return Description;
	}
};
// 平面条件で2DのBodyを作る。
struct F2DPlanar
{
	FBodyDescription2D Make(f32 X, f32 Y, EBodyType Type)
	{
		FBodyDescription2D Description;
		Description.Type = Type;
		Description.Position = {X, Y};
		Description.Mass = 1;
		Description.Inertia = 1;
		return Description;
	}
};
// 休止を切り、各Worldの実行設定を同一にする。
void Prepare2D(FPhysicsWorld2D& World)
{
	World.SetGravity({0, 0});
	FSleepSettings2D Sleep = World.GetSleepSettings();
	Sleep.bEnabled = false;
	World.SetSleepSettings(Sleep);
}
void Prepare3D(FPhysicsWorld3D& World)
{
	World.SetGravity({0, 0, 0});
	FSleepSettings3D Sleep = World.GetSleepSettings();
	Sleep.bEnabled = false;
	World.SetSleepSettings(Sleep);
}

void RotatedAnchorZ90_Internal()
{
	// Bの姿勢がZ軸まわり90度。LocalAnchorB=(0, 0.5, 0)を回転すると(-0.5, 0, 0)。
	// World Anchorは(2, 0, 0) + (-0.5, 0, 0) = (1.5, 0, 0)。A側Anchorが原点なので長さは1.5。
	// Anchorが回転していなければ(2, 0.5, 0)になり長さsqrt(4.25) ≈ 2.0616で区別できる。
	// Solver Stepは行わない。初期姿勢だけで検出する。
	FPhysicsWorld3D World;
	Prepare3D(World);
	FBodyDescription3D AnchorDescription;
	AnchorDescription.Type = EBodyType::Static;
	AnchorDescription.Position = {0, 0, 0};
	const FBodyId3D Anchor = World.CreateBody(AnchorDescription);
	FBodyDescription3D BodyDescription;
	BodyDescription.Position = {2, 0, 0};
	BodyDescription.Orientation = {0, 0, HalfSine_Internal, HalfSine_Internal};
	const FBodyId3D Body = World.CreateBody(BodyDescription);
	FDistanceJointDescription3D Joint;
	Joint.Length = 1.5;
	Joint.LocalAnchorB = {0, 0.5f, 0};
	const FJointId3D Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	const FDistanceJointState3D State = World.GetDistanceJoint(Id);
	PHYSICS_REQUIRE(Toolbox::Abs(State.CurrentLength - 1.5) <= FrameTolerance_Internal);
	PHYSICS_REQUIRE(Toolbox::Abs(State.Error) <= FrameTolerance_Internal);
}

void RotatedAnchorX90_Internal()
{
	// X軸まわり+90度。右手系でy→zなので(0, 1, 0)は(0, 0, 1)へ移る。
	// 重心(2,0,0)との距離はsqrt(4 + 1) = sqrt(5)。
	FPhysicsWorld3D World;
	Prepare3D(World);
	FBodyDescription3D AnchorDescription;
	AnchorDescription.Type = EBodyType::Static;
	AnchorDescription.Position = {0, 0, 0};
	const FBodyId3D Anchor = World.CreateBody(AnchorDescription);
	FBodyDescription3D BodyDescription;
	BodyDescription.Position = {2, 0, 0};
	BodyDescription.Orientation = {HalfSine_Internal, 0, 0, HalfSine_Internal};
	const FBodyId3D Body = World.CreateBody(BodyDescription);
	FDistanceJointDescription3D Joint;
	Joint.Length = 2.2360679774997896;
	Joint.LocalAnchorB = {0, 1, 0};
	const FJointId3D Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	PHYSICS_REQUIRE(Toolbox::Abs(World.GetDistanceJoint(Id).CurrentLength - 2.2360679774997896) <=
	                FrameTolerance_Internal);
}

void RotatedAnchorY90_Internal()
{
	// Y軸まわり+90度。右手系でz→x、x→-zなので(1, 0, 0)は(0, 0, -1)へ移る。
	// 重心(2,0,0)とのAnchor距離はsqrt(4 + 1) = sqrt(5)。
	FPhysicsWorld3D World;
	Prepare3D(World);
	FBodyDescription3D AnchorDescription;
	AnchorDescription.Type = EBodyType::Static;
	AnchorDescription.Position = {0, 0, 0};
	const FBodyId3D Anchor = World.CreateBody(AnchorDescription);
	FBodyDescription3D BodyDescription;
	BodyDescription.Position = {2, 0, 0};
	BodyDescription.Orientation = {0, HalfSine_Internal, 0, HalfSine_Internal};
	const FBodyId3D Body = World.CreateBody(BodyDescription);
	FDistanceJointDescription3D Joint;
	Joint.Length = 2.2360679774997896;
	Joint.LocalAnchorB = {1, 0, 0};
	const FJointId3D Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	PHYSICS_REQUIRE(Toolbox::Abs(World.GetDistanceJoint(Id).CurrentLength - 2.2360679774997896) <=
	                FrameTolerance_Internal);
}

void RotatedAnchorNonAxis_Internal()
{
	// (1,1,1)軸まわり120度はx→y, y→z, z→xの循環回転。
	// LocalAnchorB=(1, 0, 0)は(0, 1, 0)へ移る。
	// 重心(2,0,0)とのAnchor距離はsqrt(4 + 1) = sqrt(5)。
	FPhysicsWorld3D World;
	Prepare3D(World);
	FBodyDescription3D AnchorDescription;
	AnchorDescription.Type = EBodyType::Static;
	AnchorDescription.Position = {0, 0, 0};
	const FBodyId3D Anchor = World.CreateBody(AnchorDescription);
	FBodyDescription3D BodyDescription;
	BodyDescription.Position = {2, 0, 0};
	BodyDescription.Orientation = {Axis120Sine_Internal, Axis120Sine_Internal, Axis120Sine_Internal, Axis120Cosine_Internal};
	const FBodyId3D Body = World.CreateBody(BodyDescription);
	FDistanceJointDescription3D Joint;
	Joint.Length = 2.2360679774997896;
	Joint.LocalAnchorB = {1, 0, 0};
	const FJointId3D Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	PHYSICS_REQUIRE(Toolbox::Abs(World.GetDistanceJoint(Id).CurrentLength - 2.2360679774997896) <=
	                FrameTolerance_Internal);
}

void StaticPreRotatedAnchor_Internal()
{
	// Static Bodyを最初から回転姿勢で作り、Static側のLocalAnchorを持つ。
	// Step0のCurrentLengthで判定するのでIntegratorではなくFrame変換を検査する。
	// AのLocalAnchor(1,0,0)は90度Z回転で(0,1,0)へ移る。BのAnchorは重心そのもの。
	// 回転していれば距離はsqrt(2^2 + 1^2) = sqrt(5)、回転していなければ1.0で区別できる。
	FPhysicsWorld3D World;
	Prepare3D(World);
	FBodyDescription3D AnchorDescription;
	AnchorDescription.Type = EBodyType::Static;
	AnchorDescription.Position = {0, 0, 0};
	AnchorDescription.Orientation = {0, 0, HalfSine_Internal, HalfSine_Internal};
	const FBodyId3D Anchor = World.CreateBody(AnchorDescription);
	FBodyDescription3D BodyDescription;
	BodyDescription.Position = {2, 0, 0};
	const FBodyId3D Body = World.CreateBody(BodyDescription);
	FDistanceJointDescription3D Joint;
	Joint.Length = 2.2360679774997896;
	Joint.LocalAnchorA = {1, 0, 0};
	const FJointId3D Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	PHYSICS_REQUIRE(Toolbox::Abs(World.GetDistanceJoint(Id).CurrentLength - 2.2360679774997896) <=
	                FrameTolerance_Internal);
}

void SolverUsesRotatedAnchor_Internal()
{
	// Solverが回転済みAnchorを使う。長さ1.5の拘束で初期状態から満たし、
	// Stepしても誤差が増えないことを見る。
	FPhysicsWorld3D World;
	Prepare3D(World);
	FBodyDescription3D AnchorDescription;
	AnchorDescription.Type = EBodyType::Static;
	AnchorDescription.Position = {0, 0, 0};
	const FBodyId3D Anchor = World.CreateBody(AnchorDescription);
	FBodyDescription3D BodyDescription;
	BodyDescription.Position = {2, 0, 0};
	BodyDescription.Orientation = {0, 0, HalfSine_Internal, HalfSine_Internal};
	const FBodyId3D Body = World.CreateBody(BodyDescription);
	FDistanceJointDescription3D Joint;
	Joint.Length = 1.5;
	Joint.LocalAnchorB = {0, 0.5f, 0};
	const FJointId3D Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	World.Step(Step_Internal);
	World.Step(Step_Internal);
	PHYSICS_REQUIRE(Toolbox::Abs(World.GetDistanceJoint(Id).CurrentLength - 1.5) <= 1e-3);
}

void PlanarOneStepMatches_Internal()
{
	// 平面条件（Z=0, 回転軸=Z, ω=(0,0,w), LocalAnchorのZ=0）で2Dと3Dの1 Stepを比較する。
	// 2DがOracle。重心Anchorの対称ケースから始める。
	FPhysicsWorld2D World2D;
	Prepare2D(World2D);
	F2DPlanar Make2D;
	const FBodyId2D Anchor2D = World2D.CreateBody(Make2D.Make(0, 0, EBodyType::Static));
	const FBodyId2D Body2D = World2D.CreateBody(Make2D.Make(2, 0, EBodyType::Dynamic));
	FDistanceJointDescription2D Joint2D;
	Joint2D.Length = 2.0;
	const FJointId2D Id2D = World2D.CreateDistanceJoint(Anchor2D, Body2D, Joint2D);
	World2D.SetVelocity(Body2D, {1, 0.5f});
	World2D.Step(Step_Internal);

	FPhysicsWorld3D World3D;
	Prepare3D(World3D);
	F3DPlanar Make3D;
	const FBodyId3D Anchor3D = World3D.CreateBody(Make3D.Make(0, 0, EBodyType::Static, 1));
	const FBodyId3D Body3D = World3D.CreateBody(Make3D.Make(2, 0, EBodyType::Dynamic, 1));
	FDistanceJointDescription3D Joint3D;
	Joint3D.Length = 2.0;
	const FJointId3D Id3D = World3D.CreateDistanceJoint(Anchor3D, Body3D, Joint3D);
	World3D.SetVelocity(Body3D, {1, 0.5f, 0});
	World3D.Step(Step_Internal);

	const FVector2 P2D = World2D.GetPosition(Body2D);
	const FVector3 P3D = World3D.GetPosition(Body3D);
	PHYSICS_REQUIRE(Toolbox::Abs(P2D.X - P3D.X) <= PlanarTolerance_Internal);
	PHYSICS_REQUIRE(Toolbox::Abs(P2D.Y - P3D.Y) <= PlanarTolerance_Internal);
	const FVector2 V2D = World2D.GetVelocity(Body2D);
	const FVector3 V3D = World3D.GetVelocity(Body3D);
	PHYSICS_REQUIRE(Toolbox::Abs(V2D.X - V3D.X) <= PlanarTolerance_Internal);
	PHYSICS_REQUIRE(Toolbox::Abs(V2D.Y - V3D.Y) <= PlanarTolerance_Internal);
	PHYSICS_REQUIRE(Toolbox::Abs(World2D.GetDistanceJoint(Id2D).CurrentLength -
	                             World3D.GetDistanceJoint(Id3D).CurrentLength) <= PlanarTolerance_Internal);
}

void PlanarMultiStepMatches_Internal()
{
	// 平面条件で120 Step進めた位置と速度を比較する。
	// 重心を外したAnchorにして回転の寄与も含む比較にする。
	FPhysicsWorld2D World2D;
	Prepare2D(World2D);
	F2DPlanar Make2D;
	const FBodyId2D Anchor2D = World2D.CreateBody(Make2D.Make(0, 0, EBodyType::Static));
	const FBodyId2D Body2D = World2D.CreateBody(Make2D.Make(2, 0, EBodyType::Dynamic));
	FDistanceJointDescription2D Joint2D;
	Joint2D.Length = 2.0615528128088303;
	Joint2D.LocalAnchorA = {0, 0.5f};
	const FJointId2D Id2D = World2D.CreateDistanceJoint(Anchor2D, Body2D, Joint2D);
	World2D.SetVelocity(Body2D, {-1, 0});

	FPhysicsWorld3D World3D;
	Prepare3D(World3D);
	F3DPlanar Make3D;
	const FBodyId3D Anchor3D = World3D.CreateBody(Make3D.Make(0, 0, EBodyType::Static, 1));
	const FBodyId3D Body3D = World3D.CreateBody(Make3D.Make(2, 0, EBodyType::Dynamic, 1));
	FDistanceJointDescription3D Joint3D;
	Joint3D.Length = 2.0615528128088303;
	Joint3D.LocalAnchorA = {0, 0.5f, 0};
	const FJointId3D Id3D = World3D.CreateDistanceJoint(Anchor3D, Body3D, Joint3D);
	World3D.SetVelocity(Body3D, {-1, 0, 0});

	for (uint32 Index = 0; Index < 120; ++Index)
	{
		World2D.Step(Step_Internal);
		World3D.Step(Step_Internal);
	}
	const FVector2 P2D = World2D.GetPosition(Body2D);
	const FVector3 P3D = World3D.GetPosition(Body3D);
	PHYSICS_REQUIRE(Toolbox::Abs(P2D.X - P3D.X) <= 0.05f);
	PHYSICS_REQUIRE(Toolbox::Abs(P2D.Y - P3D.Y) <= 0.05f);
	// 角速度もZ成分で比較する。
	PHYSICS_REQUIRE(Toolbox::Abs(World2D.GetAngularVelocity(Body2D) - World3D.GetAngularVelocity(Body3D).Z) <=
	                0.05f);
	PHYSICS_REQUIRE(Toolbox::Abs(World2D.GetDistanceJoint(Id2D).CurrentLength -
	                             World3D.GetDistanceJoint(Id3D).CurrentLength) <= 0.05f);
}

void OffCenter20StepBounded_Internal()
{
	// 回転off-centerの20 Step。baseline 0.057429 の1/10以下を固定する。
	FPhysicsWorld3D World;
	Prepare3D(World);
	FBodyDescription3D AnchorDescription;
	AnchorDescription.Type = EBodyType::Static;
	AnchorDescription.Position = {0, 0, 0};
	const FBodyId3D Anchor = World.CreateBody(AnchorDescription);
	FBodyDescription3D BodyDescription;
	BodyDescription.Position = {2, 0, 0};
	BodyDescription.Velocity = {-1, 0, 0};
	BodyDescription.Mass = 1;
	BodyDescription.DiagonalInertia = {1, 1, 1};
	const FBodyId3D Body = World.CreateBody(BodyDescription);
	FDistanceJointDescription3D Joint;
	Joint.Length = 2.0615528128088303;
	Joint.LocalAnchorB = {0, 0.5f, 0};
	const FJointId3D Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	FDistanceJointState3D State;
	for (uint32 Index = 0; Index < 20; ++Index)
	{
		World.Step(Step_Internal);
		State = World.GetDistanceJoint(Id);
	}
	PHYSICS_REQUIRE(Toolbox::Abs(State.Error) <= 0.006);
	// Quaternionは有限かつ正規化を維持する。
	const FQuaternion Q = World.GetOrientation(Body);
	const f64 Norm = Toolbox::Sqrt(Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z + Q.W * Q.W);
	PHYSICS_REQUIRE(Toolbox::IsFinite(Norm));
	PHYSICS_REQUIRE(Toolbox::Abs(Norm - 1.0) <= 1e-5);
}

void OffCenter600StepBounded_Internal()
{
	// 回転off-centerの600 Stepが有界であること。baselineはmax 0.997537。
	FPhysicsWorld3D World;
	Prepare3D(World);
	FBodyDescription3D AnchorDescription;
	AnchorDescription.Type = EBodyType::Static;
	AnchorDescription.Position = {0, 0, 0};
	const FBodyId3D Anchor = World.CreateBody(AnchorDescription);
	FBodyDescription3D BodyDescription;
	BodyDescription.Position = {2, 0, 0};
	BodyDescription.Velocity = {-1, 0, 0};
	BodyDescription.Mass = 1;
	BodyDescription.DiagonalInertia = {1, 1, 1};
	const FBodyId3D Body = World.CreateBody(BodyDescription);
	FDistanceJointDescription3D Joint;
	Joint.Length = 2.0615528128088303;
	Joint.LocalAnchorB = {0, 0.5f, 0};
	const FJointId3D Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	f64 MaxError = 0;
	FDistanceJointState3D State;
	for (uint32 Index = 0; Index < 600; ++Index)
	{
		World.Step(Step_Internal);
		State = World.GetDistanceJoint(Id);
		const f64 Magnitude = State.Error < 0 ? -State.Error : State.Error;
		if (Magnitude > MaxError)
		{
			MaxError = Magnitude;
		}
	}
	PHYSICS_REQUIRE(MaxError <= 0.05);
	PHYSICS_REQUIRE(Toolbox::Abs(State.Error) <= 0.02);
	const FVector3 Position = World.GetPosition(Body);
	const FVector3 Velocity = World.GetVelocity(Body);
	const FVector3 Spin = World.GetAngularVelocity(Body);
	PHYSICS_REQUIRE(Position.IsValid());
	PHYSICS_REQUIRE(Velocity.IsValid());
	PHYSICS_REQUIRE(Spin.IsValid());
}

void OffCenter2DUnchanged_Internal()
{
	// 2D側の非退行。J3 baselineは600 Stepでmax 0.000018。
	FPhysicsWorld2D World;
	Prepare2D(World);
	F2DPlanar Make;
	const FBodyId2D Anchor = World.CreateBody(Make.Make(0, 0, EBodyType::Static));
	const FBodyId2D Body = World.CreateBody(Make.Make(2, 0, EBodyType::Dynamic));
	FDistanceJointDescription2D Joint;
	Joint.Length = 2.0615528128088303;
	Joint.LocalAnchorB = {0, 0.5f};
	const FJointId2D Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	World.SetVelocity(Body, {-1, 0});
	f64 MaxError = 0;
	for (uint32 Index = 0; Index < 600; ++Index)
	{
		World.Step(Step_Internal);
		const f64 Error = World.GetDistanceJoint(Id).Error;
		const f64 Magnitude = Error < 0 ? -Error : Error;
		if (Magnitude > MaxError)
		{
			MaxError = Magnitude;
		}
	}
	PHYSICS_REQUIRE(MaxError <= 0.001);
}

void WarmStartAfterRotatingAnchor_Internal()
{
	// 回転AnchorをJointで保持したままWarm Startが破綻しないこと。
	// 初期誤差が存在する配置で速度を与え、誤差が拡大しないことを見る。
	FPhysicsWorld3D World;
	Prepare3D(World);
	FBodyDescription3D AnchorDescription;
	AnchorDescription.Type = EBodyType::Static;
	AnchorDescription.Position = {0, 0, 0};
	const FBodyId3D Anchor = World.CreateBody(AnchorDescription);
	FBodyDescription3D BodyDescription;
	BodyDescription.Position = {2, 0, 0};
	BodyDescription.Velocity = {-1, 0, 0};
	BodyDescription.Mass = 1;
	BodyDescription.DiagonalInertia = {1, 1, 1};
	const FBodyId3D Body = World.CreateBody(BodyDescription);
	FDistanceJointDescription3D Joint;
	Joint.Length = 2.0;
	Joint.LocalAnchorB = {0, 0.5f, 0};
	const FJointId3D Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	// 初期Anchorは(2, 0.5, 0)で長さsqrt(4.25)。初期誤差が存在するため運動する。
	PHYSICS_REQUIRE(Toolbox::IsFinite(World.GetDistanceJoint(Id).CurrentLength));
	for (uint32 Index = 0; Index < 60; ++Index)
	{
		World.Step(Step_Internal);
		PHYSICS_REQUIRE(World.IsJointAlive(Id));
	}
	PHYSICS_REQUIRE(Toolbox::Abs(World.GetDistanceJoint(Id).Error) <= 0.05);
}

void KinematicRotatingAnchorStaysFinite_Internal()
{
	// Kinematic Anchorを角速度で回すとAnchorが円を描く。Jointがfiniteを保ち、
	// 長さ誤差が有界であることを見る。
	FPhysicsWorld3D World;
	Prepare3D(World);
	FBodyDescription3D AnchorDescription;
	AnchorDescription.Type = EBodyType::Kinematic;
	AnchorDescription.Position = {0, 0, 0};
	const FBodyId3D Anchor = World.CreateBody(AnchorDescription);
	FBodyDescription3D BodyDescription;
	BodyDescription.Position = {2, 0, 0};
	BodyDescription.Mass = 1;
	BodyDescription.DiagonalInertia = {1, 1, 1};
	const FBodyId3D Body = World.CreateBody(BodyDescription);
	FDistanceJointDescription3D Joint;
	Joint.Length = 2.0;
	Joint.LocalAnchorA = {0, 0.5f, 0};
	const FJointId3D Id = World.CreateDistanceJoint(Anchor, Body, Joint);
	World.SetAngularVelocity(Anchor, {0, 0, 1.5f});
	for (uint32 Index = 0; Index < 60; ++Index)
	{
		World.Step(Step_Internal);
		PHYSICS_REQUIRE(World.IsJointAlive(Id));
	}
	const FDistanceJointState3D State = World.GetDistanceJoint(Id);
	PHYSICS_REQUIRE(Toolbox::IsFinite(State.CurrentLength));
	PHYSICS_REQUIRE(Toolbox::Abs(State.Error) <= 0.1);
	PHYSICS_REQUIRE(World.GetPosition(Body).IsValid());
	PHYSICS_REQUIRE(World.GetAngularVelocity(Body).IsValid());
}

const PhysicsTest::FCase Cases_Internal[] = {
    {"3D local anchor rotates 90 degrees about Z", &RotatedAnchorZ90_Internal},
    {"3D local anchor rotates 90 degrees about X", &RotatedAnchorX90_Internal},
    {"3D local anchor rotates 90 degrees about Y", &RotatedAnchorY90_Internal},
    {"3D local anchor rotates 120 degrees about the diagonal axis", &RotatedAnchorNonAxis_Internal},
    {"3D static pre-rotated anchor is read at step zero", &StaticPreRotatedAnchor_Internal},
    {"3D solver uses the rotated anchor", &SolverUsesRotatedAnchor_Internal},
    {"planar 2D and 3D match after one step", &PlanarOneStepMatches_Internal},
    {"planar 2D and 3D match after 120 steps", &PlanarMultiStepMatches_Internal},
    {"3D off-center error stays bounded for 20 steps", &OffCenter20StepBounded_Internal},
    {"3D off-center error stays bounded for 600 steps", &OffCenter600StepBounded_Internal},
    {"2D off-center error does not regress", &OffCenter2DUnchanged_Internal},
    {"3D warm start survives a rotating anchor", &WarmStartAfterRotatingAnchor_Internal},
    {"3D kinematic rotating anchor stays finite", &KinematicRotatingAnchorStaysFinite_Internal}};
} // namespace

const PhysicsTest::FCase* PhysicsTest::GetDistanceJoint3DFrameCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
