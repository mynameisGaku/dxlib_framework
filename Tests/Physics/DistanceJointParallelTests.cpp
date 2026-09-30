// SPDX-License-Identifier: NOASSERTION
#include "DistanceJointTestSupport.h"
using namespace Toolbox;
using namespace Dxf;
using namespace PhysicsTest::JointTest;
namespace
{
// ワールドを新規構築し、同じ登録順の接触・拘束を各レーンで実行する。
// Modeは独立Static、共有Static、共有Kinematic、Dynamic対、混合接触、一本の鎖を選ぶ。
template <typename T>
TVector<f64> Run_Internal(FJobSystem* Jobs, int32 Mode, uint32 SubSteps = 1, bool bReverse = false, bool bSeparateSlices = false)
{
	// 試験ごとに独立して構築するWorld。
	typename T::FWorld World;
	World.SetGravity(T::At(0, -1, 0));
	// この試験で借用するJobと並列化する処理。
	FPhysicsExecutionSettings Execution;
	Execution.JobSystem = Jobs;
	World.SetExecutionSettings(Execution);
	// 共有または独立した接続先Bodyの生成条件。
	typename T::FBodyDescription AnchorDescription;
	AnchorDescription.Type = Mode == 2 || Mode == 6 ? EBodyType::Kinematic : EBodyType::Static;
	// 各島から読み取り専用で使う共有接続先。
	const auto Shared = World.CreateBody(AnchorDescription);
	if (Mode == 2)
	{
		World.SetVelocity(Shared, T::At(0.1f, 0, 0));
	}
	// 登録順に観測するDynamic BodyのID列。
	TVector<typename T::FBody> Bodies;
	// 登録順に観測するJointの世代付きID列。
	TVector<typename T::FJoint> Joints;
	for (int32 Order = 0; Order < 16; ++Order)
	{
		// 登録順または逆順の配置を選ぶ番号。
		const int32 Index = bReverse ? 15 - Order : Order;
		// 動くBodyの初期配置と運動条件。
		typename T::FBodyDescription Description;
		Description.Position = T::At(static_cast<f32>(Index * 4), 2, 0);
		Description.bAllowSleep = false;
		// この試験で観測するBody ID。
		const auto Body = World.CreateBody(Description);
		World.SetVelocity(Body, T::At(0.37f, -0.17f, 0.09f));
		Bodies.PushBack(Body);
		// 拘束の反対側の接続先ID。
		auto Anchor = Shared;
		if (Mode == 0 || Mode == 3 || Mode == 4)
		{
			AnchorDescription.Position = T::At(static_cast<f32>(Index * 4), 0, 0);
			AnchorDescription.Type = Mode == 3 ? EBodyType::Dynamic : EBodyType::Static;
			Anchor = World.CreateBody(AnchorDescription);
		}
		if (Mode == 5 && Order > 0)
		{
			Anchor = Bodies[static_cast<size_t>(Order - 1)];
		}
		// AnchorとBodyを結ぶ距離拘束の設定。
		typename T::FJointDescription Joint;
		Joint.LocalAnchorA = T::At(0.15f, 0.21f, 0);
		Joint.LocalAnchorB = T::At(-0.11f, 0.07f, 0);
		// 初期のWorld Anchor間距離を維持する。
		const auto Delta = World.GetPosition(Body) + Joint.LocalAnchorB - World.GetPosition(Anchor) - Joint.LocalAnchorA;
		Joint.Length = Sqrt(static_cast<f64>(Delta.X) * Delta.X + static_cast<f64>(Delta.Y) * Delta.Y);
		if constexpr (T::b3D)
		{
			Joint.Length = Sqrt(Joint.Length * Joint.Length + static_cast<f64>(Delta.Z) * Delta.Z);
		}
		Joints.PushBack(World.CreateDistanceJoint(Anchor, Body, Joint));
		if (Mode == 4)
		{
			World.AttachCollider(Body, T::Ball({}, 0.5f));
			World.AttachCollider(Anchor, T::Ball(T::At(0, 1.05f, 0), 0.5f));
		}
		if (Mode == 3)
		{
			Bodies.PushBack(Anchor);
		}
	}
	// 時系列のBody・Joint観測値。
	TVector<f64> Values;
	for (int32 Step = 0; Step < 40; ++Step)
	{
		if (bSeparateSlices)
		{
			// 蓄積力を使わない同じ入力を、分割ごとに独立した正常Stepでも進める。
			for (uint32 Slice = 0; Slice < SubSteps; ++Slice)
			{
				World.Step(1.0 / 60.0 / SubSteps);
			}
		}
		else
		{
			World.Step(1.0 / 60.0, SubSteps);
		}
		for (const auto Body : Bodies)
		{
			AppendBody<T>(World, Body, Values);
		}
		AppendBody<T>(World, Shared, Values);
		for (const auto Joint : Joints)
		{
			// 現在の登録とBody姿勢から取得するJointの距離と誤差。
			const auto State = World.GetDistanceJoint(Joint);
			Values.PushBack(State.CurrentLength);
			Values.PushBack(State.Error);
			Values.PushBack(World.IsJointAlive(Joint) ? 1.0 : 0.0);
		}
		// 直近Stepで構築・求解した島と実行レーンの集計。
		const auto Diagnostics = World.GetExecutionDiagnostics();
		PHYSICS_REQUIRE(Diagnostics.IslandCount == (Mode == 5 ? 1 : 16));
		if (Mode == 4 && Step == 0)
		{
			PHYSICS_REQUIRE(Diagnostics.ManifoldCount > 0);
		}
		PHYSICS_REQUIRE(Diagnostics.SolverIslandCount == (Jobs == nullptr ? 0 : Diagnostics.IslandCount * SubSteps));
		PHYSICS_REQUIRE(Diagnostics.ExecutionThreadCount == (Jobs == nullptr ? 1 : Jobs->GetExecutionThreadCount()));
	}
	return Values;
}
// 同期・1・2・4・8レーンで各フレームの観測値をbit一致で比較する。
template <typename T, int32 Mode>
void Lanes_Internal()
{
	// Jobを借用しない対照経路の観測値。
	const auto Reference = Run_Internal<T>(nullptr, Mode);
	for (const uint32 Lanes : {1u, 2u, 4u, 8u})
	{
		// この試験中にだけ所有するJob System。
		FJobSystem Jobs(Lanes);
		RequireBits(Reference, Run_Internal<T>(&Jobs, Mode));
	}
}
// 登録順を逆にした入力についても、同じ入力の同期と各レーンを比較する。
template <typename T>
void Reverse_Internal()
{
	// Jobを借用しない対照経路の観測値。
	const auto Reference = Run_Internal<T>(nullptr, 1, 1, true);
	for (const uint32 Lanes : {1u, 2u, 4u, 8u})
	{
		// この試験中にだけ所有するJob System。
		FJobSystem Jobs(Lanes);
		RequireBits(Reference, Run_Internal<T>(&Jobs, 1, 1, true));
	}
}
// 一回のStep内で作業記録が複数分割へ引き継がれる経路を比較する。
template <typename T>
void SubSteps_Internal()
{
	// Jobを借用しない対照経路の観測値。
	const auto Reference = Run_Internal<T>(nullptr, 3, 2);
	// 二分割の作業値引き継ぎと、半分の刻みを二回正常確定する対照が一致する。
	RequireBits(Reference, Run_Internal<T>(nullptr, 3, 2, false, true));
	// この試験中にだけ所有するJob System。
	FJobSystem Jobs(4);
	RequireBits(Reference, Run_Internal<T>(&Jobs, 3, 2));
}
// 共有Staticの全島を休止させ、一島への刺激だけが対応Bodyを起こす経路を各レーンで比べる。
template <typename T>
TVector<f64> SleepRun_Internal(FJobSystem* Jobs)
{
	// 試験ごとに独立して構築するWorld。
	typename T::FWorld World;
	World.SetGravity({});
	// この試験で借用するJobと並列化する処理。
	FPhysicsExecutionSettings Execution;
	Execution.JobSystem = Jobs;
	World.SetExecutionSettings(Execution);
	// 共有接続先を動かさないための生成条件。
	typename T::FBodyDescription Static;
	Static.Type = EBodyType::Static;
	// 拘束の反対側の接続先ID。
	const auto Anchor = World.CreateBody(Static);
	// 登録順に観測するDynamic BodyのID列。
	TVector<typename T::FBody> Bodies;
	// AnchorとBodyを結ぶ距離拘束の設定。
	typename T::FJointDescription Joint;
	Joint.Length = 2;
	for (int32 Index = 0; Index < 16; ++Index)
	{
		// 観測するBodyの生成条件。
		typename T::FBodyDescription Body;
		Body.Position = T::At(2, 0, 0);
		Bodies.PushBack(World.CreateBody(Body));
		World.CreateDistanceJoint(Anchor, Bodies.Back(), Joint);
	}
	for (int32 Step = 0; Step < 8; ++Step)
	{
		World.Step(0.1);
	}
	for (const auto Body : Bodies)
	{
		PHYSICS_REQUIRE(World.IsSleeping(Body));
	}
	World.ApplyLinearImpulse(Bodies[0], T::At(0, 1, 0));
	World.Step(1.0 / 60.0);
	PHYSICS_REQUIRE(!World.IsSleeping(Bodies[0]));
	// 時系列のBody・Joint観測値。
	TVector<f64> Values;
	for (size_t Index = 0; Index < Bodies.Size(); ++Index)
	{
		if (Index > 0)
		{
			PHYSICS_REQUIRE(World.IsSleeping(Bodies[Index]));
		}
		AppendBody<T>(World, Bodies[Index], Values);
	}
	return Values;
}
template <typename T>
void Sleep_Internal()
{
	// Jobを借用しない対照経路の観測値。
	const auto Reference = SleepRun_Internal<T>(nullptr);
	for (const uint32 Lanes : {1u, 2u, 4u, 8u})
	{
		// この試験中にだけ所有するJob System。
		FJobSystem Jobs(Lanes);
		RequireBits(Reference, SleepRun_Internal<T>(&Jobs));
	}
}
// 回数を固定して共有Staticの入力を繰り返す。成功するまでの再試行にはしない。
template <typename T>
void SharedRepeat_Internal()
{
	// Jobを借用しない対照経路の観測値。
	const auto Reference = Run_Internal<T>(nullptr, 1);
	// この試験中にだけ所有するJob System。
	FJobSystem Jobs(8);
	for (int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		RequireBits(Reference, Run_Internal<T>(&Jobs, 1));
	}
}
// 求解Jobだけを拒否する。Warm Startまでは実行し、保存方向の未確定書き戻しを検出する。
template <typename T>
void RejectedSolve_Internal()
{
	// 試験ごとに独立して構築するWorld。
	typename T::FWorld World;
	World.SetGravity({});
	// 共有接続先を動かさないための生成条件。
	typename T::FBodyDescription Static;
	Static.Type = EBodyType::Static;
	// 拘束の反対側の接続先ID。
	const auto Anchor = World.CreateBody(Static);
	// 登録順に観測するDynamic BodyのID列。
	TVector<typename T::FBody> Bodies;
	// 登録順に観測するJointの世代付きID列。
	TVector<typename T::FJoint> Joints;
	// AnchorとBodyを結ぶ距離拘束の設定。
	typename T::FJointDescription Joint;
	Joint.Length = 1;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		// 観測するBodyの生成条件。
		typename T::FBodyDescription Body;
		Body.Position = T::At(1, 0, 0);
		Bodies.PushBack(World.CreateBody(Body));
		Joints.PushBack(World.CreateDistanceJoint(Anchor, Bodies.Back(), Joint));
	}
	World.Step(1.0 / 60.0);
	for (const auto Body : Bodies)
	{
		T::Reset(World, Body, T::At(0, 1, 0));
		World.SetVelocity(Body, T::At(0, 3, 0));
	}
	// この試験中にだけ所有するJob System。
	FJobSystem Jobs(2);
	Jobs.Shutdown();
	// この試験で借用するJobと並列化する処理。
	FPhysicsExecutionSettings Execution;
	Execution.JobSystem = &Jobs;
	Execution.bParallelIntegration = false;
	Execution.bParallelBroadPhase = false;
	Execution.bParallelNarrowPhase = false;
	World.SetExecutionSettings(Execution);
	// 求解Jobの拒否が意図したエラーとして観測されたか。
	bool bFailed = false;
	try
	{
		World.Step(1.0 / 60.0, 2);
	}
	catch (const FException& Error)
	{
		bFailed = strstr(Error.What(), "island solver failed") != nullptr;
	}
	PHYSICS_REQUIRE(bFailed);
	PHYSICS_REQUIRE(World.GetExecutionDiagnostics().IslandCount == 2);
	// 途中失敗したWorldの採取が拒否されたか。
	bool bSnapshotRejected = false;
	try
	{
		(void)World.CaptureSnapshot();
	}
	catch (const FException&)
	{
		bSnapshotRejected = true;
	}
	PHYSICS_REQUIRE(bSnapshotRejected);
	World.SetExecutionSettings({});
	for (const auto Body : Bodies)
	{
		T::Reset(World, Body, {});
	}
	// Anchorを重ねると保存方向が観測できる。失敗StepのY軸を持ち越してはいけない。
	World.Step(1.0 / 60.0);
	for (const auto Body : Bodies)
	{
		PHYSICS_REQUIRE(World.GetPosition(Body).X > 0.1f);
		PHYSICS_REQUIRE(World.GetPosition(Body).Y == 0);
	}
	PHYSICS_REQUIRE(World.CaptureSnapshot().Bodies.Size() == 3);
	// 同じスロットの別世代へ作業値を引き継がない。
	PHYSICS_REQUIRE(World.DestroyJoint(Joints[0]));
	T::Reset(World, Bodies[0], {});
	// 同じslotへ再登録した別世代の拘束ID。
	const auto Reborn = World.CreateDistanceJoint(Anchor, Bodies[0], Joint);
	PHYSICS_REQUIRE(Reborn.Index == Joints[0].Index && Reborn.Generation != Joints[0].Generation);
	World.Step(1.0 / 60.0);
	PHYSICS_REQUIRE(World.GetPosition(Bodies[0]).X == 0 && World.GetPosition(Bodies[0]).Y == 0);
}
// 同じslotの新接続へ古いImpulseを引き継ぐと、隣の拘束を経て運動が残る。
// 一回の速度反復を使い、十分な反復によって誤ったWarm Startが隠れることを防ぐ。
template <typename T>
void ReusedWarmImpulse_Internal()
{
	// 履歴を含む実World。
	typename T::FWorld World;
	World.SetGravity({});
	// このケースだけ一反復に限定して、初期Impulseの影響を観測する。
	typename T::FContact Contact;
	Contact.VelocityIterations = 1;
	World.SetContactSettings(Contact);
	// 初回はAnchorとMiddleを重ね、第一JointのImpulseは0にする。
	typename T::FBodyDescription Description;
	Description.Type = EBodyType::Static;
	const auto Anchor = World.CreateBody(Description);
	Description.Type = EBodyType::Dynamic;
	Description.bAllowSleep = false;
	const auto Middle = World.CreateBody(Description);
	Description.Position = T::At(0, 2, 0);
	Description.Velocity = T::At(0, -2, 0);
	const auto Right = World.CreateBody(Description);
	typename T::FJointDescription Settings;
	Settings.Length = 1;
	(void)World.CreateDistanceJoint(Anchor, Middle, Settings);
	Settings.Length = 2;
	const auto Old = World.CreateDistanceJoint(Middle, Right, Settings);
	World.Step(1.0 / 60.0);
	PHYSICS_REQUIRE(World.DestroyJoint(Old));
	T::Reset(World, Middle, T::At(0, 1, 0));
	T::Reset(World, Right, T::At(0, 2, 0));
	Settings.Length = 1;
	const auto NewJoint = World.CreateDistanceJoint(Middle, Right, Settings);
	PHYSICS_REQUIRE(NewJoint.Index == Old.Index && NewJoint.Generation != Old.Generation);
	World.Step(1.0 / 60.0);
	// 両端の初速は0で長さも一致するため、新しい接続にImpulseは不要。
	PHYSICS_REQUIRE(World.GetVelocity(Middle) == T::At(0, 0, 0));
	PHYSICS_REQUIRE(World.GetVelocity(Right) == T::At(0, 0, 0));
}
// Jointで結んだ同じBody対のSolid接触も、通常の許可設定で生成される。
template <typename T>
void ConnectedContact_Internal()
{
	typename T::FWorld World;
	World.SetGravity({});
	typename T::FBodyDescription Description;
	Description.Type = EBodyType::Static;
	const auto A = World.CreateBody(Description);
	Description.Type = EBodyType::Dynamic;
	Description.Position = T::At(0, 0.75f, 0);
	const auto B = World.CreateBody(Description);
	World.AttachCollider(A, T::Ball({}, 0.5f));
	World.AttachCollider(B, T::Ball({}, 0.5f));
	typename T::FJointDescription Settings;
	Settings.Length = 0.75;
	(void)World.CreateDistanceJoint(A, B, Settings);
	World.Step(1.0 / 60.0);
	PHYSICS_REQUIRE(World.GetExecutionDiagnostics().ManifoldCount > 0);
	PHYSICS_REQUIRE(World.GetPosition(B).Y > 0.75f);
}
const PhysicsTest::FCase Cases_Internal[] = {
    {"J5 2D reused warm impulse does not reach adjacent constraint", &ReusedWarmImpulse_Internal<F2D>},
    {"J5 3D reused warm impulse does not reach adjacent constraint", &ReusedWarmImpulse_Internal<F3D>},
    {"J5 2D connected bodies still generate solid contact", &ConnectedContact_Internal<F2D>},
    {"J5 3D connected bodies still generate solid contact", &ConnectedContact_Internal<F3D>},
    {"J4 2D independent static anchors bit match 0/1/2/4/8", &Lanes_Internal<F2D, 0>},
    {"J4 3D independent static anchors bit match 0/1/2/4/8", &Lanes_Internal<F3D, 0>},
    {"J4 2D shared static anchor bit match 0/1/2/4/8", &Lanes_Internal<F2D, 1>},
    {"J4 3D shared static anchor bit match 0/1/2/4/8", &Lanes_Internal<F3D, 1>},
    {"J4 2D moving kinematic anchor bit match 0/1/2/4/8", &Lanes_Internal<F2D, 2>},
    {"J4 3D moving kinematic anchor bit match 0/1/2/4/8", &Lanes_Internal<F3D, 2>},
    {"J4 2D dynamic off-center pairs bit match 0/1/2/4/8", &Lanes_Internal<F2D, 3>},
    {"J4 3D dynamic off-center pairs bit match 0/1/2/4/8", &Lanes_Internal<F3D, 3>},
    {"J4 2D mixed contacts and joints bit match 0/1/2/4/8", &Lanes_Internal<F2D, 4>},
    {"J4 3D mixed contacts and joints bit match 0/1/2/4/8", &Lanes_Internal<F3D, 4>},
    {"J4 2D one chain remains one island bit match 0/1/2/4/8", &Lanes_Internal<F2D, 5>},
    {"J4 3D one chain remains one island bit match 0/1/2/4/8", &Lanes_Internal<F3D, 5>},
    {"J4 2D stationary kinematic anchor bit match 0/1/2/4/8", &Lanes_Internal<F2D, 6>},
    {"J4 3D stationary kinematic anchor bit match 0/1/2/4/8", &Lanes_Internal<F3D, 6>},
    {"J4 2D reversed registration stays deterministic", &Reverse_Internal<F2D>},
    {"J4 3D reversed registration stays deterministic", &Reverse_Internal<F3D>},
    {"J4 2D two substeps reuse solve state", &SubSteps_Internal<F2D>},
    {"J4 3D two substeps reuse solve state", &SubSteps_Internal<F3D>},
    {"J4 2D sleeping islands wake only the stimulated subset", &Sleep_Internal<F2D>},
    {"J4 3D sleeping islands wake only the stimulated subset", &Sleep_Internal<F3D>},
    {"J4 2D shared static matches in three fixed repetitions", &SharedRepeat_Internal<F2D>},
    {"J4 3D shared static matches in three fixed repetitions", &SharedRepeat_Internal<F3D>},
    {"J4 2D rejected island job discards cache and recovers", &RejectedSolve_Internal<F2D>},
    {"J4 3D rejected island job discards cache and recovers", &RejectedSolve_Internal<F3D>}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetDistanceJointParallelCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
