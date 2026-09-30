// SPDX-License-Identifier: NOASSERTION
#include "DistanceJointTestSupport.h"
#include "../../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
using namespace Toolbox;
using namespace Dxf;
using namespace PhysicsTest::JointTest;
namespace
{
// 追加登録でStep作業領域を増やす最初の確保を失敗させる。Bodyの変更より前に止まる。
template <typename T>
int32 ScratchGrowth_Internal()
{
	// 試験ごとに独立して構築するWorld。
	typename T::FWorld World;
	World.SetGravity({});
	// 共有接続先を動かさないための生成条件。
	typename T::FBodyDescription Static;
	Static.Type = EBodyType::Static;
	// 拘束の反対側の接続先ID。
	const auto Anchor = World.CreateBody(Static);
	// 作業領域拡張前のBodyの初期条件。
	typename T::FBodyDescription BodyDescription;
	BodyDescription.Position = T::At(1, 0, 0);
	// この試験で観測するBody ID。
	const auto Body = World.CreateBody(BodyDescription);
	// 作業領域へ複製する距離拘束の登録条件。
	typename T::FJointDescription Description;
	Description.Length = 1;
	World.CreateDistanceJoint(Anchor, Body, Description);
	World.Step(1.0 / 60.0);
	// 登録領域の確保はここで済ませ、次のStepでは作業領域の拡張を先に行う。
	for (int32 Index = 0; Index < 32; ++Index)
	{
		World.CreateDistanceJoint(Anchor, Body, Description);
	}
	World.SetVelocity(Body, T::At(0.31f, 0.71f, 0.17f));
	// 確保失敗前のBody観測値。
	TVector<f64> Before;
	AppendBody<T>(World, Body, Before);
	// 注入した確保失敗がStepから伝播したか。
	bool bThrew = false;
	Testing::SetAllocationFailureCountdown(0);
	try
	{
		World.Step(1.0 / 60.0);
	}
	catch (const FException&)
	{
		bThrew = true;
	}
	// 指定した確保失敗が実際に起きたか。
	const bool bInjected = Testing::WasAllocationFailureInjected();
	Testing::SetAllocationFailureCountdown(-1);
	// 確保失敗直後のBody観測値。
	TVector<f64> After;
	AppendBody<T>(World, Body, After);
	// この確保地点の検査失敗数。
	int32 Failed = bThrew && bInjected ? 0 : 1;
	try
	{
		RequireBits(Before, After);
	}
	catch (const FException&)
	{
		++Failed;
	}
	World.Step(1.0 / 60.0);
	printf("J4 %s scratch growth failure unchanged body failures=%d\n", T::Name, Failed);
	return Failed;
}
// 各確保地点を一回ずつ失敗させる。Bodyの巻き戻しを公開操作で行い、Joint記録だけを独立比較する。
template <typename T>
int32 CacheFailure_Internal()
{
	// 対照値との不一致または未到達の数。
	int32 Failures = 0;
	// 実際に故障注入へ到達した確保地点数。
	int32 Injected = 0;
	// 島の求解完了後に失敗した試行数。
	int32 AfterSolve = 0;
	for (int64 Countdown = 0; Countdown < 80; ++Countdown)
	{
		// 確保失敗を注入するWorld。
		typename T::FWorld FailedWorld;
		// 同じ正常履歴を持ち、失敗を注入しない対照World。
		typename T::FWorld ControlWorld;
		// 失敗対象WorldのBody ID。
		typename T::FBody FailedBodies[2];
		// 対照WorldのBody ID。
		typename T::FBody ControlBodies[2];
		// 失敗対象WorldのJoint ID。
		typename T::FJoint FailedJoints[2];
		// 対照WorldのJoint ID。
		typename T::FJoint ControlJoints[2];
		// 反復を一回に限って再利用値が計算過程へ及ぼす影響を残す。
		typename T::FContact Contact;
		Contact.VelocityIterations = 1;
		for (int32 Which = 0; Which < 2; ++Which)
		{
			// 試験ごとに独立して構築するWorld。
			auto& World = Which == 0 ? FailedWorld : ControlWorld;
			// 登録順に観測するDynamic BodyのID列。
			auto* Bodies = Which == 0 ? FailedBodies : ControlBodies;
			// 登録順に観測するJointの世代付きID列。
			auto* Joints = Which == 0 ? FailedJoints : ControlJoints;
			World.SetGravity({});
			World.SetContactSettings(Contact);
			// 共有接続先を動かさないための生成条件。
			typename T::FBodyDescription Static;
			Static.Type = EBodyType::Static;
			// 拘束の反対側の接続先ID。
			const auto Anchor = World.CreateBody(Static);
			for (int32 Index = 0; Index < 2; ++Index)
			{
				// 観測するBodyの生成条件。
				typename T::FBodyDescription Body;
				Body.Position = T::At(static_cast<f32>(Index * 3 + 2), 1, 0);
				Body.Velocity = T::At(1.31f, -0.71f, 0.17f);
				Body.bAllowSleep = false;
				Bodies[Index] = World.CreateBody(Body);
				// AnchorとBodyを結ぶ距離拘束の設定。
				typename T::FJointDescription Joint;
				Joint.Length = 2;
				Joint.LocalAnchorA = T::At(0.13f, 0.22f, 0);
				Joint.LocalAnchorB = T::At(-0.31f, 0.41f, 0.17f);
				Joints[Index] = World.CreateDistanceJoint(Anchor, Bodies[Index], Joint);
			}
			World.Step(1.0 / 60.0);
			// 成功した速度拘束で非ゼロのImpulseを保存する履歴を両Worldへ作る。
			PHYSICS_REQUIRE(World.GetVelocity(Bodies[0]) != T::At(1.31f, -0.71f, 0.17f));
			for (int32 Index = 0; Index < 2; ++Index)
			{
				T::Reset(World, Bodies[Index], T::At(static_cast<f32>(Index * 3 + 2), 1, 0));
				World.SetVelocity(Bodies[Index], T::At(13.17f, -7.31f, 1.77f));
			}
		}
		// この試験中にだけ所有するJob System。
		FJobSystem Jobs(1);
		// この試験で借用するJobと並列化する処理。
		FPhysicsExecutionSettings Execution;
		Execution.JobSystem = &Jobs;
		Execution.bParallelBroadPhase = false;
		Execution.bParallelNarrowPhase = false;
		FailedWorld.SetExecutionSettings(Execution);
		// 注入した確保失敗がStepから伝播したか。
		bool bThrew = false;
		Testing::SetAllocationFailureCountdown(Countdown);
		try
		{
			FailedWorld.Step(1.0 / 60.0, 2);
		}
		catch (const FException&)
		{
			bThrew = true;
		}
		// 指定した確保失敗が実際に起きたか。
		const bool bInjected = Testing::WasAllocationFailureInjected();
		Testing::SetAllocationFailureCountdown(-1);
		if (!bInjected)
		{
			break;
		}
		++Injected;
		if (!bThrew)
		{
			++Failures;
			continue;
		}
		// 一部または全島が速度求解を終えた失敗も必ず含む。
		if (FailedWorld.GetExecutionDiagnostics().SolverIslandCount > 0)
		{
			++AfterSolve;
		}
		FailedWorld.SetExecutionSettings({});
		for (int32 Index = 0; Index < 2; ++Index)
		{
			T::Reset(FailedWorld, FailedBodies[Index], T::At(static_cast<f32>(Index * 3 + 2), 1, 0));
			FailedWorld.SetVelocity(FailedBodies[Index], T::At(13.17f, -7.31f, 1.77f));
		}
		FailedWorld.Step(1.0 / 60.0, 2);
		ControlWorld.Step(1.0 / 60.0, 2);
		// 回復した失敗対象のBody・Joint観測値。
		TVector<f64> FailedValues;
		// 同じ入力から進めた対照のBody・Joint観測値。
		TVector<f64> ControlValues;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			AppendBody<T>(FailedWorld, FailedBodies[Index], FailedValues);
			AppendBody<T>(ControlWorld, ControlBodies[Index], ControlValues);
			FailedValues.PushBack(FailedWorld.GetDistanceJoint(FailedJoints[Index]).CurrentLength);
			ControlValues.PushBack(ControlWorld.GetDistanceJoint(ControlJoints[Index]).CurrentLength);
		}
		try
		{
			RequireBits(FailedValues, ControlValues);
		}
		catch (const FException&)
		{
			++Failures;
			printf("FAIL J4 %s allocation countdown=%lld cache recovery differs\n", T::Name, Countdown);
		}
	}
	if (Injected == 0 || AfterSolve == 0)
	{
		++Failures;
	}
	printf("J4 %s allocation trials=%d after-solve=%d failures=%d\n", T::Name, Injected, AfterSolve, Failures);
	return Failures;
}
// 公開noexcept破棄は、全slotを破棄しても確保しない。世代も別登録へ進む。
template <typename T>
int32 ReleaseWithoutAllocation_Internal()
{
	// 破棄中の追加確保を検査するWorld。
	typename T::FWorld World;
	// 動かない接続先の生成条件。
	typename T::FBodyDescription Anchor;
	Anchor.Type = EBodyType::Static;
	// 支点の登録ID。
	const auto A = World.CreateBody(Anchor);
	// 動く接続先の生成条件。
	typename T::FBodyDescription Weight;
	Weight.Position = T::At(1, 0, 0);
	// 荷物の登録ID。
	const auto B = World.CreateBody(Weight);
	// 同じ長さのJoint生成条件。
	typename T::FJointDescription Description;
	Description.Length = 1;
	// 空き番号領域の容量を跨ぐ33登録。
	Toolbox::TArray<typename T::FJoint, 33> Joints;
	for (auto& Joint : Joints)
	{
		Joint = World.CreateDistanceJoint(A, B, Description);
	}
	// 破棄直前の累計確保数。
	const auto Before = Testing::GetTotalTestAllocations();
	Testing::SetAllocationFailureCountdown(0);
	// すべてのJointを破棄できたか。
	bool bDestroyed = true;
	for (const auto Joint : Joints)
	{
		bDestroyed = World.DestroyJoint(Joint) && bDestroyed;
	}
	// 禁止した追加確保に到達したか。
	const bool bInjected = Testing::WasAllocationFailureInjected();
	Testing::SetAllocationFailureCountdown(-1);
	// 破棄後の累計確保数。
	const auto After = Testing::GetTotalTestAllocations();
	// 同slotの新世代の登録。
	const auto New = World.CreateDistanceJoint(A, B, Description);
	// 確保と世代の全条件の合否。
	const bool bOk = bDestroyed && !bInjected && Before == After && New.Index == Joints[32].Index && New.Generation != Joints[32].Generation;
	printf("J5 %s destroy 33 joints allocation-free=%d injected=%d\n", T::Name, bOk, bInjected);
	return bOk ? 0 : 1;
}
} // namespace
Toolbox::int32 PhysicsTest::RunJointAllocationChecks()
{
	return ReleaseWithoutAllocation_Internal<F2D>() + ReleaseWithoutAllocation_Internal<F3D>() + ScratchGrowth_Internal<F2D>() + ScratchGrowth_Internal<F3D>() + CacheFailure_Internal<F2D>() + CacheFailure_Internal<F3D>();
}
