// SPDX-License-Identifier: NOASSERTION
#include "DistanceJointTestSupport.h"
#include "../../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
using namespace Toolbox;
using namespace Dxf;
using namespace PhysicsTest::JointTest;
namespace
{
// 独立した取付Frameを生成して各種類を登録する。COM外の腕を検算へ含める。
template <typename T, EJointKind Kind>
typename T::FJoint Join_Internal(typename T::FWorld& World, typename T::FBody A, typename T::FBody B)
{
	const auto Anchor = T::At(0.13f, 0.22f, 0.17f);
	if constexpr (Kind == EJointKind::Revolute)
	{
		auto Description = World.MakeRevoluteJointDescription(A, B, Anchor);
		Description.Drive = {true, 0.7, 2};
		return World.CreateRevoluteJoint(A, B, Description);
	}
	else if constexpr (Kind == EJointKind::Fixed)
	{
		return World.CreateFixedJoint(A, B, World.MakeFixedJointDescription(A, B, Anchor));
	}
	else
	{
		auto Description = World.MakePrismaticJointDescription(A, B, Anchor);
		Description.Drive = {true, 0.7, 2};
		return World.CreatePrismaticJoint(A, B, Description);
	}
}
// Private cacheを公開せず、回復後の物理状態を対照と比較する。
template <typename T, EJointKind Kind>
f64 Coordinate_Internal(const typename T::FWorld& World, typename T::FJoint Id)
{
	if constexpr (Kind == EJointKind::Revolute)
	{
		return World.GetRevoluteJoint(Id).Angle;
	}
	else if constexpr (Kind == EJointKind::Fixed)
	{
		return World.GetFixedJoint(Id).OrientationError;
	}
	else
	{
		return World.GetPrismaticJoint(Id).Translation;
	}
}
// 登録の各確保を一回ずつ失敗させ、部分登録とslot漏れを調べる。
template <typename T, EJointKind Kind>
int32 Registration_Internal()
{
	int32 Failures = 0;
	int32 Injected = 0;
	for (int64 Countdown = 0; Countdown < 32; ++Countdown)
	{
		typename T::FWorld World;
		typename T::FBodyDescription Static;
		Static.Type = EBodyType::Static;
		const auto A = World.CreateBody(Static);
		const auto B = World.CreateBody({});
		bool bThrew = false;
		Testing::SetAllocationFailureCountdown(Countdown);
		try
		{
			(void)Join_Internal<T, Kind>(World, A, B);
		}
		catch (const FException&)
		{
			bThrew = true;
		}
		const bool bInjected = Testing::WasAllocationFailureInjected();
		Testing::SetAllocationFailureCountdown(-1);
		printf("K O01 %s kind=%d create countdown=%lld injected=%d threw=%d\n", T::Name, static_cast<int32>(Kind), Countdown, bInjected, bThrew);
		if (!bInjected)
		{
			break;
		}
		++Injected;
		if (!bThrew || !World.IsAlive(A) || !World.IsAlive(B))
		{
			++Failures;
		}
		const auto Retry = Join_Internal<T, Kind>(World, A, B);
		if (Retry.Index != 0 || !World.IsJointAlive(Retry))
		{
			++Failures;
		}
	}
	return Failures + (Injected == 0 ? 1 : 0);
}
// 保有数を増やしてもJoint／Body／Colliderの終了経路は確保しない。
template <typename T, EJointKind Kind>
int32 Release_Internal()
{
	typename T::FWorld World;
	typename T::FBodyDescription Static;
	Static.Type = EBodyType::Static;
	const auto A = World.CreateBody(Static);
	TArray<typename T::FBody, 33> Bodies;
	TArray<typename T::FJoint, 33> Joints;
	for (size_t I = 0; I < 33; ++I)
	{
		Bodies[I] = World.CreateBody({});
		World.AttachCollider(Bodies[I], T::Ball({}, 0.25f));
		Joints[I] = Join_Internal<T, Kind>(World, A, Bodies[I]);
	}
	const auto Before = Testing::GetTotalTestAllocations();
	Testing::SetAllocationFailureCountdown(0);
	bool bOk = true;
	for (size_t I = 0; I < 33; ++I)
	{
		bOk = World.DestroyBody(Bodies[I]) && !World.IsJointAlive(Joints[I]) && bOk;
	}
	const bool bInjected = Testing::WasAllocationFailureInjected();
	Testing::SetAllocationFailureCountdown(-1);
	const auto After = Testing::GetTotalTestAllocations();
	printf("K O01 %s kind=%d release allocations=%llu injected=%d\n", T::Name, static_cast<int32>(Kind), After - Before, bInjected);
	return bOk && !bInjected && Before == After ? 0 : 1;
}
template <typename T, EJointKind Kind>
int32 MechanismCacheFailure_Internal()
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
				Joints[Index] = Join_Internal<T, Kind>(World, Anchor, Bodies[Index]);
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
			printf("K T01 %s kind=%d first-uninjected=%lld exit=0\n", T::Name, static_cast<int32>(Kind), Countdown);
			break;
		}
		printf("K T01 %s kind=%d injected=%lld threw=%d solved=%llu\n", T::Name, static_cast<int32>(Kind), Countdown, bThrew, FailedWorld.GetExecutionDiagnostics().SolverIslandCount);
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
			FailedValues.PushBack(Coordinate_Internal<T, Kind>(FailedWorld, FailedJoints[Index]));
			ControlValues.PushBack(Coordinate_Internal<T, Kind>(ControlWorld, ControlJoints[Index]));
		}
		try
		{
			RequireBits(FailedValues, ControlValues);
		}
		catch (const FException&)
		{
			++Failures;
			printf("FAIL K T01 %s kind=%d allocation countdown=%lld cache recovery differs\n", T::Name, static_cast<int32>(Kind), Countdown);
		}
	}
	if (Injected == 0 || AfterSolve == 0)
	{
		++Failures;
	}
	printf("K T01 %s kind=%d allocation trials=%d after-solve=%d failures=%d\n", T::Name, static_cast<int32>(Kind), Injected, AfterSolve, Failures);
	return Failures;
}
} // namespace
Toolbox::int32 PhysicsTest::RunMechanismJointAllocationChecks()
{
	int32 Failures = 0;
	Failures += Registration_Internal<F2D, EJointKind::Revolute>();
	Failures += Release_Internal<F2D, EJointKind::Revolute>();
	Failures += MechanismCacheFailure_Internal<F2D, EJointKind::Revolute>();
	Failures += Registration_Internal<F2D, EJointKind::Fixed>();
	Failures += Release_Internal<F2D, EJointKind::Fixed>();
	Failures += MechanismCacheFailure_Internal<F2D, EJointKind::Fixed>();
	Failures += Registration_Internal<F2D, EJointKind::Prismatic>();
	Failures += Release_Internal<F2D, EJointKind::Prismatic>();
	Failures += MechanismCacheFailure_Internal<F2D, EJointKind::Prismatic>();
	Failures += Registration_Internal<F3D, EJointKind::Revolute>();
	Failures += Release_Internal<F3D, EJointKind::Revolute>();
	Failures += MechanismCacheFailure_Internal<F3D, EJointKind::Revolute>();
	Failures += Registration_Internal<F3D, EJointKind::Fixed>();
	Failures += Release_Internal<F3D, EJointKind::Fixed>();
	Failures += MechanismCacheFailure_Internal<F3D, EJointKind::Fixed>();
	Failures += Registration_Internal<F3D, EJointKind::Prismatic>();
	Failures += Release_Internal<F3D, EJointKind::Prismatic>();
	Failures += MechanismCacheFailure_Internal<F3D, EJointKind::Prismatic>();
	return Failures;
}
