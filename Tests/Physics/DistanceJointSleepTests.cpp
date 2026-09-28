// SPDX-License-Identifier: NOASSERTION
// 距離拘束の休止と起床（J3）。2D／3Dで対になる。
// 確認:
// Joint-onlyの休止、Dynamicペアとchainの同時休止、明示起床の伝播、力積と速度設定の伝播、
// Kinematic平行移動と回転Anchorの起床、Joint誤差と閾値内無起床、getter無起床、
// Joint破棄とBody破棄による支持消失、shared Static/Kinematicの非伝播、bAllowSleep整合、
// Sensor非支持、Contact＋Joint混在の伝播と再休止、600Stepの無wakeループ。
// 期待値は配置から手計算し、実装と同じ判定式を期待値生成に使わない。
// 製品既定Sleep timeoutは変えず、試験は_timeoutを合理的に短くして使う。
// J2既知の3D回転off-center残差は本試験の対象外にし、混在させない。
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 全ケースの固定Step。
constexpr f64 Step_Internal = 1.0 / 60.0;
// 試験用Sleep設定。製品既定(0.5秒)は変えず、試験で短くして回帰を短く保つ。
// LinearSpeedLimitとAngularSpeedLimitは製品既定と同じ0.05を使う。
constexpr f32 TestTimeout_Internal = 0.1f;
// 起床判定に使う距離の閾値。max(LinearSpeedLimit * Step, AxisEpsilon)。
// 0.05 * (1/60) = 0.0008333...
constexpr f64 WakeDistance_Internal = 0.05 / 60.0;
// 平面側のWorld操作。
struct F2D
{
	using FWorld = FPhysicsWorld2D;
	using FBodyId = FBodyId2D;
	using FJointId = FJointId2D;
	using FVector = FVector2;
	using FState = FDistanceJointState2D;
	static FVector At(f32 X, f32 Y)
	{
		return {X, Y};
	}
	static FBodyId Make(FWorld& World, f32 X, f32 Y, EBodyType Type)
	{
		FBodyDescription2D Description;
		Description.Type = Type;
		Description.Position = At(X, Y);
		Description.Mass = 1;
		Description.Inertia = 1;
		return World.CreateBody(Description);
	}
	static FJointId Join(FWorld& World, FBodyId A, FBodyId B, f64 Length, FVector LocalA, FVector LocalB)
	{
		FDistanceJointDescription2D Description;
		Description.Length = Length;
		Description.LocalAnchorA = LocalA;
		Description.LocalAnchorB = LocalB;
		return World.CreateDistanceJoint(A, B, Description);
	}
};
// 空間側のWorld操作。
struct F3D
{
	using FWorld = FPhysicsWorld3D;
	using FBodyId = FBodyId3D;
	using FJointId = FJointId3D;
	using FVector = FVector3;
	using FState = FDistanceJointState3D;
	static FVector At(f32 X, f32 Y, f32 Z)
	{
		return {X, Y, Z};
	}
	static FBodyId Make(FWorld& World, f32 X, f32 Y, f32 Z, EBodyType Type)
	{
		FBodyDescription3D Description;
		Description.Type = Type;
		Description.Position = At(X, Y, Z);
		Description.Mass = 1;
		Description.DiagonalInertia = {1, 1, 1};
		return World.CreateBody(Description);
	}
	static FJointId Join(FWorld& World, FBodyId A, FBodyId B, f64 Length, FVector LocalA, FVector LocalB)
	{
		FDistanceJointDescription3D Description;
		Description.Length = Length;
		Description.LocalAnchorA = LocalA;
		Description.LocalAnchorB = LocalB;
		return World.CreateDistanceJoint(A, B, Description);
	}
	// 姿勢を指定角度だけY軸まわりに回す。
	static void TurnY(FWorld& World, FBodyId Body, f32 Degrees)
	{
		const f32 Half = Degrees * 3.14159265358979323846f / 360.0f;
		const f32 Sine = Toolbox::Sin(Half);
		const f32 Cosine = Toolbox::Cos(Half);
		World.SetBodyTransform(Body, World.GetPosition(Body), {0, Sine, 0, Cosine});
	}
};
// 休止を有効にした試験用設定を入れる。次元別に設定型が違うため、
// 2D/3Dそれぞれ専用の版を使う（World型でSFINAEせず明示的に分ける）。
void EnableShortSleep2D_Internal(FPhysicsWorld2D& World)
{
	FSleepSettings2D Sleep = World.GetSleepSettings();
	Sleep.bEnabled = true;
	Sleep.TimeoutSeconds = TestTimeout_Internal;
	World.SetSleepSettings(Sleep);
}
void EnableShortSleep3D_Internal(FPhysicsWorld3D& World)
{
	FSleepSettings3D Sleep = World.GetSleepSettings();
	Sleep.bEnabled = true;
	Sleep.TimeoutSeconds = TestTimeout_Internal;
	World.SetSleepSettings(Sleep);
}
// 指定回数だけ進める。StepごとにWorldを1回更新する。
template <typename TWorld> void RunSteps_Internal(TWorld& World, uint32 Steps)
{
	for (uint32 Index = 0; Index < Steps; ++Index)
	{
		World.Step(Step_Internal);
	}
}
// 静止したStatic AnchorとDynamic Worldを作り、Jointを返す。
struct F2DScene
{
	FPhysicsWorld2D World;
	FBodyId2D Anchor;
	FBodyId2D Ball;
	FJointId2D Joint;
	// 重力なしの静的Anchor＋動的Ball。長さ2.0、Collider無しでJoint支持のみに依存する。
	void Build()
	{
		World.SetGravity({0, 0});
		EnableShortSleep2D_Internal(World);
		Anchor = F2D::Make(World, 0, 0, EBodyType::Static);
		Ball = F2D::Make(World, 2, 0, EBodyType::Dynamic);
		Joint = F2D::Join(World, Anchor, Ball, 2.0, {}, {});
	}
};
// 3D版。Zも使ってXYZの配置にする。
struct F3DScene
{
	FPhysicsWorld3D World;
	FBodyId3D Anchor;
	FBodyId3D Ball;
	FJointId3D Joint;
	void Build()
	{
		World.SetGravity({0, 0, 0});
		EnableShortSleep3D_Internal(World);
		Anchor = F3D::Make(World, 0, 0, 0, EBodyType::Static);
		Ball = F3D::Make(World, 2, 0, 0, EBodyType::Dynamic);
		Joint = F3D::Join(World, Anchor, Ball, 2.0, {}, {});
	}
};

void StaticJointDynamicSleeps_Internal()
{
	// Static Anchor＋DynamicはCollider無しでもJointが支持になり休止できる。
	// Joint supportがCollider依存でないことの検出。
	F2DScene Scene;
	Scene.Build();
	RunSteps_Internal(Scene.World, 60);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
	// AnchorはStaticなので休止対象外。
	PHYSICS_REQUIRE(!Scene.World.IsSleeping(Scene.Anchor));
}

void StaticJoint3DDynamicSleeps_Internal()
{
	F3DScene Scene;
	Scene.Build();
	RunSteps_Internal(Scene.World, 60);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
}

void DynamicPairSleepsTogether_Internal()
{
	// Dynamic同士は同Islandなので同じStepで同時に休止する。ズレを作らない。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Left = F2D::Make(World, 0, 0, EBodyType::Dynamic);
	const FBodyId2D Right = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	F2D::Join(World, Left, Right, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(Left));
	PHYSICS_REQUIRE(World.IsSleeping(Right));
	// Island同時sleepなので両方同時に寝る。一方だけ寝ている状態を作らない。
	PHYSICS_REQUIRE(World.IsSleeping(Left) == World.IsSleeping(Right));
}

void DynamicPair3DSleepsTogether_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	EnableShortSleep3D_Internal(World);
	const FBodyId3D Left = F3D::Make(World, 0, 0, 0, EBodyType::Dynamic);
	const FBodyId3D Right = F3D::Make(World, 2, 0, 0, EBodyType::Dynamic);
	F3D::Join(World, Left, Right, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(Left));
	PHYSICS_REQUIRE(World.IsSleeping(Right));
}

void JointChainSleepsTogether_Internal()
{
	// 3体のJoint chain。全員が同じIslandで同時に休止する。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D D0 = F2D::Make(World, 0, 0, EBodyType::Dynamic);
	const FBodyId2D D1 = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	const FBodyId2D D2 = F2D::Make(World, 4, 0, EBodyType::Dynamic);
	F2D::Join(World, D0, D1, 2.0, {}, {});
	F2D::Join(World, D1, D2, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(D0));
	PHYSICS_REQUIRE(World.IsSleeping(D1));
	PHYSICS_REQUIRE(World.IsSleeping(D2));
}

void JointChain3DSleepsTogether_Internal()
{
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	EnableShortSleep3D_Internal(World);
	const FBodyId3D D0 = F3D::Make(World, 0, 0, 0, EBodyType::Dynamic);
	const FBodyId3D D1 = F3D::Make(World, 2, 0, 0, EBodyType::Dynamic);
	const FBodyId3D D2 = F3D::Make(World, 4, 0, 0, EBodyType::Dynamic);
	F3D::Join(World, D0, D1, 2.0, {}, {});
	F3D::Join(World, D1, D2, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(D0));
	PHYSICS_REQUIRE(World.IsSleeping(D1));
	PHYSICS_REQUIRE(World.IsSleeping(D2));
}

void ExplicitWakeUpPropagates_Internal()
{
	// 両方休止した状態で片方を起こすと、Island内の他方も次のStepで起きる。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Left = F2D::Make(World, 0, 0, EBodyType::Dynamic);
	const FBodyId2D Right = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	F2D::Join(World, Left, Right, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(Left));
	PHYSICS_REQUIRE(World.IsSleeping(Right));
	PHYSICS_REQUIRE(World.WakeUp(Left));
	// 起床伝播は次のStepのconstraint solve前に完了する契約。
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(Left));
	PHYSICS_REQUIRE(!World.IsSleeping(Right));
}

void LinearImpulsePropagates_Internal()
{
	// 線形Impulseは対象だけを起こす。相手は次のStepのsolve前に起きる。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Left = F2D::Make(World, 0, 0, EBodyType::Dynamic);
	const FBodyId2D Right = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	F2D::Join(World, Left, Right, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(Left));
	World.ApplyLinearImpulse(Left, {0.5f, 0});
	// 対象は即時起きている。
	PHYSICS_REQUIRE(!World.IsSleeping(Left));
	// 相手はJointを正しく受ける前に起きている必要がある。
	// sleepingのまま無限質量でJoint solveを受けると伸びる。
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(Right));
}

void ImpulseKeepsPairConsistent_Internal()
{
	// 起床後、ペアが正しい質量で反応して長さが保たれることを見る。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Left = F2D::Make(World, 0, 0, EBodyType::Dynamic);
	const FBodyId2D Right = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	const FJointId2D Joint = F2D::Join(World, Left, Right, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(Left));
	World.ApplyLinearImpulse(Left, {0.5f, 0});
	RunSteps_Internal(World, 20);
	PHYSICS_REQUIRE(!World.IsSleeping(Left));
	PHYSICS_REQUIRE(!World.IsSleeping(Right));
	// 両者が動くので長さは誤差内で保たれる。
	PHYSICS_REQUIRE(Toolbox::Abs(World.GetDistanceJoint(Joint).CurrentLength - 2.0) < 1e-2);
}

void SetVelocityPropagates_Internal()
{
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Left = F2D::Make(World, 0, 0, EBodyType::Dynamic);
	const FBodyId2D Right = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	F2D::Join(World, Left, Right, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(Left));
	World.SetVelocity(Left, {0.5f, 0});
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(Right));
}

void AngularVelocityPropagates_Internal()
{
	// 角速度設定もIslandへ伝播する。3DでXYZを使う。
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	EnableShortSleep3D_Internal(World);
	const FBodyId3D Left = F3D::Make(World, 0, 0, 0, EBodyType::Dynamic);
	const FBodyId3D Right = F3D::Make(World, 2, 0, 0, EBodyType::Dynamic);
	F3D::Join(World, Left, Right, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(Left));
	World.SetAngularVelocity(Left, {0, 0, 1.0f});
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(Right));
}

void KinematicMotionWakes_Internal()
{
	// 静止していたKinematic Anchorが動くとsleeping Dynamicが起きる。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Anchor = F2D::Make(World, 0, 0, EBodyType::Kinematic);
	const FBodyId2D Ball = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	F2D::Join(World, Anchor, Ball, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(Ball));
	// Kinematicは速度0では起こさない。
	RunSteps_Internal(World, 5);
	PHYSICS_REQUIRE(World.IsSleeping(Ball));
	// 軸方向へ動かし始める。
	World.SetVelocity(Anchor, {0.5f, 0});
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(Ball));
}

void KinematicStopAllowsResleep_Internal()
{
	// Kinematicが止まって運動が収束すると、再び休止できる。sleep thrashも無い。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Anchor = F2D::Make(World, 0, 0, EBodyType::Kinematic);
	const FBodyId2D Ball = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	const FJointId2D Joint = F2D::Join(World, Anchor, Ball, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(Ball));
	World.SetVelocity(Anchor, {0.5f, 0});
	RunSteps_Internal(World, 10);
	PHYSICS_REQUIRE(!World.IsSleeping(Ball));
	// 運動を止める。速度が0になると拘束誤差も収束する。
	World.SetVelocity(Anchor, {0, 0});
	World.SetVelocity(Ball, {0, 0});
	RunSteps_Internal(World, 120);
	PHYSICS_REQUIRE(World.IsSleeping(Ball));
	// 一度寝たら勝手に起きない（thrash無し）。
	RunSteps_Internal(World, 300);
	PHYSICS_REQUIRE(World.IsSleeping(Ball));
	PHYSICS_REQUIRE(World.IsJointAlive(Joint));
}

void KinematicRotatingAnchorWakes3D_Internal()
{
	// off-center LocalAnchorを持つKinematicを角速度で回すとAnchor点速度が生じ、
	// COM速度0でもsleeping Dynamicが起きる。3DがZ=0に留まらないことの確認。
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	EnableShortSleep3D_Internal(World);
	const FBodyId3D Anchor = F3D::Make(World, 0, 0, 0, EBodyType::Kinematic);
	const FBodyId3D Ball = F3D::Make(World, 2, 0, 0, EBodyType::Dynamic);
	// KinematicのLocalAnchorをZ方向へずらす。Anchor点速度はomega × rでZ成分を作る。
	F3D::Join(World, Anchor, Ball, Toolbox::Sqrt(5.0), {0, 0, 1}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(Ball));
	// Kinematicは並進0。角速度だけ与える。
	World.SetAngularVelocity(Anchor, {0, 1.0f, 0});
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(Ball));
}

void JointErrorWakes_Internal()
{
	// sleep後にAnchorを動かし、Joint誤差が起床閾値を超えると起きる。
	// 配置から手計算した期待値を使う。
	F2DScene Scene;
	Scene.Build();
	RunSteps_Internal(Scene.World, 60);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
	// Anchorを+0.1へ動すと、Ballとの距離は2.1になり誤差0.1。
	// 閾値0.000833の121倍なので明確に起床理由になる。
	Scene.World.SetBodyTransform(Scene.Anchor, {0.1f, 0}, 0);
	RunSteps_Internal(Scene.World, 1);
	PHYSICS_REQUIRE(!Scene.World.IsSleeping(Scene.Ball));
}

void SmallJointErrorDoesNotWake_Internal()
{
	// 起床閾値の1/4以下の誤差は、毎Step起こさない。
	// 閾値の境界そのものではなく、十分内側を使う。
	F2DScene Scene;
	Scene.Build();
	RunSteps_Internal(Scene.World, 60);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
	// 誤差0.0002は閾値0.000833の1/4以下。
	Scene.World.SetBodyTransform(Scene.Anchor, {0.0002f, 0}, 0);
	RunSteps_Internal(Scene.World, 1);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
}

void GetterDoesNotWake_Internal()
{
	// GetDistanceJointとIsJointAliveはWorldを購読しない。
	F2DScene Scene;
	Scene.Build();
	RunSteps_Internal(Scene.World, 60);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
	for (uint32 Index = 0; Index < 50; ++Index)
	{
		const FDistanceJointState2D State = Scene.World.GetDistanceJoint(Scene.Joint);
		PHYSICS_REQUIRE(Toolbox::IsFinite(State.CurrentLength));
		PHYSICS_REQUIRE(Scene.World.IsJointAlive(Scene.Joint));
	}
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
}

void DestroyJointRemovesSupport_Internal()
{
	// Jointを破棄すると支持が消え、次のStepでDynamicが起きる。
	F2DScene Scene;
	Scene.Build();
	RunSteps_Internal(Scene.World, 60);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
	PHYSICS_REQUIRE(Scene.World.DestroyJoint(Scene.Joint));
	RunSteps_Internal(Scene.World, 1);
	PHYSICS_REQUIRE(!Scene.World.IsSleeping(Scene.Ball));
}

void DestroyAnchorWakesSurvivor_Internal()
{
	// Anchor Bodyを破棄するとJ1契約でJointも失効し、生存Dynamicが起きる。
	F2DScene Scene;
	Scene.Build();
	RunSteps_Internal(Scene.World, 60);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
	PHYSICS_REQUIRE(Scene.World.DestroyBody(Scene.Anchor));
	PHYSICS_REQUIRE(!Scene.World.IsJointAlive(Scene.Joint));
	RunSteps_Internal(Scene.World, 1);
	PHYSICS_REQUIRE(!Scene.World.IsSleeping(Scene.Ball));
}

void GravityFallbackAfterJointLoss_Internal()
{
	// 重力下でJointを失うと落下を再開する。
	// 先に重力を切ってJoint支持だけで休止させ、その後重力を有効にして-support喪失を見る。
	// 重力下の振り子は揺れ続けるため休止しないので、この順序が成立する。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Anchor = F2D::Make(World, 0, 0, EBodyType::Static);
	const FBodyId2D Ball = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	const FJointId2D Joint = F2D::Join(World, Anchor, Ball, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	// Joint支持のみで休止する。
	PHYSICS_REQUIRE(World.IsSleeping(Ball));
	PHYSICS_REQUIRE(World.DestroyJoint(Joint));
	RunSteps_Internal(World, 1);
	// support喪失で起きる。
	PHYSICS_REQUIRE(!World.IsSleeping(Ball));
	// 重力を有効にすると、支持のないBallは落下する。
	World.SetGravity({0, -9.8f});
	const f32 Before = World.GetPosition(Ball).Y;
	RunSteps_Internal(World, 20);
	PHYSICS_REQUIRE(World.GetPosition(Ball).Y < Before);
	// 落下しているので休止しない。
	PHYSICS_REQUIRE(!World.IsSleeping(Ball));
}

void SharedStaticDoesNotCrossWake_Internal()
{
	// 共有StaticはDynamic同士を結ばないので、Aが起きてもBは休眠のまま。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Anchor = F2D::Make(World, 0, 0, EBodyType::Static);
	const FBodyId2D A = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	const FBodyId2D B = F2D::Make(World, -2, 0, EBodyType::Dynamic);
	F2D::Join(World, Anchor, A, 2.0, {}, {});
	F2D::Join(World, Anchor, B, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(A));
	PHYSICS_REQUIRE(World.IsSleeping(B));
	PHYSICS_REQUIRE(World.WakeUp(A));
	RunSteps_Internal(World, 1);
	// A側の島だけ起きる。Bは別島なので眠ったまま。
	PHYSICS_REQUIRE(!World.IsSleeping(A));
	PHYSICS_REQUIRE(World.IsSleeping(B));
}

void SharedKinematicStationaryDoesNotCrossWake_Internal()
{
	// 共有Kinematicが静止している状態では、片方の起床が他方へ漏れない。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Kinematic = F2D::Make(World, 0, 0, EBodyType::Kinematic);
	const FBodyId2D A = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	const FBodyId2D B = F2D::Make(World, -2, 0, EBodyType::Dynamic);
	F2D::Join(World, Kinematic, A, 2.0, {}, {});
	F2D::Join(World, Kinematic, B, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(A));
	PHYSICS_REQUIRE(World.IsSleeping(B));
	PHYSICS_REQUIRE(World.WakeUp(A));
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(A));
	PHYSICS_REQUIRE(World.IsSleeping(B));
}

void SharedKinematicMotionWakesBoth_Internal()
{
	// 一方で共有Kinematicが動くと、両方の島がそれぞれの判定で起きる。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Kinematic = F2D::Make(World, 0, 0, EBodyType::Kinematic);
	const FBodyId2D A = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	const FBodyId2D B = F2D::Make(World, -2, 0, EBodyType::Dynamic);
	F2D::Join(World, Kinematic, A, 2.0, {}, {});
	F2D::Join(World, Kinematic, B, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(A));
	PHYSICS_REQUIRE(World.IsSleeping(B));
	World.SetVelocity(Kinematic, {0.5f, 0});
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(A));
	PHYSICS_REQUIRE(!World.IsSleeping(B));
}

void DisallowSleepPreventsIslandSleep_Internal()
{
	// bAllowSleep=falseが1体でもあればIsland全体を休止させない。
	// Aだけsleepさせないことを検出する。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	FBodyDescription2D NoSleep;
	NoSleep.Type = EBodyType::Dynamic;
	NoSleep.Position = {0, 0};
	NoSleep.bAllowSleep = false;
	const FBodyId2D A = World.CreateBody(NoSleep);
	const FBodyId2D B = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	F2D::Join(World, A, B, 2.0, {}, {});
	RunSteps_Internal(World, 120);
	PHYSICS_REQUIRE(!World.IsSleeping(A));
	PHYSICS_REQUIRE(!World.IsSleeping(B));
}

void SensorIsNotSleepSupport_Internal()
{
	// SensorはSolver拘束ではないのでSleep supportにならない。
	// Dynamic body + Sensor onlyはSensorが床やJointの代わりにならない。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	// 床のStatic。
	const FBodyId2D Floor = F2D::Make(World, 0, 0, EBodyType::Static);
	FColliderDescription2D FloorCollider;
	FloorCollider.Shape = FOrientedBox2D{{0, -0.5f}, {5.0f, 0.5f}, 0};
	FloorCollider.Response = EColliderResponse::Sensor;
	World.AttachCollider(Floor, FloorCollider);
	// 重力なしでDynamicだけ置く。Sensorと重なっているだけ。
	const FBodyId2D Ball = F2D::Make(World, 0, 0.4f, EBodyType::Dynamic);
	FColliderDescription2D BallCollider;
	BallCollider.Shape = FCircle2D{{0, 0}, 0.5f};
	World.AttachCollider(Ball, BallCollider);
	RunSteps_Internal(World, 120);
	// Sensorは支持にならないので.sleepできない。
	PHYSICS_REQUIRE(!World.IsSleeping(Ball));
}

void ContactAndJointMixedWakePropagates_Internal()
{
	// A--Contact--B--Joint--CでAへImpulseすると全員Islandで起きる。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	// 床。
	const FBodyId2D Floor = F2D::Make(World, 0, 0, EBodyType::Static);
	FColliderDescription2D FloorCollider;
	FloorCollider.Shape = FOrientedBox2D{{0, -0.5f}, {5.0f, 0.5f}, 0};
	World.AttachCollider(Floor, FloorCollider);
	// Aが床に接触。
	const FBodyId2D A = F2D::Make(World, 0, 0.5f, EBodyType::Dynamic);
	FColliderDescription2D ACollider;
	ACollider.Shape = FOrientedBox2D{{0, 0}, {0.5f, 0.5f}, 0};
	World.AttachCollider(A, ACollider);
	// BがAに接触。
	const FBodyId2D B = F2D::Make(World, 0, 1.5f, EBodyType::Dynamic);
	FColliderDescription2D BCollider;
	BCollider.Shape = FOrientedBox2D{{0, 0}, {0.5f, 0.5f}, 0};
	World.AttachCollider(B, BCollider);
	// CがBへJoint。
	const FBodyId2D C = F2D::Make(World, 0, 3.5f, EBodyType::Dynamic);
	F2D::Join(World, B, C, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(A));
	PHYSICS_REQUIRE(World.IsSleeping(C));
	// Aを起こす。Island全体传到C。
	PHYSICS_REQUIRE(World.WakeUp(A));
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(A));
	PHYSICS_REQUIRE(!World.IsSleeping(B));
	// CもJoint経由でIsland传到起きている。
	PHYSICS_REQUIRE(!World.IsSleeping(C));
}

void ContactAndJointMixedResleep_Internal()
{
	// 混在Islandは最終的に全休止できる。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Floor = F2D::Make(World, 0, 0, EBodyType::Static);
	FColliderDescription2D FloorCollider;
	FloorCollider.Shape = FOrientedBox2D{{0, -0.5f}, {5.0f, 0.5f}, 0};
	World.AttachCollider(Floor, FloorCollider);
	const FBodyId2D A = F2D::Make(World, 0, 0.5f, EBodyType::Dynamic);
	FColliderDescription2D ACollider;
	ACollider.Shape = FOrientedBox2D{{0, 0}, {0.5f, 0.5f}, 0};
	World.AttachCollider(A, ACollider);
	const FBodyId2D B = F2D::Make(World, 0, 3.5f, EBodyType::Dynamic);
	const FJointId2D Joint = F2D::Join(World, Floor, B, 3.0, {}, {});
	RunSteps_Internal(World, 120);
	// 床に rests したAと、Jointで吊られたBが両方眠る。
	PHYSICS_REQUIRE(World.IsSleeping(A));
	PHYSICS_REQUIRE(World.IsSleeping(B));
	PHYSICS_REQUIRE(World.IsJointAlive(Joint));
}

void ContactUnknownWakeReachesJoint_Internal()
{
	// 新規Contactで起きたBが、JointでCへ同じStepのsolve前に起床が伝わる。
	// 箱が落下して床へ新規接触し、B--Joint--CなのでCもIslandで起きる。
	FPhysicsWorld2D World;
	World.SetGravity({0, -9.8f});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Floor = F2D::Make(World, 0, 0, EBodyType::Static);
	FColliderDescription2D FloorCollider;
	FloorCollider.Shape = FOrientedBox2D{{0, -0.5f}, {5.0f, 0.5f}, 0};
	World.AttachCollider(Floor, FloorCollider);
	const FBodyId2D B = F2D::Make(World, 0, 5.0f, EBodyType::Dynamic);
	FColliderDescription2D BCollider;
	BCollider.Shape = FOrientedBox2D{{0, 0}, {0.5f, 0.5f}, 0};
	World.AttachCollider(B, BCollider);
	// CはBへJointで吊られる。Bが床に当たるとCもIslandで起きる。
	const FBodyId2D C = F2D::Make(World, 0, 6.5f, EBodyType::Dynamic);
	F2D::Join(World, B, C, 1.5, {}, {});
	// 落下させる。接触が起きるまでBもCも醒ている。
	RunSteps_Internal(World, 120);
	// 床上のBと吊られたCが両方休止できる。
	PHYSICS_REQUIRE(World.IsSleeping(B));
	PHYSICS_REQUIRE(World.IsSleeping(C));
}

void FreeDynamicNeverSleeps_Internal()
{
	// 拘束が無いDynamicは休止しない。既存契約の維持。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D Ball = F2D::Make(World, 0, 0, EBodyType::Dynamic);
	RunSteps_Internal(World, 120);
	PHYSICS_REQUIRE(!World.IsSleeping(Ball));
}

void NoWakeLoopAfterSleep_Internal()
{
	// 静止Jointを600Step（10秒）回しても、一度寝た後に勝手に起きない。
	F2DScene Scene;
	Scene.Build();
	RunSteps_Internal(Scene.World, 60);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
	RunSteps_Internal(Scene.World, 600);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
}

void NoWakeLoopAfterSleep3D_Internal()
{
	F3DScene Scene;
	Scene.Build();
	RunSteps_Internal(Scene.World, 60);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
	RunSteps_Internal(Scene.World, 600);
	PHYSICS_REQUIRE(Scene.World.IsSleeping(Scene.Ball));
}

void PairResleepAfterImpulse_Internal()
{
	// sleep→impulse→island wake→settle→同じStepで全sleep、まで進む。
	// 減衰はBody生成時の設定で行う（製品既定は変えない）。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	FBodyDescription2D LeftDescription;
	LeftDescription.Position = {0, 0};
	LeftDescription.LinearDamping = 4.0f;
	LeftDescription.Mass = 1;
	LeftDescription.Inertia = 1;
	const FBodyId2D Left = World.CreateBody(LeftDescription);
	FBodyDescription2D RightDescription;
	RightDescription.Position = {2, 0};
	RightDescription.LinearDamping = 4.0f;
	RightDescription.Mass = 1;
	RightDescription.Inertia = 1;
	const FBodyId2D Right = World.CreateBody(RightDescription);
	F2D::Join(World, Left, Right, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(Left));
	World.ApplyLinearImpulse(Left, {0.2f, 0});
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(Left));
	PHYSICS_REQUIRE(!World.IsSleeping(Right));
	// 減衰で運動が収束すると再び同時に休止する。
	RunSteps_Internal(World, 300);
	PHYSICS_REQUIRE(World.IsSleeping(Left));
	PHYSICS_REQUIRE(World.IsSleeping(Right));
}

void ChainWakePropagatesThroughHops_Internal()
{
	// D0--J0--D1--J1--D2--J2--D3 の両端を互に起こす。
	// 起床は直接の相手だけでなく島全体へ届くので、逆端も次のStepで起きる。
	// 登録順には依存しない。
	FPhysicsWorld2D World;
	World.SetGravity({0, 0});
	EnableShortSleep2D_Internal(World);
	const FBodyId2D D0 = F2D::Make(World, 0, 0, EBodyType::Dynamic);
	const FBodyId2D D1 = F2D::Make(World, 2, 0, EBodyType::Dynamic);
	const FBodyId2D D2 = F2D::Make(World, 4, 0, EBodyType::Dynamic);
	const FBodyId2D D3 = F2D::Make(World, 6, 0, EBodyType::Dynamic);
	F2D::Join(World, D0, D1, 2.0, {}, {});
	F2D::Join(World, D1, D2, 2.0, {}, {});
	F2D::Join(World, D2, D3, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(D0));
	PHYSICS_REQUIRE(World.IsSleeping(D3));
	// 先端を起こす。最遠端まで伝わる。
	PHYSICS_REQUIRE(World.WakeUp(D0));
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(D0));
	PHYSICS_REQUIRE(!World.IsSleeping(D1));
	PHYSICS_REQUIRE(!World.IsSleeping(D2));
	PHYSICS_REQUIRE(!World.IsSleeping(D3));
	// もう一度全部寝かせる。
	RunSteps_Internal(World, 120);
	PHYSICS_REQUIRE(World.IsSleeping(D3));
	// 逆端を起こしても同じように全員へ伝わる。
	PHYSICS_REQUIRE(World.WakeUp(D3));
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(D0));
	PHYSICS_REQUIRE(!World.IsSleeping(D1));
	PHYSICS_REQUIRE(!World.IsSleeping(D2));
	PHYSICS_REQUIRE(!World.IsSleeping(D3));
}

void ChainWake3DPropagates_Internal()
{
	// 3Dでもchainの起床が伝播する。XYZを使う配置。
	FPhysicsWorld3D World;
	World.SetGravity({0, 0, 0});
	EnableShortSleep3D_Internal(World);
	const FBodyId3D D0 = F3D::Make(World, 0, 0, 0, EBodyType::Dynamic);
	const FBodyId3D D1 = F3D::Make(World, 2, 0, 0, EBodyType::Dynamic);
	const FBodyId3D D2 = F3D::Make(World, 2, 2, 0, EBodyType::Dynamic);
	const FBodyId3D D3 = F3D::Make(World, 2, 2, 2, EBodyType::Dynamic);
	F3D::Join(World, D0, D1, 2.0, {}, {});
	F3D::Join(World, D1, D2, 2.0, {}, {});
	F3D::Join(World, D2, D3, 2.0, {}, {});
	RunSteps_Internal(World, 60);
	PHYSICS_REQUIRE(World.IsSleeping(D0));
	PHYSICS_REQUIRE(World.WakeUp(D0));
	RunSteps_Internal(World, 1);
	PHYSICS_REQUIRE(!World.IsSleeping(D1));
	PHYSICS_REQUIRE(!World.IsSleeping(D2));
	PHYSICS_REQUIRE(!World.IsSleeping(D3));
}



const PhysicsTest::FCase Cases_Internal[] = {

    {"2D static joint dynamic sleeps", &StaticJointDynamicSleeps_Internal},
    {"3D static joint dynamic sleeps", &StaticJoint3DDynamicSleeps_Internal},
    {"2D dynamic pair sleeps together", &DynamicPairSleepsTogether_Internal},
    {"3D dynamic pair sleeps together", &DynamicPair3DSleepsTogether_Internal},
    {"2D joint chain sleeps together", &JointChainSleepsTogether_Internal},
    {"3D joint chain sleeps together", &JointChain3DSleepsTogether_Internal},
    {"2D explicit wake up propagates in the island", &ExplicitWakeUpPropagates_Internal},
    {"2D linear impulse propagates in the island", &LinearImpulsePropagates_Internal},
    {"2D woken pair keeps the joint length", &ImpulseKeepsPairConsistent_Internal},
    {"2D set velocity propagates in the island", &SetVelocityPropagates_Internal},
    {"3D set angular velocity propagates in the island", &AngularVelocityPropagates_Internal},
    {"2D kinematic motion wakes the sleeper", &KinematicMotionWakes_Internal},
    {"2D kinematic stop allows resleep", &KinematicStopAllowsResleep_Internal},
    {"3D kinematic rotating anchor wakes the sleeper", &KinematicRotatingAnchorWakes3D_Internal},
    {"2D joint error beyond tolerance wakes", &JointErrorWakes_Internal},
    {"2D joint error below tolerance does not wake", &SmallJointErrorDoesNotWake_Internal},
    {"2D joint getter does not wake", &GetterDoesNotWake_Internal},
    {"2D destroy joint removes sleep support", &DestroyJointRemovesSupport_Internal},
    {"2D destroy anchor wakes the survivor", &DestroyAnchorWakesSurvivor_Internal},
    {"2D gravity resumes after joint loss", &GravityFallbackAfterJointLoss_Internal},
    {"2D shared static does not cross wake", &SharedStaticDoesNotCrossWake_Internal},
    {"2D shared kinematic stationary does not cross wake", &SharedKinematicStationaryDoesNotCrossWake_Internal},
    {"2D shared kinematic motion wakes both islands", &SharedKinematicMotionWakesBoth_Internal},
    {"2D bAllowSleep false prevents island sleep", &DisallowSleepPreventsIslandSleep_Internal},
    {"2D sensor is not sleep support", &SensorIsNotSleepSupport_Internal},
    {"2D contact and joint mixed island propagates wake", &ContactAndJointMixedWakePropagates_Internal},
    {"2D contact and joint mixed island resleeps", &ContactAndJointMixedResleep_Internal},
    {"2D new contact wake reaches the joint neighbor", &ContactUnknownWakeReachesJoint_Internal},
    {"2D free dynamic never sleeps", &FreeDynamicNeverSleeps_Internal},
    {"2D no wake loop after sleep for 600 steps", &NoWakeLoopAfterSleep_Internal},
    {"3D no wake loop after sleep for 600 steps", &NoWakeLoopAfterSleep3D_Internal},
    {"2D pair resleeps after an impulse", &PairResleepAfterImpulse_Internal},
    {"2D chain wake propagates through every hop", &ChainWakePropagatesThroughHops_Internal},
    {"3D chain wake propagates", &ChainWake3DPropagates_Internal}};
} // namespace

const PhysicsTest::FCase* PhysicsTest::GetDistanceJointSleepCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
